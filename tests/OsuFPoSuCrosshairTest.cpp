// Host-side test for OsuFPoSuCrosshair (no engine dependencies). Build & run on Linux/macOS:
//   g++ -std=c++11 -O2 -Isrc/App/Osu tests/OsuFPoSuCrosshairTest.cpp src/App/Osu/OsuFPoSuCrosshair.cpp -o /tmp/OsuFPoSuCrosshairTest && /tmp/OsuFPoSuCrosshairTest
// (must NOT live under src/ because the engine builds every source there)

#include "OsuFPoSuCrosshair.h"
#include <cstdio>
#include <string>
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
	// gap 2 = innermost bar pixel is 2px from the center pixel -> exactly 1 empty pixel per side
	CHECK(r.size() == 4, "thin cross: expected 4 rects, got %zu", r.size());
	CHECK(hasRect(r, 95, 100, 4, 1, false), "thin cross: left bar");
	CHECK(hasRect(r, 102, 100, 4, 1, false), "thin cross: right bar");
	CHECK(hasRect(r, 100, 95, 1, 4, false), "thin cross: top bar");
	CHECK(hasRect(r, 100, 102, 1, 4, false), "thin cross: bottom bar");

	// gap 1: bars touch the center pixel, only the center pixel stays empty
	p.gap = 1;
	r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 4, "gap 1: expected 4 rects, got %zu", r.size());
	CHECK(hasRect(r, 96, 100, 4, 1, false), "gap 1: left bar ends next to the center pixel");
	CHECK(hasRect(r, 101, 100, 4, 1, false), "gap 1: right bar starts next to the center pixel");
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
	p.gap = 1;
	std::vector<RECT> r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 4, "thick gap 1: expected 4 rects, got %zu", r.size());
	CHECK(hasRect(r, 94, 99, 5, 3, false), "thick gap 1: left bar touches center block");
	CHECK(hasRect(r, 102, 99, 5, 3, false), "thick gap 1: right bar touches center block");
	CHECK(hasRect(r, 99, 94, 3, 5, false), "thick gap 1: top bar");
	CHECK(hasRect(r, 99, 102, 3, 5, false), "thick gap 1: bottom bar");

	// gap 0: solid cross, the innermost bar pixel is the center block, so bars keep length-1 px outside of it
	p.gap = 0;
	r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 5, "thick gap 0: expected 5 rects, got %zu", r.size());
	CHECK(hasRect(r, 95, 99, 4, 3, false), "thick gap 0: left bar");
	CHECK(hasRect(r, 102, 99, 4, 3, false), "thick gap 0: right bar");
	CHECK(hasRect(r, 99, 95, 3, 4, false), "thick gap 0: top bar");
	CHECK(hasRect(r, 99, 102, 3, 4, false), "thick gap 0: bottom bar");
	CHECK(hasRect(r, 99, 99, 3, 3, false), "thick gap 0: center block filled");

	// gap 0 with dot: the center block is still emitted exactly once
	p.dot = true;
	r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 5, "thick gap 0 + dot: expected 5 rects, got %zu", r.size());

	// gap 0 with length 1 (thin): only the center pixel remains
	PARAMS q = base();
	q.gap = 0;
	q.length = 1;
	r = OsuFPoSuCrosshair::build(q, 100, 100);
	CHECK(r.size() == 1 && hasRect(r, 100, 100, 1, 1, false), "gap 0 length 1: only the center pixel, got %zu rects", r.size());

	// even thickness: center block starts at cx - t/2
	p = base();
	p.thickness = 2;
	p.length = 1;
	r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(hasRect(r, 97, 99, 1, 2, false), "even thickness: left bar");
	CHECK(hasRect(r, 102, 99, 1, 2, false), "even thickness: right bar");
}

static void testFullOutline()
{
	PARAMS p = base();
	p.outline = OsuFPoSuCrosshair::OUTLINE_FULL;
	p.dot = true;
	std::vector<RECT> r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 10, "full outline: expected 10 rects, got %zu", r.size());
	CHECK(countOutline(r, true) == 5, "full outline: 5 outline rects");
	CHECK(hasRect(r, 94, 99, 6, 3, true), "full outline: left bar outline");
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
	// left bar is {95,99,3,2} (center block starts at 99, gap 2 = 1 empty px, length 3):
	// top strip spans the top-left corner + top edge, left strip the left edge
	CHECK(hasRect(r, 95, 99, 3, 2, false), "half outline: left bar fill");
	CHECK(hasRect(r, 94, 98, 4, 1, true), "half outline: left bar top strip");
	CHECK(hasRect(r, 94, 99, 1, 2, true), "half outline: left bar left strip");
	// nothing on the right/bottom sides
	CHECK(!hasRect(r, 98, 99, 1, 2, true), "half outline: no right strip");
	CHECK(!hasRect(r, 94, 101, 4, 1, true), "half outline: no bottom strip");
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
	p.dot = false;
	p.gap = 0;
	r = OsuFPoSuCrosshair::build(p, 100, 100);
	CHECK(r.size() == 0, "length 0 gap 0 without dot: nothing, got %zu", r.size());

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
	CHECK(hasRect(r, 95, 100, 4, 1, false), "build clamps thickness 0 to 1");
}

typedef OsuFPoSuCrosshair::SHARECODE SHARECODE;

static void testDecodeUserCode()
{
	// user supplied v4 code (static cross, half outline, gap 3, length 3, thickness 1, 1080p)
	SHARECODE c;
	OsuFPoSuCrosshair::DECODE_RESULT res = OsuFPoSuCrosshair::decodeShareCode("CSGO-j2ajq-qWSp8-eZuK3-ZWxTa-tDnoE", c);
	CHECK(res == OsuFPoSuCrosshair::DECODE_OK, "user code: decode result %d", (int)res);
	CHECK(c.version == 4, "user code: version %d", c.version);
	CHECK(c.style == 4, "user code: style %d", c.style);
	CHECK(c.outline == OsuFPoSuCrosshair::OUTLINE_HALF, "user code: outline %d", c.outline);
	CHECK(c.r == 255 && c.g == 182 && c.b == 193 && c.a == 220, "user code: rgba %d %d %d %d", c.r, c.g, c.b, c.a);
	CHECK(c.gap == 3 && c.length == 3 && c.thickness == 1, "user code: gap %d length %d thickness %d", c.gap, c.length, c.thickness);
	CHECK(!c.dot && !c.tStyle && !c.recoil, "user code: flags");
	CHECK(c.spreadLimit == 255 && c.splitDistance == 5 && c.innerAlphaRaw == 20 && c.outerAlphaRaw == 0 && c.splitRatioRaw == 0, "user code: dynamic fields");
	CHECK(c.screenHeight == 1080, "user code: screen height %d", c.screenHeight);

	// whitespace and lowercase "csgo-" prefix are tolerated
	CHECK(OsuFPoSuCrosshair::decodeShareCode("  csgo-j2ajq-qWSp8-eZuK3-ZWxTa-tDnoE\n", c) == OsuFPoSuCrosshair::DECODE_OK, "user code: trimmed/lowercase prefix");
}

static void testDecodePublicCodes()
{
	SHARECODE c;
	// f0rest (v4): style 5, dot, full outline, 250/250/250, gap 0, length 2, thickness 1
	CHECK(OsuFPoSuCrosshair::decodeShareCode("CSGO-uQPmY-jAqPO-O4O2C-Gj299-BzuKG", c) == OsuFPoSuCrosshair::DECODE_OK, "f0rest: decode");
	CHECK(c.version == 4 && c.style == 5 && c.dot && c.outline == 1 && c.r == 250 && c.gap == 0 && c.length == 2 && c.thickness == 1 && c.screenHeight == 1080, "f0rest: fields");

	// donk (v3): outline off, 0/255/135, gap 0, length 2, thickness 2, 960p
	CHECK(OsuFPoSuCrosshair::decodeShareCode("CSGO-RK9FT-PLK2v-dsfSa-5w8jZ-7fDxH", c) == OsuFPoSuCrosshair::DECODE_OK, "donk: decode");
	CHECK(c.version == 3 && c.style == 4 && !c.dot && c.outline == 0 && c.g == 255 && c.b == 135 && c.gap == 0 && c.length == 2 && c.thickness == 2 && c.screenHeight == 960, "donk: fields");

	// v3 with outline bit set (published fixture): outline 1, thickness 31, length 255, 1440p
	CHECK(OsuFPoSuCrosshair::decodeShareCode("CSGO-frCAy-PRXin-PY8Kj-wDTUo-TWAwO", c) == OsuFPoSuCrosshair::DECODE_OK, "v3 fixture: decode");
	CHECK(c.version == 3 && c.outline == 1 && c.thickness == 31 && c.length == 255 && c.screenHeight == 1440, "v3 fixture: fields");

	// v4 with recoil + dot, style 2
	CHECK(OsuFPoSuCrosshair::decodeShareCode("CSGO-sP6xU-TSyN9-sZcO5-2D48M-UppkP", c) == OsuFPoSuCrosshair::DECODE_OK, "v4b: decode");
	CHECK(c.version == 4 && c.style == 2 && c.recoil && c.dot && c.outline == 0 && c.r == 255 && c.length == 5 && c.screenHeight == 768, "v4b: fields");
}

static void testDecodeErrors()
{
	SHARECODE c;
	CHECK(OsuFPoSuCrosshair::decodeShareCode("", c) == OsuFPoSuCrosshair::DECODE_BAD_FORMAT, "empty");
	CHECK(OsuFPoSuCrosshair::decodeShareCode("CSGO-j2ajq-qWSp8-eZuK3-ZWxTa", c) == OsuFPoSuCrosshair::DECODE_BAD_FORMAT, "too short");
	CHECK(OsuFPoSuCrosshair::decodeShareCode("CSGO-j2ajq-qWSp8-eZuK3-ZWxTa-tDno0", c) == OsuFPoSuCrosshair::DECODE_BAD_FORMAT, "invalid character (0 is not in the alphabet)");
	CHECK(OsuFPoSuCrosshair::decodeShareCode("CSGO-j2ajq-qWSp8-eZuK3-ZWxTa-tDnoF", c) == OsuFPoSuCrosshair::DECODE_BAD_CHECKSUM, "checksum");
	CHECK(OsuFPoSuCrosshair::decodeShareCode("CSGO-99999-99999-99999-99999-99999", c) == OsuFPoSuCrosshair::DECODE_BAD_FORMAT, "out of 144 bit range");
	// pre "Rush Hour" (v1) code: must be rejected as old, not as garbage
	CHECK(OsuFPoSuCrosshair::decodeShareCode("CSGO-WsnnD-eHaMw-QNDf9-oxuDh-ydOUD", c) == OsuFPoSuCrosshair::DECODE_OLD_VERSION, "old v1 code");
}

static void testEncodeRoundtrip()
{
	const char *codes[] = {
		"CSGO-j2ajq-qWSp8-eZuK3-ZWxTa-tDnoE",
		"CSGO-uQPmY-jAqPO-O4O2C-Gj299-BzuKG",
		"CSGO-RK9FT-PLK2v-dsfSa-5w8jZ-7fDxH",
		"CSGO-UJOA4-HOjFP-Dy4wk-vieCq-TKTVP",
		"CSGO-OK6DC-masuG-LF59Q-s3K4w-GpqcG",
		"CSGO-7CGxA-32vy6-NNuc4-WCUpn-SUpAL",
		"CSGO-frCAy-PRXin-PY8Kj-wDTUo-TWAwO",
		"CSGO-sP6xU-TSyN9-sZcO5-2D48M-UppkP",
	};
	for (size_t i=0; i<sizeof(codes)/sizeof(codes[0]); i++)
	{
		SHARECODE c;
		CHECK(OsuFPoSuCrosshair::decodeShareCode(codes[i], c) == OsuFPoSuCrosshair::DECODE_OK, "roundtrip %zu: decode", i);
		const std::string again = OsuFPoSuCrosshair::encodeShareCode(c);
		CHECK(again == codes[i], "roundtrip %zu: got %s", i, again.c_str());
	}

	// a default constructed SHARECODE encodes as v4 and survives a roundtrip
	SHARECODE d;
	d.r = 12; d.g = 34; d.b = 56; d.a = 78; d.gap = 3; d.length = 9; d.thickness = 2; d.outline = 2; d.dot = true; d.screenHeight = 1440;
	const std::string code = OsuFPoSuCrosshair::encodeShareCode(d);
	CHECK(code.size() == 34 && code.compare(0, 5, "CSGO-") == 0, "default encode: format %s", code.c_str());
	SHARECODE e;
	CHECK(OsuFPoSuCrosshair::decodeShareCode(code, e) == OsuFPoSuCrosshair::DECODE_OK, "default encode: decodes");
	CHECK(e.version == 4 && e.style == 4 && e.r == 12 && e.g == 34 && e.b == 56 && e.a == 78 && e.gap == 3 && e.length == 9 && e.thickness == 2 && e.outline == 2 && e.dot && e.screenHeight == 1440, "default encode: fields survive");
}

static void testParamsConversion()
{
	SHARECODE c;
	OsuFPoSuCrosshair::decodeShareCode("CSGO-j2ajq-qWSp8-eZuK3-ZWxTa-tDnoE", c);
	PARAMS p = OsuFPoSuCrosshair::paramsFromShareCode(c);
	CHECK(p.r == 255 && p.g == 182 && p.b == 193 && p.a == 220 && p.outline == 2 && p.thickness == 1 && !p.dot && p.length == 3 && p.gap == 3, "params from code");

	p.dot = true;
	SHARECODE back = OsuFPoSuCrosshair::shareCodeFromParams(p, 1440);
	CHECK(back.version == 4 && back.style == 4 && back.dot && back.a == 220 && back.gap == 3 && back.screenHeight == 1440, "code from params");
}

static void testScaleToScreen()
{
	PARAMS p = base();
	p.thickness = 2; p.length = 3; p.gap = 3;
	OsuFPoSuCrosshair::scaleToScreen(p, 1080, 1440);
	CHECK(p.thickness == 3 && p.length == 4 && p.gap == 4, "scale 1080->1440: %d %d %d", p.thickness, p.length, p.gap);

	p = base(); p.thickness = 2; p.length = 3; p.gap = 3;
	OsuFPoSuCrosshair::scaleToScreen(p, 0, 1440);
	CHECK(p.thickness == 2 && p.length == 3 && p.gap == 3, "scale with reference 0: unchanged");

	p = base(); p.thickness = 1; p.length = 1; p.gap = 1;
	OsuFPoSuCrosshair::scaleToScreen(p, 2160, 720);
	CHECK(p.thickness == 1 && p.length == 0 && p.gap == 0, "scale 2160->720: thickness stays >= 1, others round to 0 (%d %d %d)", p.thickness, p.length, p.gap);

	p = base(); p.thickness = 31; p.length = 255; p.gap = 128;
	OsuFPoSuCrosshair::scaleToScreen(p, 720, 2160);
	CHECK(p.thickness == 31 && p.length == 255 && p.gap == 128, "scale up clamps to the maxima");
}

int main()
{
	testDecodeUserCode();
	testDecodePublicCodes();
	testDecodeErrors();
	testEncodeRoundtrip();
	testParamsConversion();
	testScaleToScreen();

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
