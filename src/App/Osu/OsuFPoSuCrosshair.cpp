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

	// CS2 gap semantics: gap = distance (in pixels) from the center pixel to the innermost bar pixel,
	// so gap 1 leaves only the center block empty, gap 0 pulls the bars into the center (solid cross).
	// For thick bars the same rule is applied from the edge of the center block.
	const int inset = (p.gap > 0 ? p.gap - 1 : 0);				// empty pixels between the center block and a bar
	const int barLength = (p.gap > 0 ? p.length : p.length - 1);	// gap 0: the innermost bar pixel is the center block itself
	const bool fillCenter = (p.dot || (p.gap == 0 && p.length > 0));

	if (barLength > 0)
	{
		RECT left = {c0x - inset - barLength, c0y, barLength, t, false};
		RECT right = {c0x + t + inset, c0y, barLength, t, false};
		RECT top = {c0x, c0y - inset - barLength, t, barLength, false};
		RECT bottom = {c0x, c0y + t + inset, t, barLength, false};

		fills.push_back(left);
		fills.push_back(right);
		fills.push_back(top);
		fills.push_back(bottom);
	}

	if (fillCenter)
	{
		RECT center = {c0x, c0y, t, t, false};
		fills.push_back(center);
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
