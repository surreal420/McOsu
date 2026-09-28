//================ Copyright (c) 2026, PG & surreal420, All rights reserved. =================//
//
// Purpose:		CS2 style static crosshair geometry for FPoSu (engine independent)
//
// $NoKeywords: $fposuxhair
//===============================================================================//

#include "OsuFPoSuCrosshair.h"

static int clampInt(int v, int lo, int hi)
{
	return (v < lo ? lo : (v > hi ? hi : v));
}

void OsuFPoSuCrosshair::clampParams(PARAMS &p)
{
	p.r = clampInt(p.r, 0, 255);
	p.g = clampInt(p.g, 0, 255);
	p.b = clampInt(p.b, 0, 255);
	p.a = clampInt(p.a, 0, 255);

	if (p.outline != OUTLINE_FULL && p.outline != OUTLINE_HALF)
		p.outline = OUTLINE_NONE;

	p.thickness = clampInt(p.thickness, 1, MAX_THICKNESS); // CS2: "minimum 1 pixel"
	p.length = clampInt(p.length, 0, MAX_LENGTH);
	p.gap = clampInt(p.gap, 0, MAX_GAP);
}

static void addOutline(std::vector<OsuFPoSuCrosshair::RECT> &out, const OsuFPoSuCrosshair::RECT &fill, int outline)
{
	OsuFPoSuCrosshair::RECT o;
	o.outline = true;

	switch (outline)
	{
	case OsuFPoSuCrosshair::OUTLINE_FULL:
		o.x = fill.x - 1;
		o.y = fill.y - 1;
		o.w = fill.w + 2;
		o.h = fill.h + 2;
		out.push_back(o);
		break;

	case OsuFPoSuCrosshair::OUTLINE_HALF:
		// top edge (including the top-left corner pixel)
		o.x = fill.x - 1;
		o.y = fill.y - 1;
		o.w = fill.w + 1;
		o.h = 1;
		out.push_back(o);
		// left edge
		o.x = fill.x - 1;
		o.y = fill.y;
		o.w = 1;
		o.h = fill.h;
		out.push_back(o);
		break;

	default:
		break;
	}
}

std::vector<OsuFPoSuCrosshair::RECT> OsuFPoSuCrosshair::build(PARAMS p, int cx, int cy)
{
	clampParams(p);

	const int t = p.thickness;
	const int half = t / 2;

	// the t x t center block; bars start "gap" pixels outside of it
	const int c0x = cx - half;
	const int c0y = cy - half;

	std::vector<RECT> fills;

	if (p.length > 0)
	{
		RECT left = {c0x - p.gap - p.length, c0y, p.length, t, false};
		RECT right = {c0x + t + p.gap, c0y, p.length, t, false};
		RECT top = {c0x, c0y - p.gap - p.length, t, p.length, false};
		RECT bottom = {c0x, c0y + t + p.gap, t, p.length, false};

		fills.push_back(left);
		fills.push_back(right);
		fills.push_back(top);
		fills.push_back(bottom);
	}

	if (p.dot)
	{
		RECT dot = {c0x, c0y, t, t, false};
		fills.push_back(dot);
	}

	std::vector<RECT> out;

	if (p.outline != OUTLINE_NONE)
	{
		for (size_t i=0; i<fills.size(); i++)
		{
			addOutline(out, fills[i], p.outline);
		}
	}

	for (size_t i=0; i<fills.size(); i++)
	{
		out.push_back(fills[i]);
	}

	return out;
}
