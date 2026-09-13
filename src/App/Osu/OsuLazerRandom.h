//================ Copyright (c) 2026, PG & surreal420, All rights reserved. =================//
//
// Purpose:		osu!lazer "Random" mod: reposition hitobjects while keeping jump distances, randomizing angles
//
// $NoKeywords: $osulazerrandom
//===============================================================================//

#ifndef OSULAZERRANDOM_H
#define OSULAZERRANDOM_H

#include "Vectors.h"

#include <vector>

// Port of osu!lazer's OsuModRandom + OsuHitObjectGenerationUtils (Reposition) for osu!standard.
// Works purely in osu!pixel space (512x384) and does not depend on any engine/game class, so it can be unit tested on its own.
class OsuLazerRandom
{
public:
	enum class TYPE
	{
		CIRCLE,
		SLIDER,
		SPINNER
	};

	struct OBJECT
	{
		// in
		TYPE type;
		long time;				// start time in ms
		int comboIndex;			// 0-based index within its combo (lazer: IndexInCurrentCombo)
		bool newCombo;			// starts a new combo (lazer: NewCombo)
		bool onBeat;			// start time lies on a beat of the active uninherited timing point
		bool onDownbeat;		// start time lies on a downbeat
		int tag;				// free for the caller (e.g. index into the source array), untouched

		// in, SLIDER only
		std::vector<Vector2> path;	// sampled curve relative to the head (path[0] should be (0,0), last element is the end of the curve)
		bool endsAtHead;			// even number of spans, i.e. the slider ends where it started

		// in/out
		Vector2 pos;			// head position in osu!pixels

		// out, SLIDER only: rigid transform to apply to the original control points, relative to the original head
		// (see applySliderTransform(): first mirror x if flipped, then rotate by rotation radians, then move the head to pos)
		bool flipped;
		float rotation;

		OBJECT() : type(TYPE::CIRCLE), time(0), comboIndex(0), newCombo(false), onBeat(false), onDownbeat(false), tag(0), endsAtHead(false), flipped(false), rotation(0.0f) {;}
	};

	static const float ANGLE_SHARPNESS_MIN;		// 1
	static const float ANGLE_SHARPNESS_MAX;		// 10
	static const float ANGLE_SHARPNESS_DEFAULT;	// 7

	// objects must be sorted by time. angleSharpness is clamped to [1, 10]. circleRadius in osu!pixels (used for playfield clamping).
	static void reposition(std::vector<OBJECT> &objects, int seed, float angleSharpness, float circleRadius);

	// maps a point given in the same coordinate space as the original control points of obj (SLIDER) to its repositioned location
	static Vector2 applySliderTransform(const OBJECT &obj, const Vector2 &originalHead, const Vector2 &point);
};

#endif
