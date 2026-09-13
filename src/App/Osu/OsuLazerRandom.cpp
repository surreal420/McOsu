//================ Copyright (c) 2026, PG & surreal420, All rights reserved. =================//
//
// Purpose:		osu!lazer "Random" mod: reposition hitobjects while keeping jump distances, randomizing angles
//
// $NoKeywords: $osulazerrandom
//===============================================================================//

// Port of:
//   osu.Game.Rulesets.Osu/Mods/OsuModRandom.cs
//   osu.Game.Rulesets.Osu/Utils/OsuHitObjectGenerationUtils.cs
//   osu.Game.Rulesets.Osu/Utils/OsuHitObjectGenerationUtils.Reposition.cs
// (ppy/osu, MIT licence). Structure and naming follow the original so the two can be compared side by side.

#include "OsuLazerRandom.h"

#include <algorithm>
#include <cmath>
#include <random>

const float OsuLazerRandom::ANGLE_SHARPNESS_MIN = 1.0f;
const float OsuLazerRandom::ANGLE_SHARPNESS_MAX = 10.0f;
const float OsuLazerRandom::ANGLE_SHARPNESS_DEFAULT = 7.0f;

namespace
{
	const float PLAYFIELD_WIDTH = 512.0f;
	const float PLAYFIELD_HEIGHT = 384.0f;
	const float PLAYFIELD_DIAGONAL = 640.0f; // sqrt(512^2 + 384^2)
	const Vector2 PLAYFIELD_CENTRE = Vector2(PLAYFIELD_WIDTH / 2.0f, PLAYFIELD_HEIGHT / 2.0f);

	// the relative distance to the edge of the playfield before objects' positions should start to "turn around" and curve towards the middle
	const float PLAYFIELD_EDGE_RATIO = 0.375f;
	const float BORDER_DISTANCE_X = PLAYFIELD_WIDTH * PLAYFIELD_EDGE_RATIO;
	const float BORDER_DISTANCE_Y = PLAYFIELD_HEIGHT * PLAYFIELD_EDGE_RATIO;

	// number of previous hitobjects to be shifted together when an object is being moved
	const int PRECEDING_HITOBJECTS_TO_SHIFT = 10;

	const float LAZER_PI = 3.14159265358979f;

	class RNG
	{
	public:
		RNG(int seed) : m_gen((unsigned int)seed), m_dist(0.0, 1.0) {;}

		// [0, 1)
		double nextDouble() {return m_dist(m_gen);}

	private:
		std::mt19937 m_gen;
		std::uniform_real_distribution<double> m_dist;
	};

	struct POSITION_INFO
	{
		float relativeAngle;			// jump angle from the previous object, relative to the previous object's jump angle
		float distanceFromPrevious;		// jump distance from the previous object
		float rotation;					// SLIDER: angle from head to end of curve, relative to the jump angle
	};

	struct WORKING_OBJECT
	{
		OsuLazerRandom::OBJECT *obj;
		POSITION_INFO info;
		float rotationOriginal;
		Vector2 positionModified;
		Vector2 endPositionModified;
	};

	inline float clampf(float v, float lo, float hi) {return (v < lo ? lo : (v > hi ? hi : v));}

	float randomGaussian(RNG &rng, float mean, float stdDev)
	{
		// Box-Muller, both inputs in (0, 1] so that log() is defined
		const double x1 = 1.0 - rng.nextDouble();
		const double x2 = 1.0 - rng.nextDouble();
		const double stdNormal = std::sqrt(-2.0 * std::log(x1)) * std::sin(2.0 * 3.14159265358979 * x2);
		return mean + stdDev * (float)stdNormal;
	}

	Vector2 rotateVector(const Vector2 &vector, float rotation)
	{
		const float angle = std::atan2(vector.y, vector.x) + rotation;
		const float length = vector.length();
		return Vector2(length * std::cos(angle), length * std::sin(angle));
	}

	// rotates "initial" towards "destination", rotationRatio 0 = no rotation, 1 = fully rotated onto destination's direction
	Vector2 rotateVectorTowardsVector(const Vector2 &initial, const Vector2 &destination, float rotationRatio)
	{
		const float initialAngleRad = std::atan2(initial.y, initial.x);
		const float destAngleRad = std::atan2(destination.y, destination.x);

		float diff = destAngleRad - initialAngleRad;
		while (diff < -LAZER_PI) diff += 2.0f * LAZER_PI;
		while (diff > LAZER_PI) diff -= 2.0f * LAZER_PI;

		const float finalAngleRad = initialAngleRad + rotationRatio * diff;
		const float length = initial.length();

		return Vector2(length * std::cos(finalAngleRad), length * std::sin(finalAngleRad));
	}

	// rotates a hitobject away from the playfield edge while keeping a constant distance from the previous object (the closer to the edge, the stronger)
	Vector2 rotateAwayFromEdge(const Vector2 &prevObjectPos, const Vector2 &posRelativeToPrev, float rotationRatio = 0.5f)
	{
		float relativeRotationDistance = 0.0f;

		if (prevObjectPos.x < PLAYFIELD_CENTRE.x)
			relativeRotationDistance = std::max((BORDER_DISTANCE_X - prevObjectPos.x) / BORDER_DISTANCE_X, relativeRotationDistance);
		else
			relativeRotationDistance = std::max((prevObjectPos.x - (PLAYFIELD_WIDTH - BORDER_DISTANCE_X)) / BORDER_DISTANCE_X, relativeRotationDistance);

		if (prevObjectPos.y < PLAYFIELD_CENTRE.y)
			relativeRotationDistance = std::max((BORDER_DISTANCE_Y - prevObjectPos.y) / BORDER_DISTANCE_Y, relativeRotationDistance);
		else
			relativeRotationDistance = std::max((prevObjectPos.y - (PLAYFIELD_HEIGHT - BORDER_DISTANCE_Y)) / BORDER_DISTANCE_Y, relativeRotationDistance);

		return rotateVectorTowardsVector(posRelativeToPrev, PLAYFIELD_CENTRE - prevObjectPos, std::min(1.0f, relativeRotationDistance * rotationRatio));
	}

	Vector2 clampToPlayfieldWithPadding(const Vector2 &position, float padding)
	{
		return Vector2(clampf(position.x, padding, PLAYFIELD_WIDTH - padding), clampf(position.y, padding, PLAYFIELD_HEIGHT - padding));
	}

	// absolute difference between two angles, in [0, pi)
	float getAngleDifference(float angle1, float angle2)
	{
		const float diff = std::fmod(std::fabs(angle1 - angle2), LAZER_PI * 2.0f);
		return std::min(diff, LAZER_PI * 2.0f - diff);
	}

	//**************//
	//	  Sliders	//
	//**************//

	// angle from the head to the end of the curve
	float getSliderRotation(const std::vector<Vector2> &path)
	{
		if (path.size() < 1) return 0.0f;

		const Vector2 &end = path[path.size() - 1];
		return std::atan2(end.y, end.x);
	}

	Vector2 getSliderEndOffset(const OsuLazerRandom::OBJECT &obj)
	{
		if (obj.type != OsuLazerRandom::TYPE::SLIDER || obj.endsAtHead || obj.path.size() < 1) return Vector2(0, 0);

		return obj.path[obj.path.size() - 1];
	}

	Vector2 getEndPosition(const OsuLazerRandom::OBJECT &obj)
	{
		return obj.pos + getSliderEndOffset(obj);
	}

	void rotateSlider(OsuLazerRandom::OBJECT &obj, float rotation)
	{
		for (size_t i=0; i<obj.path.size(); i++)
		{
			obj.path[i] = rotateVector(obj.path[i], rotation);
		}
		obj.rotation += rotation;
	}

	void flipSliderInPlaceHorizontally(OsuLazerRandom::OBJECT &obj)
	{
		for (size_t i=0; i<obj.path.size(); i++)
		{
			obj.path[i].x = -obj.path[i].x;
		}
		obj.flipped = !obj.flipped;
	}

	// estimate of the centre of mass of a slider relative to its head (sampled every 50 osu!pixels along the curve)
	Vector2 calculateCentreOfMass(const std::vector<Vector2> &path)
	{
		const float sampleStep = 50.0f;

		if (path.size() < 1) return Vector2(0, 0);

		float pathDistance = 0.0f;
		for (size_t i=1; i<path.size(); i++)
		{
			pathDistance += (path[i] - path[i-1]).length();
		}

		// just sample the start and end positions if the slider is too short
		if (pathDistance <= sampleStep)
			return path[path.size() - 1] / 2.0f;

		int count = 0;
		Vector2 sum;
		size_t segment = 1;
		float segmentStart = 0.0f;
		for (float d=0.0f; d<pathDistance; d+=sampleStep)
		{
			// advance to the segment containing distance d
			while (segment < path.size() - 1 && segmentStart + (path[segment] - path[segment-1]).length() < d)
			{
				segmentStart += (path[segment] - path[segment-1]).length();
				segment++;
			}

			const float segmentLength = (path[segment] - path[segment-1]).length();
			const float t = (segmentLength > 0.0f ? clampf((d - segmentStart) / segmentLength, 0.0f, 1.0f) : 0.0f);

			sum += path[segment-1] + (path[segment] - path[segment-1]) * t;
			count++;
		}

		return sum / (float)count;
	}

	struct MOVEMENT_BOUNDS
	{
		float left;
		float top;
		float right;
		float bottom;

		float width() const {return right - left;}
		float height() const {return bottom - top;}
	};

	// rectangle of all head positions for which the entire slider (incl. circle radius) stays inside the playfield (negative width/height = impossible)
	MOVEMENT_BOUNDS calculatePossibleMovementBounds(const std::vector<Vector2> &path, float radius)
	{
		float minX = 0.0f;
		float maxX = 0.0f;
		float minY = 0.0f;
		float maxY = 0.0f;

		for (size_t i=0; i<path.size(); i++)
		{
			minX = std::min(minX, path[i].x);
			maxX = std::max(maxX, path[i].x);
			minY = std::min(minY, path[i].y);
			maxY = std::max(maxY, path[i].y);
		}

		minX -= radius;
		minY -= radius;
		maxX += radius;
		maxY += radius;

		MOVEMENT_BOUNDS b;
		b.left = -minX;
		b.right = PLAYFIELD_WIDTH - maxX;
		b.top = -minY;
		b.bottom = PLAYFIELD_HEIGHT - maxY;
		return b;
	}

	//**********************//
	//	  OsuModRandom.cs	//
	//**********************//

	float getRandomOffset(RNG &rng, float stdDev, float angleSharpness)
	{
		// range [0.5, 2], higher angle sharpness -> lower multiplier
		const float customMultiplier = (1.5f * OsuLazerRandom::ANGLE_SHARPNESS_MAX - angleSharpness) / (1.5f * OsuLazerRandom::ANGLE_SHARPNESS_MAX - OsuLazerRandom::ANGLE_SHARPNESS_DEFAULT);

		return randomGaussian(rng, 0.0f, stdDev * customMultiplier);
	}

	// targetDistance: distance between the previous and the current object, offset: angle (rad) to offset the target angle by, flowDirection: sign of the relative angle
	float getRelativeTargetAngle(float targetDistance, float offset, bool flowDirection, float angleSharpnessSetting)
	{
		// range [0.1, 1]
		const float angleSharpness = angleSharpnessSetting / OsuLazerRandom::ANGLE_SHARPNESS_MAX;
		// range [0, 0.9]
		const float angleWideness = 1.0f - angleSharpness;

		// range [-60, 30]
		const float customOffsetX = angleSharpness * 100.0f - 70.0f;
		// range [-0.075, 0.15]
		const float customOffsetY = angleWideness * 0.25f - 0.075f;

		targetDistance += customOffsetX;
		float angle = (float)(2.16 / (1.0 + 200.0 * std::exp(0.036 * (targetDistance - 310.0f + customOffsetX))) + 0.5);
		angle += offset + customOffsetY;

		const float relativeAngle = LAZER_PI - angle;

		return (flowDirection ? -relativeAngle : relativeAngle);
	}

	bool previousObjectStartedCombo(const std::vector<OsuLazerRandom::OBJECT> &objects, size_t i)
	{
		// exclude new-combo-spam and 1-2-combos
		return (objects[(i >= 2 ? i - 2 : 0)].comboIndex > 1 && objects[i - 1].newCombo);
	}

	bool shouldStartNewSection(const std::vector<OsuLazerRandom::OBJECT> &objects, size_t i, RNG &rng)
	{
		if (i == 0) return true;

		const bool startedCombo = previousObjectStartedCombo(objects, i);
		const bool previousObjectWasOnDownbeat = objects[i - 1].onDownbeat;
		const bool previousObjectWasOnBeat = objects[i - 1].onBeat;

		return (startedCombo && rng.nextDouble() < 0.6) ||
			   previousObjectWasOnDownbeat ||
			   (previousObjectWasOnBeat && rng.nextDouble() < 0.4);
	}

	bool shouldApplyFlowChange(const std::vector<OsuLazerRandom::OBJECT> &objects, size_t i, RNG &rng)
	{
		return (previousObjectStartedCombo(objects, i) && rng.nextDouble() < 0.6);
	}

	//**********************************************//
	//	  OsuHitObjectGenerationUtils.Reposition.cs	//
	//**********************************************//

	std::vector<POSITION_INFO> generatePositionInfos(const std::vector<OsuLazerRandom::OBJECT> &objects)
	{
		std::vector<POSITION_INFO> positionInfos;
		positionInfos.reserve(objects.size());

		Vector2 previousPosition = PLAYFIELD_CENTRE;
		float previousAngle = 0.0f;

		for (size_t i=0; i<objects.size(); i++)
		{
			const OsuLazerRandom::OBJECT &obj = objects[i];

			const Vector2 relativePosition = obj.pos - previousPosition;
			float absoluteAngle = std::atan2(relativePosition.y, relativePosition.x);

			POSITION_INFO info;
			info.relativeAngle = absoluteAngle - previousAngle;
			info.distanceFromPrevious = relativePosition.length();
			info.rotation = 0.0f;

			if (obj.type == OsuLazerRandom::TYPE::SLIDER)
			{
				const float absoluteRotation = getSliderRotation(obj.path);
				info.rotation = absoluteRotation - absoluteAngle;
				absoluteAngle = absoluteRotation;
			}

			positionInfos.push_back(info);

			previousPosition = getEndPosition(obj);
			previousAngle = absoluteAngle;
		}

		return positionInfos;
	}

	void computeModifiedPosition(WORKING_OBJECT &current, const WORKING_OBJECT *previous, const WORKING_OBJECT *beforePrevious)
	{
		float previousAbsoluteAngle = 0.0f;

		if (previous != NULL)
		{
			if (previous->obj->type == OsuLazerRandom::TYPE::SLIDER)
				previousAbsoluteAngle = getSliderRotation(previous->obj->path);
			else
			{
				const Vector2 earliestPosition = (beforePrevious != NULL ? getEndPosition(*beforePrevious->obj) : PLAYFIELD_CENTRE);
				const Vector2 relativePosition = previous->obj->pos - earliestPosition;
				previousAbsoluteAngle = std::atan2(relativePosition.y, relativePosition.x);
			}
		}

		float absoluteAngle = previousAbsoluteAngle + current.info.relativeAngle;

		Vector2 posRelativeToPrev = Vector2(current.info.distanceFromPrevious * std::cos(absoluteAngle), current.info.distanceFromPrevious * std::sin(absoluteAngle));

		const Vector2 lastEndPosition = (previous != NULL ? previous->endPositionModified : PLAYFIELD_CENTRE);

		posRelativeToPrev = rotateAwayFromEdge(lastEndPosition, posRelativeToPrev);

		current.positionModified = lastEndPosition + posRelativeToPrev;

		if (current.obj->type != OsuLazerRandom::TYPE::SLIDER) return;

		absoluteAngle = std::atan2(posRelativeToPrev.y, posRelativeToPrev.x);

		const Vector2 centreOfMassOriginal = calculateCentreOfMass(current.obj->path);
		Vector2 centreOfMassModified = rotateVector(centreOfMassOriginal, current.info.rotation + absoluteAngle - getSliderRotation(current.obj->path));
		centreOfMassModified = rotateAwayFromEdge(current.positionModified, centreOfMassModified);

		const float relativeRotation = std::atan2(centreOfMassModified.y, centreOfMassModified.x) - std::atan2(centreOfMassOriginal.y, centreOfMassOriginal.x);
		if (std::fabs(relativeRotation) > 0.001f) // Precision.AlmostEquals()
			rotateSlider(*current.obj, relativeRotation);
	}

	// moves the circle inside the playfield, returns the deviation from the wanted position
	Vector2 clampHitCircleToPlayfield(WORKING_OBJECT &workingObject, float radius)
	{
		const Vector2 previousPosition = workingObject.positionModified;

		workingObject.endPositionModified = workingObject.positionModified = clampToPlayfieldWithPadding(workingObject.positionModified, radius);
		workingObject.obj->pos = workingObject.positionModified;

		return workingObject.positionModified - previousPosition;
	}

	// moves (and if necessary un-rotates) the slider so that it fits inside the playfield, returns the deviation from the wanted position
	Vector2 clampSliderToPlayfield(WORKING_OBJECT &workingObject, float radius)
	{
		OsuLazerRandom::OBJECT &slider = *workingObject.obj;

		MOVEMENT_BOUNDS possibleMovementBounds = calculatePossibleMovementBounds(slider.path, radius);

		// the rotation applied in computeModifiedPosition() might make it impossible to fit the slider into the playfield
		// (e.g. a long horizontal slider rotated by 90 degrees), in that case limit the rotation to either 0 or 180 degrees
		if (possibleMovementBounds.width() < 0.0f || possibleMovementBounds.height() < 0.0f)
		{
			const float currentRotation = getSliderRotation(slider.path);
			const float diff1 = getAngleDifference(workingObject.rotationOriginal, currentRotation);
			const float diff2 = getAngleDifference(workingObject.rotationOriginal + LAZER_PI, currentRotation);

			if (diff1 < diff2)
				rotateSlider(slider, workingObject.rotationOriginal - getSliderRotation(slider.path));
			else
				rotateSlider(slider, workingObject.rotationOriginal + LAZER_PI - getSliderRotation(slider.path));

			possibleMovementBounds = calculatePossibleMovementBounds(slider.path, radius);
		}

		const Vector2 previousPosition = workingObject.positionModified;

		// clamp the head to the placement area, if the slider is larger than the playfield at least make sure that the head is inside the playfield
		const float newX = (possibleMovementBounds.width() < 0.0f
			? clampf(possibleMovementBounds.left, 0.0f, PLAYFIELD_WIDTH)
			: clampf(previousPosition.x, possibleMovementBounds.left, possibleMovementBounds.right));

		const float newY = (possibleMovementBounds.height() < 0.0f
			? clampf(possibleMovementBounds.top, 0.0f, PLAYFIELD_HEIGHT)
			: clampf(previousPosition.y, possibleMovementBounds.top, possibleMovementBounds.bottom));

		slider.pos = workingObject.positionModified = Vector2(newX, newY);
		workingObject.endPositionModified = getEndPosition(slider);

		return workingObject.positionModified - previousPosition;
	}

	// shifts a list of circles by a decreasing fraction of shift (first element most, last element least)
	void applyDecreasingShift(std::vector<OsuLazerRandom::OBJECT*> &hitObjects, const Vector2 &shift, float radius)
	{
		const int count = (int)hitObjects.size();
		for (int i=0; i<count; i++)
		{
			OsuLazerRandom::OBJECT &hitObject = *hitObjects[i];

			const Vector2 position = hitObject.pos + shift * ((float)(count - i) / (float)(count + 1));

			hitObject.pos = clampToPlayfieldWithPadding(position, radius);
		}
	}

	void repositionHitObjects(std::vector<OsuLazerRandom::OBJECT> &objects, const std::vector<POSITION_INFO> &positionInfos, float radius)
	{
		std::vector<WORKING_OBJECT> workingObjects;
		workingObjects.resize(objects.size());
		for (size_t i=0; i<objects.size(); i++)
		{
			WORKING_OBJECT &w = workingObjects[i];
			w.obj = &objects[i];
			w.info = positionInfos[i];
			w.rotationOriginal = (objects[i].type == OsuLazerRandom::TYPE::SLIDER ? getSliderRotation(objects[i].path) : 0.0f);
			w.positionModified = objects[i].pos;
			w.endPositionModified = getEndPosition(objects[i]);
		}

		WORKING_OBJECT *previous = NULL;

		for (size_t i=0; i<workingObjects.size(); i++)
		{
			WORKING_OBJECT &current = workingObjects[i];

			if (current.obj->type == OsuLazerRandom::TYPE::SPINNER)
			{
				previous = &current;
				continue;
			}

			computeModifiedPosition(current, previous, (i > 1 ? &workingObjects[i - 2] : NULL));

			// move hitobjects back into the playfield if they are outside of it
			Vector2 shift;

			switch (current.obj->type)
			{
			case OsuLazerRandom::TYPE::CIRCLE:
				shift = clampHitCircleToPlayfield(current, radius);
				break;

			case OsuLazerRandom::TYPE::SLIDER:
				shift = clampSliderToPlayfield(current, radius);
				break;

			default:
				break;
			}

			if (shift != Vector2(0, 0))
			{
				std::vector<OsuLazerRandom::OBJECT*> toBeShifted;

				for (int j=(int)i-1; j>=(int)i-PRECEDING_HITOBJECTS_TO_SHIFT && j>=0; j--)
				{
					// only shift hit circles
					if (workingObjects[j].obj->type != OsuLazerRandom::TYPE::CIRCLE) break;

					toBeShifted.push_back(workingObjects[j].obj);
				}

				if (toBeShifted.size() > 0)
					applyDecreasingShift(toBeShifted, shift, radius);
			}

			previous = &current;
		}
	}
}

void OsuLazerRandom::reposition(std::vector<OBJECT> &objects, int seed, float angleSharpness, float circleRadius)
{
	if (objects.size() < 1) return;

	angleSharpness = clampf(angleSharpness, ANGLE_SHARPNESS_MIN, ANGLE_SHARPNESS_MAX);
	circleRadius = std::max(0.0f, circleRadius);

	RNG rng(seed);

	std::vector<POSITION_INFO> positionInfos = generatePositionInfos(objects);

	// offsets the angles of all hitobjects in a "section" by the same amount
	float sectionOffset = 0.0f;

	// whether the angles are positive or negative (clockwise or counter-clockwise flow)
	bool flowDirection = false;

	for (size_t i=0; i<positionInfos.size(); i++)
	{
		if (shouldStartNewSection(objects, i, rng))
		{
			sectionOffset = getRandomOffset(rng, 0.0008f, angleSharpness);
			flowDirection = !flowDirection;
		}

		if (objects[i].type == TYPE::SLIDER && rng.nextDouble() < 0.5)
			flipSliderInPlaceHorizontally(objects[i]);

		if (i == 0)
		{
			positionInfos[i].distanceFromPrevious = (float)(rng.nextDouble() * PLAYFIELD_HEIGHT / 2.0);
			positionInfos[i].relativeAngle = (float)(rng.nextDouble() * 2.0 * LAZER_PI - LAZER_PI);
		}
		else
		{
			// offsets only the angle of the current hitobject if a flow change occurs
			float flowChangeOffset = 0.0f;

			// offsets only the angle of the current hitobject
			const float oneTimeOffset = getRandomOffset(rng, 0.002f, angleSharpness);

			if (shouldApplyFlowChange(objects, i, rng))
			{
				flowChangeOffset = getRandomOffset(rng, 0.002f, angleSharpness);
				flowDirection = !flowDirection;
			}

			const float totalOffset =
				// sectionOffset and oneTimeOffset should mainly affect patterns with large spacing
				(sectionOffset + oneTimeOffset) * positionInfos[i].distanceFromPrevious +
				// flowChangeOffset should mainly affect streams
				flowChangeOffset * (PLAYFIELD_DIAGONAL - positionInfos[i].distanceFromPrevious);

			positionInfos[i].relativeAngle = getRelativeTargetAngle(positionInfos[i].distanceFromPrevious, totalOffset, flowDirection, angleSharpness);
		}
	}

	repositionHitObjects(objects, positionInfos, circleRadius);
}

Vector2 OsuLazerRandom::applySliderTransform(const OBJECT &obj, const Vector2 &originalHead, const Vector2 &point)
{
	Vector2 relative = point - originalHead;

	if (obj.flipped)
		relative.x = -relative.x;

	const float c = std::cos(obj.rotation);
	const float s = std::sin(obj.rotation);
	const Vector2 rotated = Vector2(relative.x * c - relative.y * s, relative.x * s + relative.y * c);

	return obj.pos + rotated;
}
