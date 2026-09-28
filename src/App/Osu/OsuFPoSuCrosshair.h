//================ Copyright (c) 2026, PG & surreal420, All rights reserved. =================//
//
// Purpose:		CS2 style static crosshair geometry for FPoSu (engine independent)
//
// $NoKeywords: $fposuxhair
//===============================================================================//

#ifndef OSUFPOSUCROSSHAIR_H
#define OSUFPOSUCROSSHAIR_H

#include <vector>
#include <string>
#include <cstddef>

// Mirrors the CS2 "Rush Hour" (2026-09-22) pixel based crosshair settings:
// length 0..255 px, thickness 0..31 px (0 draws as 1), gap 0..128 px, RGBA 0..255,
// outline off/full/half (half = only the top-left edges of every bar), optional center dot.
// gap = pixels from the center pixel to the innermost bar pixel (gap 1 = only the center is empty, gap 0 = solid cross).
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

	// CS2 stores the screen height the pixel values were made for and rescales them on other resolutions
	// (2 px at 720p -> 4 px at 1440p). referenceHeight <= 0 disables the scaling.
	static void scaleToScreen(PARAMS &p, int referenceHeight, int screenHeight);

	// cx/cy = the center pixel; with an odd thickness the center pixel is the middle of the cross
	static std::vector<RECT> build(PARAMS p, int cx, int cy);

	// CS2 share codes ("CSGO-xxxxx-xxxxx-xxxxx-xxxxx-xxxxx"), versions 3 and 4 = post "Rush Hour" (2026-09-22).
	// Codes from before that update are version 1 and are NOT compatible (different units and layout).
	// Layout (18 bytes, big endian number encoded as 25 base-57 digits, least significant digit first):
	//   [0] checksum = sum(bytes[1..17]) & 0xff, [1] version, [2] style(4 bits) | recoil<<4 | (v3: outline<<5) | dot<<6 | t<<7,
	//   [3..6] r g b a, [7] gap, [8] length, [9] dynamic spread limit,
	//   [10..13] little endian packed: splitdist(7) | inneralpha(5) | outeralpha(4) | splitratio(7) | thickness(5) | (v4: outline(2)),
	//   [14..15] little endian screen height, [16..17] zero.
	struct SHARECODE
	{
		int version;		// 3 or 4 when decoded; encodeShareCode() writes 4 unless 3 is requested
		int style;			// 0..8 (4 = Static Cross)
		bool recoil;
		bool dot;
		bool tStyle;
		int outline;		// OUTLINE
		int r, g, b, a;
		int gap, length, thickness;
		int spreadLimit;	// 0..255
		int splitDistance;	// 0..127
		int innerAlphaRaw;	// 0..31, value/20
		int outerAlphaRaw;	// 0..15, (value+6)/20
		int splitRatioRaw;	// 0..127, value/100
		int screenHeight;

		SHARECODE();
	};

	enum DECODE_RESULT
	{
		DECODE_OK = 0,
		DECODE_BAD_FORMAT,			// not CSGO-xxxxx-xxxxx-xxxxx-xxxxx-xxxxx, invalid character or out of range
		DECODE_BAD_CHECKSUM,
		DECODE_OLD_VERSION,			// version 1/2 = pre "Rush Hour" code, incompatible
		DECODE_UNSUPPORTED_VERSION	// anything newer than 4
	};

	static DECODE_RESULT decodeShareCode(const std::string &code, SHARECODE &out);
	static std::string encodeShareCode(const SHARECODE &c);

	static PARAMS paramsFromShareCode(const SHARECODE &c);
	static SHARECODE shareCodeFromParams(const PARAMS &p, int screenHeight);
};

#endif
