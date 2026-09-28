//================ Copyright (c) 2026, PG & surreal420, All rights reserved. =================//
//
// Purpose:		CS2 style static crosshair geometry for FPoSu (engine independent)
//
// $NoKeywords: $fposuxhair
//===============================================================================//

#include "OsuFPoSuCrosshair.h"

#include <cmath>

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

void OsuFPoSuCrosshair::scaleToScreen(PARAMS &p, int referenceHeight, int screenHeight)
{
	if (referenceHeight <= 0 || screenHeight <= 0 || referenceHeight == screenHeight) return;

	const double scale = (double)screenHeight / (double)referenceHeight;

	p.thickness = (int)std::floor(p.thickness * scale + 0.5);
	p.length = (int)std::floor(p.length * scale + 0.5);
	p.gap = (int)std::floor(p.gap * scale + 0.5);

	clampParams(p);
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


//**********************//
//	CS2 share codes		//
//**********************//

static const char *SHARECODE_ALPHABET = "ABCDEFGHJKLMNOPQRSTUVWXYZabcdefhijkmnopqrstuvwxyz23456789"; // 57 symbols
static const int SHARECODE_BYTES = 18;
static const int SHARECODE_DIGITS = 25;

OsuFPoSuCrosshair::SHARECODE::SHARECODE()
{
	// CS2 defaults for a static cross
	version = 4;
	style = 4;
	recoil = false;
	dot = false;
	tStyle = false;
	outline = OUTLINE_FULL;
	r = 0;
	g = 255;
	b = 0;
	a = 255;
	gap = 4;
	length = 8;
	thickness = 1;
	spreadLimit = 255;
	splitDistance = 7;
	innerAlphaRaw = 20;
	outerAlphaRaw = 4;
	splitRatioRaw = 35;
	screenHeight = 1080;
}

static int shareCodeDigitValue(char c)
{
	for (int i=0; i<57; i++)
	{
		if (SHARECODE_ALPHABET[i] == c)
			return i;
	}
	return -1;
}

OsuFPoSuCrosshair::DECODE_RESULT OsuFPoSuCrosshair::decodeShareCode(const std::string &input, SHARECODE &out)
{
	// trim
	size_t start = 0;
	size_t end = input.size();
	while (start < end && (input[start] == ' ' || input[start] == '\t' || input[start] == '\r' || input[start] == '\n')) start++;
	while (end > start && (input[end-1] == ' ' || input[end-1] == '\t' || input[end-1] == '\r' || input[end-1] == '\n')) end--;
	const std::string code = input.substr(start, end - start);

	// CSGO-xxxxx-xxxxx-xxxxx-xxxxx-xxxxx
	if (code.size() != 34) return DECODE_BAD_FORMAT;
	{
		const std::string prefix = code.substr(0, 5);
		if (!((prefix[0] == 'C' || prefix[0] == 'c') && (prefix[1] == 'S' || prefix[1] == 's') && (prefix[2] == 'G' || prefix[2] == 'g') && (prefix[3] == 'O' || prefix[3] == 'o') && prefix[4] == '-'))
			return DECODE_BAD_FORMAT;
	}

	char digits[SHARECODE_DIGITS];
	{
		int n = 0;
		for (size_t i=5; i<code.size(); i++)
		{
			const bool isSeparatorPos = ((i - 5) % 6 == 5);
			if (isSeparatorPos)
			{
				if (code[i] != '-') return DECODE_BAD_FORMAT;
			}
			else
			{
				if (n >= SHARECODE_DIGITS) return DECODE_BAD_FORMAT;
				digits[n++] = code[i];
			}
		}
		if (n != SHARECODE_DIGITS) return DECODE_BAD_FORMAT;
	}

	// value = sum(digit[i] * 57^i), evaluated from the most significant (last) digit down, in a big endian byte array
	unsigned char bytes[SHARECODE_BYTES];
	for (int i=0; i<SHARECODE_BYTES; i++) bytes[i] = 0;
	for (int i=SHARECODE_DIGITS-1; i>=0; i--)
	{
		const int d = shareCodeDigitValue(digits[i]);
		if (d < 0) return DECODE_BAD_FORMAT;

		unsigned int carry = (unsigned int)d;
		for (int k=SHARECODE_BYTES-1; k>=0; k--)
		{
			const unsigned int v = (unsigned int)bytes[k] * 57u + carry;
			bytes[k] = (unsigned char)(v & 0xffu);
			carry = v >> 8;
		}
		if (carry != 0) return DECODE_BAD_FORMAT; // > 144 bits
	}

	{
		unsigned int sum = 0;
		for (int i=1; i<SHARECODE_BYTES; i++) sum += bytes[i];
		if ((sum & 0xffu) != bytes[0]) return DECODE_BAD_CHECKSUM;
	}

	const int version = bytes[1];
	if (version < 3) return DECODE_OLD_VERSION;
	if (version > 4) return DECODE_UNSUPPORTED_VERSION;

	const unsigned int packed = (unsigned int)bytes[10] | ((unsigned int)bytes[11] << 8) | ((unsigned int)bytes[12] << 16) | ((unsigned int)bytes[13] << 24);

	SHARECODE c;
	c.version = version;
	c.style = bytes[2] & 0x0f;
	c.recoil = (bytes[2] & 0x10) != 0;
	c.dot = (bytes[2] & 0x40) != 0;
	c.tStyle = (bytes[2] & 0x80) != 0;
	c.outline = (version == 4 ? (int)((packed >> 28) & 3u) : ((bytes[2] & 0x20) != 0 ? OUTLINE_FULL : OUTLINE_NONE));
	c.r = bytes[3];
	c.g = bytes[4];
	c.b = bytes[5];
	c.a = bytes[6];
	c.gap = bytes[7];
	c.length = bytes[8];
	c.spreadLimit = bytes[9];
	c.splitDistance = (int)(packed & 127u);
	c.innerAlphaRaw = (int)((packed >> 7) & 31u);
	c.outerAlphaRaw = (int)((packed >> 12) & 15u);
	c.splitRatioRaw = (int)((packed >> 16) & 127u);
	c.thickness = (int)((packed >> 23) & 31u);
	c.screenHeight = (int)bytes[14] | ((int)bytes[15] << 8);

	if (c.outline > OUTLINE_HALF) c.outline = OUTLINE_NONE;

	out = c;
	return DECODE_OK;
}

std::string OsuFPoSuCrosshair::encodeShareCode(const SHARECODE &c)
{
	const int version = (c.version == 3 ? 3 : 4);
	const int outline = (c.outline == OUTLINE_FULL || c.outline == OUTLINE_HALF ? c.outline : OUTLINE_NONE);

	unsigned char bytes[SHARECODE_BYTES];
	for (int i=0; i<SHARECODE_BYTES; i++) bytes[i] = 0;

	bytes[1] = (unsigned char)version;
	bytes[2] = (unsigned char)((clampInt(c.style, 0, 15)) | (c.recoil ? 0x10 : 0) | (version == 3 && outline != OUTLINE_NONE ? 0x20 : 0) | (c.dot ? 0x40 : 0) | (c.tStyle ? 0x80 : 0));
	bytes[3] = (unsigned char)clampInt(c.r, 0, 255);
	bytes[4] = (unsigned char)clampInt(c.g, 0, 255);
	bytes[5] = (unsigned char)clampInt(c.b, 0, 255);
	bytes[6] = (unsigned char)clampInt(c.a, 0, 255);
	bytes[7] = (unsigned char)clampInt(c.gap, 0, MAX_GAP);
	bytes[8] = (unsigned char)clampInt(c.length, 0, MAX_LENGTH);
	bytes[9] = (unsigned char)clampInt(c.spreadLimit, 0, 255);

	unsigned int packed = 0;
	packed |= (unsigned int)clampInt(c.splitDistance, 0, 127);
	packed |= (unsigned int)clampInt(c.innerAlphaRaw, 0, 31) << 7;
	packed |= (unsigned int)clampInt(c.outerAlphaRaw, 0, 15) << 12;
	packed |= (unsigned int)clampInt(c.splitRatioRaw, 0, 127) << 16;
	packed |= (unsigned int)clampInt(c.thickness, 0, MAX_THICKNESS) << 23;
	if (version == 4)
		packed |= (unsigned int)outline << 28;

	bytes[10] = (unsigned char)(packed & 0xffu);
	bytes[11] = (unsigned char)((packed >> 8) & 0xffu);
	bytes[12] = (unsigned char)((packed >> 16) & 0xffu);
	bytes[13] = (unsigned char)((packed >> 24) & 0xffu);

	const int screenHeight = clampInt(c.screenHeight, 0, 65535);
	bytes[14] = (unsigned char)(screenHeight & 0xff);
	bytes[15] = (unsigned char)((screenHeight >> 8) & 0xff);

	{
		unsigned int sum = 0;
		for (int i=1; i<SHARECODE_BYTES; i++) sum += bytes[i];
		bytes[0] = (unsigned char)(sum & 0xffu);
	}

	// repeated division by 57, least significant digit first
	char digits[SHARECODE_DIGITS];
	for (int i=0; i<SHARECODE_DIGITS; i++)
	{
		unsigned int rem = 0;
		for (int k=0; k<SHARECODE_BYTES; k++)
		{
			const unsigned int v = (rem << 8) | bytes[k];
			bytes[k] = (unsigned char)(v / 57u);
			rem = v % 57u;
		}
		digits[i] = SHARECODE_ALPHABET[rem];
	}

	std::string code = "CSGO";
	for (int i=0; i<SHARECODE_DIGITS; i++)
	{
		if (i % 5 == 0)
			code += '-';
		code += digits[i];
	}
	return code;
}

OsuFPoSuCrosshair::PARAMS OsuFPoSuCrosshair::paramsFromShareCode(const SHARECODE &c)
{
	PARAMS p;
	p.r = c.r;
	p.g = c.g;
	p.b = c.b;
	p.a = c.a;
	p.outline = c.outline;
	p.thickness = c.thickness;
	p.dot = c.dot;
	p.length = c.length;
	p.gap = c.gap;
	clampParams(p);
	return p;
}

OsuFPoSuCrosshair::SHARECODE OsuFPoSuCrosshair::shareCodeFromParams(const PARAMS &p, int screenHeight)
{
	SHARECODE c; // v4 static cross with CS2 defaults for everything McOsu does not model
	c.r = p.r;
	c.g = p.g;
	c.b = p.b;
	c.a = p.a;
	c.outline = p.outline;
	c.thickness = p.thickness;
	c.dot = p.dot;
	c.length = p.length;
	c.gap = p.gap;
	c.screenHeight = screenHeight;
	return c;
}
