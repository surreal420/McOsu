// Host-side test for OsuFPoSuCrosshair (no engine dependencies). Build & run on Linux/macOS:
//   g++ -std=c++11 -O2 -Isrc/App/Osu tests/OsuFPoSuCrosshairTest.cpp src/App/Osu/OsuFPoSuCrosshair.cpp -o /tmp/OsuFPoSuCrosshairTest && /tmp/OsuFPoSuCrosshairTest
// (must NOT live under src/ because the engine builds every source there)

#include "OsuFPoSuCrosshair.h"
#include <cstdio>
#include <vector>

typedef OsuFPoSuCrosshair::PARAMS PARAMS;
typedef OsuFPoSuCrosshair::RECT RECT;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static bool hasRect(const std::vector<RECT> &rects, int x, int y, int w, int h, bool outline)
{
	for (size_t i=0; i<rects.size(); i++)
	{
		const RECT &r = rects[i];
		if (r.x == x && r.y == y && r.w == w && r.h == h && r.outline == outline)
			return true;
	}
	return false;
}

static size_t countOutline(const std::vector<RECT> &rects, bool outline)
{
	size_t n = 0;
	for (size_t i=0; i<rects.size(); i++)
		if (rects[i].outline == outline) n++;
	return n;
}

static PARAMS base()
{
	PARAMS p;
	p.r = 0; p.g = 255; p.b = 0; p.a = 255;
	p.outline = OsuFPoSuCrosshair::OUTLINE_NONE;
	p.thickness = 1;
	p.dot = false;
	p.length = 4;
	p.gap = 2;
	return p;
}

static void testThinCrossNoOutline()
{
	PARAMS p = base();
	std::vector<RECT> r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 4, "thin cross: expected 4 rects, got %zu", r.size());
	CHECK(hasRect(r, 94, 100, 4, 1, false), "thin cross: left bar");
	CHECK(hasRect(r, 103, 100, 4, 1, false), "thin cross: right bar");
	CHECK(hasRect(r, 100, 94, 1, 4, false), "thin cross: top bar");
	CHECK(hasRect(r, 100, 103, 1, 4, false), "thin cross: bottom bar");
}

static void testCenterDot()
{
	PARAMS p = base();
	p.dot = true;
	std::vector<RECT> r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 5, "dot: expected 5 rects, got %zu", r.size());
	CHECK(hasRect(r, 100, 100, 1, 1, false), "dot: 1x1 center pixel");

	p.thickness = 3;
	r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(hasRect(r, 99, 99, 3, 3, false), "dot: 3x3 centered on (100,100)");
}

static void testThickBarsAndZeroGap()
{
	PARAMS p = base();
	p.thickness = 3;
	p.length = 5;
	p.gap = 0;
	std::vector<RECT> r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 4, "thick: expected 4 rects, got %zu", r.size());
	CHECK(hasRect(r, 94, 99, 5, 3, false), "thick: left bar touches center block");
	CHECK(hasRect(r, 102, 99, 5, 3, false), "thick: right bar touches center block");
	CHECK(hasRect(r, 99, 94, 3, 5, false), "thick: top bar");
	CHECK(hasRect(r, 99, 102, 3, 5, false), "thick: bottom bar");

	// even thickness: center block starts at cx - t/2
	p.thickness = 2;
	p.length = 1;
	r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(hasRect(r, 98, 99, 1, 2, false), "even thickness: left bar");
	CHECK(hasRect(r, 101, 99, 1, 2, false), "even thickness: right bar");
}

static void testFullOutline()
{
	PARAMS p = base();
	p.outline = OsuFPoSuCrosshair::OUTLINE_FULL;
	p.dot = true;
	std::vector<RECT> r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 10, "full outline: expected 10 rects, got %zu", r.size());
	CHECK(countOutline(r, true) == 5, "full outline: 5 outline rects");
	CHECK(hasRect(r, 93, 99, 6, 3, true), "full outline: left bar outline");
	CHECK(hasRect(r, 99, 99, 3, 3, true), "full outline: dot outline");
	// outlines are emitted before fills so fills always win
	for (size_t i=0; i<r.size(); i++)
		CHECK(r[i].outline == (i < 5), "full outline: rect %zu ordering", i);
}

static void testHalfOutline()
{
	PARAMS p = base();
	p.outline = OsuFPoSuCrosshair::OUTLINE_HALF;
	p.thickness = 2;
	p.length = 3;
	std::vector<RECT> r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 12, "half outline: expected 12 rects, got %zu", r.size());
	CHECK(countOutline(r, true) == 8, "half outline: 8 outline strips");
	// left bar is {94,99,3,2} (center block starts at 99, gap 2, length 3):
	// top strip spans the top-left corner + top edge, left strip the left edge
	CHECK(hasRect(r, 94, 99, 3, 2, false), "half outline: left bar fill");
	CHECK(hasRect(r, 93, 98, 4, 1, true), "half outline: left bar top strip");
	CHECK(hasRect(r, 93, 99, 1, 2, true), "half outline: left bar left strip");
	// nothing on the right/bottom sides
	CHECK(!hasRect(r, 97, 99, 1, 2, true), "half outline: no right strip");
	CHECK(!hasRect(r, 93, 101, 4, 1, true), "half outline: no bottom strip");
}

static void testZeroLengthAndClamping()
{
	PARAMS p = base();
	p.length = 0;
	std::vector<RECT> r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 0, "length 0 without dot: nothing, got %zu", r.size());
	p.dot = true;
	r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 1, "length 0 with dot: only the dot, got %zu", r.size());

	PARAMS c = base();
	c.r = -5; c.g = 999; c.b = 256; c.a = -1;
	c.outline = 7; c.thickness = 0; c.length = 1000; c.gap = -3;
	OsuFPoSuCrosshair::clampParams(c);
	CHECK(c.r == 0 && c.g == 255 && c.b == 255 && c.a == 0, "clamp: rgba");
	CHECK(c.outline == OsuFPoSuCrosshair::OUTLINE_NONE, "clamp: invalid outline -> none");
	CHECK(c.thickness == 1, "clamp: thickness 0 -> 1 (CS2 minimum 1px)");
	CHECK(c.length == 255, "clamp: length max 255");
	CHECK(c.gap == 0, "clamp: gap min 0");
	c.thickness = 50; c.gap = 500;
	OsuFPoSuCrosshair::clampParams(c);
	CHECK(c.thickness == 31 && c.gap == 128, "clamp: thickness max 31, gap max 128");

	// build() clamps too: thickness 0 draws as 1px
	PARAMS z = base();
	z.thickness = 0;
	r = OsuFPoSuCrosshair::build(z, 100, 100);
	CHECK(hasRect(r, 94, 100, 4, 1, false), "build clamps thickness 0 to 1");
}

int main()
{
	testThinCrossNoOutline();
	testCenterDot();
	testThickBarsAndZeroGap();
	testFullOutline();
	testHalfOutline();
	testZeroLengthAndClamping();

	if (g_failures == 0)
		printf("OK: all OsuFPoSuCrosshair tests passed\n");
	else
		printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
