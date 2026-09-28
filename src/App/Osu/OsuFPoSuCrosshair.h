//================ Copyright (c) 2026, PG & surreal420, All rights reserved. =================//
//
// Purpose:		CS2 style static crosshair geometry for FPoSu (engine independent)
//
// $NoKeywords: $fposuxhair
//===============================================================================//

#ifndef OSUFPOSUCROSSHAIR_H
#define OSUFPOSUCROSSHAIR_H

#include <vector>
#include <cstddef>

// Mirrors the CS2 "Rush Hour" (2026-09-22) pixel based crosshair settings:
// length 0..255 px, thickness 0..31 px (0 draws as 1), gap 0..128 px, RGBA 0..255,
// outline off/full/half (half = only the top-left edges of every bar), optional center dot.
// All values are literal screen pixels. build() returns axis aligned rects, outline rects first.
class OsuFPoSuCrosshair
{
public:
	enum OUTLINE
	{
		OUTLINE_NONE = 0,
		OUTLINE_FULL = 1,
		OUTLINE_HALF = 2
	};

	struct PARAMS
	{
		int r, g, b, a;
		int outline;	// OUTLINE
		int thickness;
		bool dot;
		int length;
		int gap;
	};

	struct RECT
	{
		int x, y, w, h;
		bool outline;
	};

	static const int MAX_THICKNESS = 31;
	static const int MAX_LENGTH = 255;
	static const int MAX_GAP = 128;

	static void clampParams(PARAMS &p);

	// cx/cy = the center pixel; with an odd thickness the center pixel is the middle of the cross
	static std::vector<RECT> build(PARAMS p, int cx, int cy);
};

#endif
