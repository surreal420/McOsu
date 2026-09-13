// Host-side test for OsuLazerRandom (no engine dependencies). Build & run on Linux/macOS:
//   g++ -std=c++11 -O2 -I../../McEngine/McEngine/src/Util -Isrc/App/Osu tests/OsuLazerRandomTest.cpp -o /tmp/OsuLazerRandomTest && /tmp/OsuLazerRandomTest
// (includes OsuLazerRandom.cpp directly for white-box access to its helpers; must NOT live under src/ because the engine builds every source there)

// host-side test for OsuLazerRandom (no engine dependencies)
#include "OsuLazerRandom.cpp" // white-box: gives the test access to the internal helpers
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

typedef OsuLazerRandom::OBJECT OBJ;
typedef OsuLazerRandom::TYPE TYPE;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float W = 512.0f, H = 384.0f, R = 32.0f; // CS ~5.2 radius

static void setBeat(OBJ &o, long beatLength)
{
	const double ts = (double)o.time;
	o.onBeat = std::fmod(std::fabs(ts + 1), (double)beatLength) < 2;
	o.onDownbeat = std::fmod(std::fabs(ts + 1), (double)beatLength * 4) < 2;
}

static OBJ circle(long time, Vector2 pos, int comboIndex, int tag)
{
	OBJ o; o.type = TYPE::CIRCLE; o.time = time; o.pos = pos; o.comboIndex = comboIndex; o.newCombo = (comboIndex == 0); o.tag = tag; setBeat(o, 500); return o;
}

static std::vector<Vector2> linePath(Vector2 dir, float length)
{
	std::vector<Vector2> p;
	Vector2 n = dir; n.normalize();
	for (float d=0; d<=length; d+=5.0f) p.push_back(n * d);
	return p;
}

static OBJ slider(long time, Vector2 head, std::vector<Vector2> path, bool endsAtHead, int comboIndex, int tag)
{
	OBJ o; o.type = TYPE::SLIDER; o.time = time; o.pos = head; o.path = path; o.endsAtHead = endsAtHead; o.comboIndex = comboIndex; o.newCombo = (comboIndex == 0); o.tag = tag; setBeat(o, 500); return o;
}

static OBJ spinner(long time, int tag)
{
	OBJ o; o.type = TYPE::SPINNER; o.time = time; o.pos = Vector2(256, 192); o.newCombo = true; o.tag = tag; setBeat(o, 500); return o;
}

// zigzag jump map with some straight sliders and one spinner
static std::vector<OBJ> buildMap(int n, float spacing)
{
	std::vector<OBJ> m;
	Vector2 p(100, 100);
	int combo = 0;
	for (int i=0; i<n; i++)
	{
		long t = i * 250;
		if (i % 37 == 20)
		{
			m.push_back(spinner(t, i));
			combo = 0;
			continue;
		}
		if (i % 5 == 3)
		{
			Vector2 dir = (i % 10 == 3) ? Vector2(1, 0) : Vector2(0.6f, -0.8f);
			m.push_back(slider(t, p, linePath(dir, 120.0f), (i % 20 == 13), combo, i));
			p = m.back().pos + (m.back().endsAtHead ? Vector2(0,0) : m.back().path.back());
		}
		else
			m.push_back(circle(t, p, combo, i));

		combo = (combo + 1) % 8;
		// next position: zigzag
		Vector2 step = (i % 2 == 0) ? Vector2(spacing, spacing * 0.3f) : Vector2(spacing * 0.2f, -spacing);
		p += step;
		if (p.x > W - 40) p.x = 60;
		if (p.y < 40) p.y = H - 60;
		if (p.y > H - 40) p.y = 60;
	}
	return m;
}


// gentle chain: walks around a circle of radius 110 around the playfield centre (~57px jumps), never near the edges
static std::vector<OBJ> buildGentleMap(int n)
{
	std::vector<OBJ> m;
	for (int i=0; i<n; i++)
	{
		const float a = i * 0.5236f; // 30 deg
		m.push_back(circle(i * 250, Vector2(256 + 110*std::cos(a), 192 + 110*std::sin(a)), i % 8, i));
	}
	return m;
}

static bool sameResult(const std::vector<OBJ> &a, const std::vector<OBJ> &b)
{
	if (a.size() != b.size()) return false;
	for (size_t i=0; i<a.size(); i++)
		if (a[i].pos != b[i].pos || a[i].flipped != b[i].flipped || a[i].rotation != b[i].rotation) return false;
	return true;
}

int main()
{
	// 1. edge cases must not crash
	{
		std::vector<OBJ> empty; OsuLazerRandom::reposition(empty, 1, 7, R);
		std::vector<OBJ> one; one.push_back(circle(0, Vector2(256,192), 0, 0)); OsuLazerRandom::reposition(one, 1, 7, R);
		CHECK(one[0].pos.x >= R && one[0].pos.x <= W-R && one[0].pos.y >= R && one[0].pos.y <= H-R, "single circle clamped");
		std::vector<OBJ> sp; sp.push_back(spinner(0, 0)); OsuLazerRandom::reposition(sp, 1, 7, R);
		CHECK(sp[0].pos == Vector2(256,192), "spinner untouched");
		std::vector<OBJ> degenerate; degenerate.push_back(slider(0, Vector2(10,10), std::vector<Vector2>(1, Vector2(0,0)), false, 0, 0)); degenerate.push_back(circle(250, Vector2(10,10), 1, 1));
		OsuLazerRandom::reposition(degenerate, 3, 7, R);
		std::vector<OBJ> emptyPath; emptyPath.push_back(slider(0, Vector2(10,10), std::vector<Vector2>(), false, 0, 0)); OsuLazerRandom::reposition(emptyPath, 3, 7, R);
		printf("edge cases ok\n");
	}

	// 2. determinism + seed sensitivity
	std::vector<OBJ> original = buildMap(400, 70.0f);
	std::vector<OBJ> a = original, b = original, c = original;
	OsuLazerRandom::reposition(a, 42, 7, R);
	OsuLazerRandom::reposition(b, 42, 7, R);
	OsuLazerRandom::reposition(c, 43, 7, R);
	CHECK(sameResult(a, b), "same seed must give the same layout");
	CHECK(!sameResult(a, c), "different seed must give a different layout");

	// 3. playfield bounds + shape preservation + distance preservation
	int circles = 0, circlesInside = 0, sliders = 0, slidersFit = 0, slidersFitInside = 0, headsInside = 0;
	int pairs = 0, pairsPreserved = 0, flipped = 0, rotated = 0, moved = 0;
	for (size_t i=0; i<a.size(); i++)
	{
		const OBJ &o = a[i];
		const OBJ &orig = original[i];
		CHECK(o.tag == orig.tag && o.type == orig.type && o.time == orig.time, "metadata untouched");
		if (o.pos != orig.pos) moved++;

		if (o.type == TYPE::CIRCLE)
		{
			circles++;
			if (o.pos.x >= R-0.01f && o.pos.x <= W-R+0.01f && o.pos.y >= R-0.01f && o.pos.y <= H-R+0.01f) circlesInside++;
		}
		else if (o.type == TYPE::SLIDER)
		{
			sliders++;
			if (o.flipped) flipped++;
			if (std::fabs(o.rotation) > 1e-4f) rotated++;
			if (o.pos.x >= 0 && o.pos.x <= W && o.pos.y >= 0 && o.pos.y <= H) headsInside++;

			// transformed original path must equal the internally transformed path (rigid transform, same end point)
			Vector2 endInternal = o.pos + o.path.back();
			Vector2 endViaTransform = OsuLazerRandom::applySliderTransform(o, orig.pos, orig.pos + orig.path.back());
			CHECK((endInternal - endViaTransform).length() < 0.05f, "slider %d: end mismatch internal (%.2f,%.2f) vs transform (%.2f,%.2f)", (int)i, endInternal.x, endInternal.y, endViaTransform.x, endViaTransform.y);
			CHECK(std::fabs(o.path.back().length() - orig.path.back().length()) < 0.05f, "slider %d: length changed", (int)i);

			// bounding box (+radius) of the transformed path: if it can fit at all, it must be fully inside
			float minX=1e9, maxX=-1e9, minY=1e9, maxY=-1e9;
			for (size_t p=0; p<orig.path.size(); p++)
			{
				Vector2 q = OsuLazerRandom::applySliderTransform(o, orig.pos, orig.pos + orig.path[p]);
				minX = std::min(minX, q.x); maxX = std::max(maxX, q.x); minY = std::min(minY, q.y); maxY = std::max(maxY, q.y);
			}
			const bool canFit = (maxX - minX + 2*R <= W) && (maxY - minY + 2*R <= H);
			if (canFit)
			{
				slidersFit++;
				if (minX >= R-0.05f && maxX <= W-R+0.05f && minY >= R-0.05f && maxY <= H-R+0.05f) slidersFitInside++;
			}
		}

		// jump distance preservation between consecutive circles
		if (i > 0 && o.type == TYPE::CIRCLE && a[i-1].type == TYPE::CIRCLE)
		{
			pairs++;
			const float dOrig = (orig.pos - original[i-1].pos).length();
			const float dNew = (o.pos - a[i-1].pos).length();
			if (std::fabs(dOrig - dNew) < 0.5f) pairsPreserved++;
		}
	}
	printf("objects: %d, moved: %d\n", (int)a.size(), moved);
	printf("circles inside playfield (padded): %d/%d\n", circlesInside, circles);
	printf("slider heads inside playfield: %d/%d, fittable sliders fully inside: %d/%d, flipped: %d, rotated: %d\n", headsInside, sliders, slidersFitInside, slidersFit, flipped, rotated);
	printf("consecutive circle pairs with preserved jump distance: %d/%d\n", pairsPreserved, pairs);
	CHECK(circlesInside == circles, "all circles must be inside the padded playfield");
	CHECK(headsInside == sliders, "all slider heads must be inside the playfield");
	CHECK(slidersFitInside == slidersFit, "every slider that can fit must be fully inside the playfield");
	// white-box unit checks of the internal helpers
	{
		const float len = rotateVector(Vector2(30, 40), 1.234f).length();
		CHECK(std::fabs(len - 50.0f) < 1e-3f, "rotateVector keeps length (%f)", len);
		const Vector2 tw = rotateVectorTowardsVector(Vector2(100, 0), Vector2(0, 100), 1.0f);
		CHECK((tw - Vector2(0, 100)).length() < 1e-3f, "rotateVectorTowardsVector ratio 1 lands on destination direction");
		const Vector2 half = rotateVectorTowardsVector(Vector2(100, 0), Vector2(0, 100), 0.5f);
		CHECK(std::fabs(std::atan2(half.y, half.x) - LAZER_PI/4) < 1e-4f, "rotateVectorTowardsVector ratio 0.5 rotates half way");
		// previous object at the left edge, jump pointing further left: must be bent towards the centre (positive x component), same length
		const Vector2 away = rotateAwayFromEdge(Vector2(10, 192), Vector2(-60, 0));
		CHECK(std::fabs(away.length() - 60.0f) < 1e-3f && away.x > -60.0f + 1.0f, "rotateAwayFromEdge bends towards the centre and keeps length (%f,%f)", away.x, away.y);
		// far from the edges: no rotation at all
		const Vector2 keep = rotateAwayFromEdge(Vector2(256, 192), Vector2(-60, 0));
		CHECK((keep - Vector2(-60, 0)).length() < 1e-3f, "rotateAwayFromEdge is identity in the middle");
		CHECK(clampToPlayfieldWithPadding(Vector2(-5, 500), 32) == Vector2(32, 352), "clampToPlayfieldWithPadding");
		CHECK(std::fabs(getAngleDifference(0.1f, 2*LAZER_PI - 0.1f) - 0.2f) < 1e-4f, "getAngleDifference wraps");

		std::vector<Vector2> line = linePath(Vector2(1, 0), 100.0f);
		MOVEMENT_BOUNDS b = calculatePossibleMovementBounds(line, 32.0f);
		CHECK(std::fabs(b.left - 32) < 1e-3f && std::fabs(b.right - 380) < 1e-3f && std::fabs(b.top - 32) < 1e-3f && std::fabs(b.bottom - 352) < 1e-3f, "movement bounds of a 100px horizontal slider: %f %f %f %f", b.left, b.right, b.top, b.bottom);
		const Vector2 cm = calculateCentreOfMass(line);
		CHECK((cm - Vector2(25, 0)).length() < 1e-3f, "centre of mass samples every 50px like lazer (%f,%f)", cm.x, cm.y);
		CHECK((calculateCentreOfMass(linePath(Vector2(1, 0), 40.0f)) - Vector2(20, 0)).length() < 1e-3f, "centre of mass of a short slider is end/2");
		CHECK(std::fabs(getSliderRotation(linePath(Vector2(0, 1), 50.0f)) - LAZER_PI/2) < 1e-4f, "slider rotation");

		// lazer formula: distance 57, sharpness 7 -> relative angle ~ pi - (2.16/(1+200*exp(0.036*(57-310))) + 0.5)
		const float expected = LAZER_PI - (float)(2.16 / (1.0 + 200.0 * std::exp(0.036 * (57.0 - 310.0))) + 0.5);
		CHECK(std::fabs(getRelativeTargetAngle(57.0f, 0.0f, false, 7.0f) - expected) < 1e-4f, "getRelativeTargetAngle");
		CHECK(std::fabs(getRelativeTargetAngle(57.0f, 0.0f, true, 7.0f) + expected) < 1e-4f, "getRelativeTargetAngle flow direction flips sign");

		// position infos: two circles, then a slider pointing up
		std::vector<OBJ> m;
		m.push_back(circle(0, Vector2(256, 192), 0, 0));
		m.push_back(circle(250, Vector2(356, 192), 1, 1));
		m.push_back(slider(500, Vector2(356, 292), linePath(Vector2(0, 1), 80.0f), false, 2, 2));
		m.push_back(circle(750, Vector2(356, 372), 3, 3)); // right at the slider end
		std::vector<POSITION_INFO> infos = generatePositionInfos(m);
		CHECK(std::fabs(infos[1].distanceFromPrevious - 100) < 1e-3f && std::fabs(infos[1].relativeAngle) < 1e-4f, "info[1]: straight 100px jump");
		CHECK(std::fabs(infos[2].distanceFromPrevious - 100) < 1e-3f && std::fabs(infos[2].relativeAngle - LAZER_PI/2) < 1e-4f && std::fabs(infos[2].rotation) < 1e-4f, "info[2]: 90 deg turn, slider body continues straight (rotation 0)");
		CHECK(std::fabs(infos[3].distanceFromPrevious) < 1e-3f, "info[3]: measured from the slider END (distance 0)");
		m[2].endsAtHead = true;
		infos = generatePositionInfos(m);
		CHECK(std::fabs(infos[3].distanceFromPrevious - 80) < 1e-3f, "info[3]: repeat slider ends at its head (distance 80)");
	}

	// mirrored reposition loop: before clamping, every object is exactly distanceFromPrevious away from the previous object's end
	{
		std::vector<OBJ> gOrig = buildGentleMap(300);
		int checkedPairs = 0, clamps = 0;
		for (int seed=1; seed<=100; seed++)
		{
			std::vector<OBJ> objects = gOrig;
			std::vector<POSITION_INFO> infos = generatePositionInfos(objects);
			for (size_t i=0; i<infos.size(); i++) infos[i].distanceFromPrevious *= 3.0f; // ~170px jumps so that the chain reaches the edges
			std::vector<WORKING_OBJECT> w(objects.size());
			for (size_t i=0; i<objects.size(); i++) { w[i].obj = &objects[i]; w[i].info = infos[i]; w[i].rotationOriginal = 0; w[i].positionModified = objects[i].pos; w[i].endPositionModified = objects[i].pos; }
			WORKING_OBJECT *previous = NULL;
			for (size_t i=0; i<w.size(); i++)
			{
				const Vector2 lastEnd = previous ? previous->endPositionModified : PLAYFIELD_CENTRE;
				computeModifiedPosition(w[i], previous, i > 1 ? &w[i-2] : NULL);
				const float d = (w[i].positionModified - lastEnd).length();
				CHECK(std::fabs(d - w[i].info.distanceFromPrevious) < 1e-2f, "seed %d obj %d: placed at distance %f instead of %f", seed, (int)i, d, w[i].info.distanceFromPrevious);
				checkedPairs++;
				const Vector2 before = w[i].positionModified;
				const Vector2 shift = clampHitCircleToPlayfield(w[i], R);
				if (shift != Vector2(0, 0))
				{
					clamps++;
					const Vector2 p = objects[i].pos;
					CHECK(std::fabs(p.x - R) < 1e-3f || std::fabs(p.x - (W-R)) < 1e-3f || std::fabs(p.y - R) < 1e-3f || std::fabs(p.y - (H-R)) < 1e-3f, "clamped object must sit on the padded boundary");
					CHECK((before + shift - p).length() < 1e-3f, "shift is the clamp deviation");
					std::vector<OBJ*> toBeShifted;
					for (int j=(int)i-1; j>=(int)i-PRECEDING_HITOBJECTS_TO_SHIFT && j>=0; j--) toBeShifted.push_back(&objects[j]);
					applyDecreasingShift(toBeShifted, shift, R);
				}
				previous = &w[i];
			}
		}
		printf("mirrored loop: %d placements checked, %d clamps\n", checkedPairs, clamps);
		CHECK(clamps > 0, "the gentle map should still hit the edge sometimes");
	}

	// 4. sharpness extremes must not crash and must still respect bounds
	for (int s=1; s<=10; s+=9)
	{
		std::vector<OBJ> d = original;
		OsuLazerRandom::reposition(d, 7, (float)s, R);
		for (size_t i=0; i<d.size(); i++)
			if (d[i].type == TYPE::CIRCLE)
				CHECK(d[i].pos.x >= R-0.01f && d[i].pos.x <= W-R+0.01f && d[i].pos.y >= R-0.01f && d[i].pos.y <= H-R+0.01f, "sharpness %d: circle %d out of bounds (%.1f,%.1f)", s, (int)i, d[i].pos.x, d[i].pos.y);
	}

	// 5. a huge slider (bigger than the playfield) keeps its head inside
	{
		std::vector<OBJ> m; m.push_back(circle(0, Vector2(100,100), 0, 0)); m.push_back(slider(250, Vector2(0,192), linePath(Vector2(1,0), 700.0f), false, 1, 1)); m.push_back(circle(500, Vector2(300,300), 2, 2));
		OsuLazerRandom::reposition(m, 5, 7, R);
		CHECK(m[1].pos.x >= 0 && m[1].pos.x <= W && m[1].pos.y >= 0 && m[1].pos.y <= H, "huge slider head inside (%.1f,%.1f)", m[1].pos.x, m[1].pos.y);
		CHECK(m[2].pos.x >= R && m[2].pos.x <= W-R, "circle after huge slider inside");
	}

	printf("%s (%d failures)\n", g_failures == 0 ? "ALL OK" : "FAILED", g_failures);
	return g_failures == 0 ? 0 : 1;
}
