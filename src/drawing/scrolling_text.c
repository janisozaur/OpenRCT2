#pragma region Copyright (c) 2014-2016 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include "../addresses.h"
#include "../config.h"
#include "../interface/colour.h"
#include "../localisation/localisation.h"
#include "drawing.h"

#pragma pack(push, 1)
/* size: 0xA12 */
typedef struct rct_draw_scroll_text {
	rct_string_id string_id;	// 0x00
	uint32 string_args_0;		// 0x02
	uint32 string_args_1;		// 0x06
	uint16 position;			// 0x0A
	uint16 mode;				// 0x0C
	uint32 id;					// 0x0E
	uint8 bitmap[64 * 8 * 5];	// 0x12
} rct_draw_scroll_text;
assert_struct_size(rct_draw_scroll_text, 0xA12);
#pragma pack(pop)

rct_draw_scroll_text *gDrawScrollTextList = RCT2_ADDRESS(RCT2_ADDRESS_DRAW_SCROLL_LIST, rct_draw_scroll_text);
uint8 *gCharacterBitmaps = RCT2_ADDRESS(RCT2_ADDRESS_CHARACTER_BITMAP, uint8);

void scrolling_text_set_bitmap_for_sprite(utf8 *text, int scroll, uint8 *bitmap, const rct_xy8 *scrollPositionOffsets);
void scrolling_text_set_bitmap_for_ttf(utf8 *text, int scroll, uint8 *bitmap, const rct_xy8 *scrollPositionOffsets);

void scrolling_text_initialise_bitmaps()
{
	uint8 drawingSurface[64];
	rct_drawpixelinfo dpi = {
		.bits = (uint8 *)&drawingSurface,
		.x = 0,
		.y = 0,
		.width = 8,
		.height = 8,
		.pitch = 0,
		.zoom_level = 0
	};


	for (int i = 0; i < 224; i++) {
		memset(drawingSurface, 0, sizeof(drawingSurface));
		gfx_draw_sprite_software(&dpi, i + 0x10D5, -1, 0, 0);

		for (int x = 0; x < 8; x++) {
			uint8 val = 0;
			for (int y = 0; y < 8; y++) {
				val >>= 1;
				if (dpi.bits[x + y * 8] == 1) {
					val |= 0x80;
				}
			}
			gCharacterBitmaps[i * 8 + x] = val;
		}

	}
}

static uint8 *font_sprite_get_codepoint_bitmap(int codepoint)
{
	return &gCharacterBitmaps[font_sprite_get_codepoint_offset(codepoint) * 8];
}


static int scrolling_text_get_matching_or_oldest(rct_string_id stringId, uint16 scroll, uint16 scrollingMode)
{
	uint32 oldestId = 0xFFFFFFFF;
	int scrollIndex = -1;
	rct_draw_scroll_text* oldestScroll = NULL;
	for (int i = 0; i < 32; i++) {
		rct_draw_scroll_text *scrollText = &gDrawScrollTextList[i];
		if (oldestId >= scrollText->id) {
			oldestId = scrollText->id;
			scrollIndex = i;
			oldestScroll = scrollText;
		}

		// If exact match return the matching index
		uint32 stringArgs0, stringArgs1;
		memcpy(&stringArgs0, gCommonFormatArgs + 0, sizeof(uint32));
		memcpy(&stringArgs1, gCommonFormatArgs + 4, sizeof(uint32));
		if (
			scrollText->string_id == stringId &&
			scrollText->string_args_0 == stringArgs0 &&
			scrollText->string_args_1 == stringArgs1 &&
			scrollText->position == scroll &&
			scrollText->mode == scrollingMode
		) {
			scrollText->id = RCT2_GLOBAL(RCT2_ADDRESS_DRAW_SCROLL_NEXT_ID, uint32);
			return i + 0x606;
		}
	}
	return scrollIndex;
}

static uint8 scrolling_text_get_colour(uint32 character)
{
	int colour = character & 0x7F;
	if (character & (1 << 7)) {
		return ColourMapA[colour].light;
	} else {
		return ColourMapA[colour].mid_dark;
	}
}

static void scrolling_text_format(utf8 *dst, rct_draw_scroll_text *scrollText)
{
	if (gConfigGeneral.upper_case_banners) {
		format_string_to_upper(dst, scrollText->string_id, &scrollText->string_args_0);
	} else {
		format_string(dst, scrollText->string_id, &scrollText->string_args_0);
	}
}

extern bool TempForScrollText;

static const rct_xy8 _scrollpos0[] = {
	{ 35, 12},
	{ 36, 12},
	{ 37, 11},
	{ 38, 11},
	{ 39, 10},
	{ 40, 10},
	{ 41,  9},
	{ 42,  9},
	{ 43,  8},
	{ 44,  8},
	{ 45,  7},
	{ 46,  7},
	{ 47,  6},
	{ 48,  6},
	{ 49,  5},
	{ 50,  5},
	{ 51,  4},
	{ 52,  4},
	{ 53,  3},
	{ 54,  3},
	{ 55,  2},
	{ 56,  2},
	{ 57,  1},
	{ 58,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos1[] = {
	{  5,  1},
	{  6,  1},
	{  7,  2},
	{  8,  2},
	{  9,  3},
	{ 10,  3},
	{ 11,  4},
	{ 12,  4},
	{ 13,  5},
	{ 14,  5},
	{ 15,  6},
	{ 16,  6},
	{ 17,  7},
	{ 18,  7},
	{ 19,  8},
	{ 20,  8},
	{ 21,  9},
	{ 22,  9},
	{ 23, 10},
	{ 24, 10},
	{ 25, 11},
	{ 26, 11},
	{ 27, 12},
	{ 28, 12},
	{ -1, -1},
};
static const rct_xy8 _scrollpos2[] = {
	{ 12,  1},
	{ 13,  1},
	{ 14,  2},
	{ 15,  2},
	{ 16,  3},
	{ 17,  3},
	{ 18,  4},
	{ 19,  4},
	{ 20,  5},
	{ 21,  5},
	{ 22,  6},
	{ 23,  6},
	{ 24,  7},
	{ 25,  7},
	{ 26,  8},
	{ 27,  8},
	{ 28,  9},
	{ 29,  9},
	{ 30, 10},
	{ 31, 10},
	{ 32, 10},
	{ 33,  9},
	{ 34,  9},
	{ 35,  8},
	{ 36,  8},
	{ 37,  7},
	{ 38,  7},
	{ 39,  6},
	{ 40,  6},
	{ 41,  5},
	{ 42,  5},
	{ 43,  4},
	{ 44,  4},
	{ 45,  3},
	{ 46,  3},
	{ 47,  2},
	{ 48,  2},
	{ 49,  1},
	{ 50,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos3[] = {
	{ 16,  0},
	{ 17,  1},
	{ 18,  1},
	{ 19,  2},
	{ 20,  3},
	{ 21,  3},
	{ 22,  4},
	{ 23,  5},
	{ 24,  5},
	{ 25,  6},
	{ 26,  6},
	{ 27,  7},
	{ 28,  7},
	{ 29,  8},
	{ 30,  8},
	{ 31,  9},
	{ 32,  9},
	{ 33, 10},
	{ 34, 10},
	{ 35, 11},
	{ 36, 11},
	{ 37, 12},
	{ 38, 12},
	{ 39, 13},
	{ 40, 13},
	{ 41, 14},
	{ 42, 14},
	{ 43, 14},
	{ 44, 15},
	{ 45, 15},
	{ 46, 15},
	{ 47, 16},
	{ 48, 16},
	{ -1, -1},
};
static const rct_xy8 _scrollpos4[] = {
	{ 15, 17},
	{ 16, 17},
	{ 17, 16},
	{ 18, 16},
	{ 19, 16},
	{ 20, 15},
	{ 21, 15},
	{ 22, 15},
	{ 23, 14},
	{ 24, 14},
	{ 25, 13},
	{ 26, 13},
	{ 27, 12},
	{ 28, 12},
	{ 29, 11},
	{ 30, 11},
	{ 31, 10},
	{ 32, 10},
	{ 33,  9},
	{ 34,  9},
	{ 35,  8},
	{ 36,  8},
	{ 37,  7},
	{ 38,  7},
	{ 39,  6},
	{ 40,  6},
	{ 41,  5},
	{ 42,  4},
	{ 43,  4},
	{ 44,  3},
	{ 45,  2},
	{ 46,  2},
	{ 47,  1},
	{ 48,  0},
	{ -1, -1},
};
static const rct_xy8 _scrollpos5[] = {
	{  4, 12},
	{  5, 12},
	{  6, 11},
	{  7, 11},
	{  8, 10},
	{  9, 10},
	{ 10,  9},
	{ 11,  9},
	{ 12,  8},
	{ 13,  8},
	{ 14,  7},
	{ 15,  7},
	{ 16,  6},
	{ 17,  6},
	{ 18,  5},
	{ 19,  5},
	{ 20,  4},
	{ 21,  4},
	{ 22,  3},
	{ 23,  3},
	{ 24,  2},
	{ 25,  2},
	{ 26,  1},
	{ 27,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos6[] = {
	{ 36,  1},
	{ 37,  1},
	{ 38,  2},
	{ 39,  2},
	{ 40,  3},
	{ 41,  3},
	{ 42,  4},
	{ 43,  4},
	{ 44,  5},
	{ 45,  5},
	{ 46,  6},
	{ 47,  6},
	{ 48,  7},
	{ 49,  7},
	{ 50,  8},
	{ 51,  8},
	{ 52,  9},
	{ 53,  9},
	{ 54, 10},
	{ 55, 10},
	{ 56, 11},
	{ 57, 11},
	{ 58, 12},
	{ 59, 12},
	{ -1, -1},
};
static const rct_xy8 _scrollpos7[] = {
	{  8, 11},
	{  9, 11},
	{ 10, 10},
	{ 11, 10},
	{ 12,  9},
	{ 13,  9},
	{ 14,  8},
	{ 15,  8},
	{ 16,  7},
	{ 17,  7},
	{ 18,  6},
	{ 19,  6},
	{ 20,  5},
	{ 21,  5},
	{ 22,  4},
	{ 23,  4},
	{ 24,  3},
	{ 25,  3},
	{ 26,  2},
	{ 27,  2},
	{ 28,  1},
	{ 29,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos8[] = {
	{ 36,  2},
	{ 37,  2},
	{ 38,  3},
	{ 39,  3},
	{ 40,  4},
	{ 41,  4},
	{ 42,  5},
	{ 43,  5},
	{ 44,  6},
	{ 45,  6},
	{ 46,  7},
	{ 47,  7},
	{ 48,  8},
	{ 49,  8},
	{ 50,  9},
	{ 51,  9},
	{ 52, 10},
	{ 53, 10},
	{ 54, 11},
	{ 55, 11},
	{ -1, -1},
};
static const rct_xy8 _scrollpos9[] = {
	{ 11,  9},
	{ 12,  9},
	{ 13,  9},
	{ 14,  9},
	{ 15,  9},
	{ 16,  8},
	{ 17,  8},
	{ 18,  7},
	{ 19,  7},
	{ 20,  7},
	{ 21,  6},
	{ 22,  6},
	{ 23,  5},
	{ 24,  4},
	{ 25,  4},
	{ 26,  3},
	{ 27,  3},
	{ 28,  2},
	{ 29,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos10[] = {
	{ 34,  1},
	{ 35,  2},
	{ 36,  3},
	{ 37,  3},
	{ 38,  4},
	{ 39,  4},
	{ 40,  5},
	{ 41,  6},
	{ 42,  6},
	{ 43,  7},
	{ 44,  7},
	{ 45,  7},
	{ 46,  8},
	{ 47,  8},
	{ 48,  9},
	{ 49,  9},
	{ 50,  9},
	{ 51,  9},
	{ 52,  9},
	{ -1, -1},
};
static const rct_xy8 _scrollpos11[] = {
	{ 14, 10},
	{ 15,  9},
	{ 16,  9},
	{ 17,  8},
	{ 18,  8},
	{ 19,  7},
	{ 20,  7},
	{ 21,  6},
	{ 22,  6},
	{ 23,  5},
	{ 24,  5},
	{ 25,  4},
	{ 26,  4},
	{ 27,  3},
	{ 28,  3},
	{ 29,  2},
	{ 30,  2},
	{ 31,  1},
	{ 32,  1},
	{ 33,  0},
	{ 34,  0},
	{ -1, -1},
};
static const rct_xy8 _scrollpos12[] = {
	{ 33,  1},
	{ 34,  2},
	{ 35,  2},
	{ 36,  3},
	{ 37,  3},
	{ 38,  4},
	{ 39,  4},
	{ 40,  5},
	{ 41,  5},
	{ 42,  6},
	{ 43,  6},
	{ 44,  7},
	{ 45,  7},
	{ 46,  8},
	{ 47,  8},
	{ 48,  9},
	{ 49,  9},
	{ 50, 10},
	{ 51, 10},
	{ 52, 11},
	{ 53, 11},
	{ -1, -1},
};
static const rct_xy8 _scrollpos13[] = {
	{ 12, 11},
	{ 13, 10},
	{ 14, 10},
	{ 15,  9},
	{ 16,  9},
	{ 17,  8},
	{ 18,  8},
	{ 19,  7},
	{ 20,  7},
	{ 21,  6},
	{ 22,  6},
	{ 23,  5},
	{ 24,  5},
	{ 25,  4},
	{ 26,  4},
	{ 27,  3},
	{ 28,  3},
	{ 29,  2},
	{ 30,  2},
	{ 31,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos14[] = {
	{ 33,  1},
	{ 34,  2},
	{ 35,  2},
	{ 36,  3},
	{ 37,  3},
	{ 38,  4},
	{ 39,  4},
	{ 40,  5},
	{ 41,  5},
	{ 42,  6},
	{ 43,  6},
	{ 44,  7},
	{ 45,  7},
	{ 46,  8},
	{ 47,  8},
	{ 48,  9},
	{ 49,  9},
	{ 50, 10},
	{ 51, 10},
	{ 52, 11},
	{ 53, 11},
	{ -1, -1},
};
static const rct_xy8 _scrollpos15[] = {
	{ 10, 10},
	{ 11, 10},
	{ 12,  9},
	{ 13,  9},
	{ 14,  8},
	{ 15,  8},
	{ 16,  7},
	{ 17,  7},
	{ 18,  6},
	{ 19,  6},
	{ 20,  5},
	{ 21,  5},
	{ 22,  4},
	{ 23,  4},
	{ 24,  3},
	{ 25,  3},
	{ 26,  2},
	{ 27,  2},
	{ 28,  1},
	{ 29,  1},
	{ 30,  0},
	{ 31,  0},
	{ -1, -1},
};
static const rct_xy8 _scrollpos16[] = {
	{ 33,  0},
	{ 34,  0},
	{ 35,  1},
	{ 36,  1},
	{ 37,  2},
	{ 38,  2},
	{ 39,  3},
	{ 40,  3},
	{ 41,  4},
	{ 42,  4},
	{ 43,  5},
	{ 44,  5},
	{ 45,  6},
	{ 46,  6},
	{ 47,  7},
	{ 48,  7},
	{ 49,  8},
	{ 50,  8},
	{ 51,  9},
	{ 52,  9},
	{ 53, 10},
	{ 54, 10},
	{ -1, -1},
};
static const rct_xy8 _scrollpos17[] = {
	{  6, 11},
	{  7, 11},
	{  8, 10},
	{  9, 10},
	{ 10,  9},
	{ 11,  9},
	{ 12,  8},
	{ 13,  8},
	{ 14,  7},
	{ 15,  7},
	{ 16,  6},
	{ 17,  6},
	{ 18,  5},
	{ 19,  5},
	{ 20,  4},
	{ 21,  4},
	{ 22,  3},
	{ 23,  3},
	{ 24,  2},
	{ 25,  2},
	{ 26,  1},
	{ 27,  1},
	{ 28,  0},
	{ 29,  0},
	{ -1, -1},
};
static const rct_xy8 _scrollpos18[] = {
	{ 34,  0},
	{ 35,  0},
	{ 36,  1},
	{ 37,  1},
	{ 38,  2},
	{ 39,  2},
	{ 40,  3},
	{ 41,  3},
	{ 42,  4},
	{ 43,  4},
	{ 44,  5},
	{ 45,  5},
	{ 46,  6},
	{ 47,  6},
	{ 48,  7},
	{ 49,  7},
	{ 50,  8},
	{ 51,  8},
	{ 52,  9},
	{ 53,  9},
	{ 54, 10},
	{ 55, 10},
	{ 56, 11},
	{ 57, 11},
	{ -1, -1},
};
static const rct_xy8 _scrollpos19[] = {
	{ 13,  1},
	{ 14,  1},
	{ 15,  2},
	{ 16,  2},
	{ 17,  3},
	{ 18,  3},
	{ 19,  4},
	{ 20,  4},
	{ 21,  5},
	{ 22,  5},
	{ 23,  6},
	{ 24,  6},
	{ 25,  7},
	{ 26,  7},
	{ 27,  8},
	{ 28,  8},
	{ 29,  9},
	{ 30,  9},
	{ 31, 10},
	{ 32, 10},
	{ 33, 10},
	{ 34,  9},
	{ 35,  9},
	{ 36,  8},
	{ 37,  8},
	{ 38,  7},
	{ 39,  7},
	{ 40,  6},
	{ 41,  6},
	{ 42,  5},
	{ 43,  5},
	{ 44,  4},
	{ 45,  4},
	{ 46,  3},
	{ 47,  3},
	{ 48,  2},
	{ 49,  2},
	{ 50,  1},
	{ 51,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos20[] = {
	{ 12,  1},
	{ 13,  3},
	{ 14,  4},
	{ 15,  5},
	{ 16,  6},
	{ 17,  7},
	{ 18,  7},
	{ 19,  8},
	{ 20,  8},
	{ 21,  9},
	{ 22,  9},
	{ 23,  9},
	{ 24, 10},
	{ 25, 10},
	{ 26, 10},
	{ 27, 10},
	{ 28, 10},
	{ 29, 10},
	{ 30, 10},
	{ 31, 10},
	{ 32, 10},
	{ 33, 10},
	{ 34, 10},
	{ 35, 10},
	{ 36, 10},
	{ 37, 10},
	{ 38, 10},
	{ 39,  9},
	{ 40,  9},
	{ 41,  9},
	{ 42,  8},
	{ 43,  8},
	{ 44,  7},
	{ 45,  7},
	{ 46,  6},
	{ 47,  5},
	{ 48,  4},
	{ 49,  3},
	{ -1, -1},
};
static const rct_xy8 _scrollpos21[] = {
	{ 12,  1},
	{ 13,  1},
	{ 14,  2},
	{ 15,  2},
	{ 16,  3},
	{ 17,  3},
	{ 18,  4},
	{ 19,  4},
	{ 20,  5},
	{ 21,  5},
	{ 22,  6},
	{ 23,  6},
	{ 24,  7},
	{ 25,  7},
	{ 26,  8},
	{ 27,  8},
	{ 28,  9},
	{ 29,  9},
	{ 30, 10},
	{ 31, 10},
	{ 32, 10},
	{ 33,  9},
	{ 34,  9},
	{ 35,  8},
	{ 36,  8},
	{ 37,  7},
	{ 38,  7},
	{ 39,  6},
	{ 40,  6},
	{ 41,  5},
	{ 42,  5},
	{ 43,  4},
	{ 44,  4},
	{ 45,  3},
	{ 46,  3},
	{ 47,  2},
	{ 48,  2},
	{ 49,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos22[] = {
	{ 16,  1},
	{ 17,  1},
	{ 18,  2},
	{ 19,  2},
	{ 20,  3},
	{ 21,  3},
	{ 22,  4},
	{ 23,  4},
	{ 24,  5},
	{ 25,  5},
	{ 26,  6},
	{ 27,  6},
	{ 28,  6},
	{ 29,  6},
	{ 30,  6},
	{ 31,  6},
	{ 32,  6},
	{ 33,  6},
	{ 34,  6},
	{ 35,  6},
	{ 36,  6},
	{ 37,  6},
	{ 38,  6},
	{ 39,  5},
	{ 40,  5},
	{ 41,  4},
	{ 42,  4},
	{ 43,  3},
	{ 44,  3},
	{ 45,  2},
	{ 46,  2},
	{ 47,  1},
	{ 48,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos23[] = {
	{ 15,  1},
	{ 16,  2},
	{ 17,  2},
	{ 18,  3},
	{ 19,  4},
	{ 20,  5},
	{ 21,  5},
	{ 22,  5},
	{ 23,  6},
	{ 24,  6},
	{ 25,  6},
	{ 26,  6},
	{ 27,  7},
	{ 28,  7},
	{ 29,  7},
	{ 30,  7},
	{ 31,  7},
	{ 32,  7},
	{ 33,  7},
	{ 34,  7},
	{ 35,  7},
	{ 36,  7},
	{ 37,  6},
	{ 38,  6},
	{ 39,  6},
	{ 40,  6},
	{ 41,  5},
	{ 42,  5},
	{ 43,  5},
	{ 44,  4},
	{ 45,  3},
	{ 46,  3},
	{ 47,  2},
	{ 48,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos24[] = {
	{  8,  9},
	{  9,  9},
	{ 10,  8},
	{ 11,  8},
	{ 12,  7},
	{ 13,  7},
	{ 14,  6},
	{ 15,  6},
	{ 16,  5},
	{ 17,  5},
	{ 18,  4},
	{ 19,  4},
	{ 20,  3},
	{ 21,  3},
	{ 22,  2},
	{ 23,  2},
	{ 24,  1},
	{ 25,  1},
	{ 26,  0},
	{ 27,  0},
	{ -1, -1},
};
static const rct_xy8 _scrollpos25[] = {
	{ 36,  0},
	{ 37,  0},
	{ 38,  1},
	{ 39,  1},
	{ 40,  2},
	{ 41,  2},
	{ 42,  3},
	{ 43,  3},
	{ 44,  4},
	{ 45,  4},
	{ 46,  5},
	{ 47,  5},
	{ 48,  6},
	{ 49,  6},
	{ 50,  7},
	{ 51,  7},
	{ 52,  8},
	{ 53,  8},
	{ 54,  9},
	{ 55,  9},
	{ -1, -1},
};
static const rct_xy8 _scrollpos26[] = {
	{  4, 13},
	{  5, 13},
	{  6, 12},
	{  7, 12},
	{  8, 11},
	{  9, 11},
	{ 10, 10},
	{ 11, 10},
	{ 12,  9},
	{ 13,  9},
	{ 14,  8},
	{ 15,  8},
	{ 16,  7},
	{ 17,  7},
	{ 18,  6},
	{ 19,  6},
	{ 20,  5},
	{ 21,  5},
	{ 22,  4},
	{ 23,  4},
	{ 24,  3},
	{ 25,  3},
	{ 26,  2},
	{ 27,  2},
	{ 28,  1},
	{ 29,  1},
	{ 30,  0},
	{ 31,  0},
	{ -1, -1},
};
static const rct_xy8 _scrollpos27[] = {
	{ 32,  0},
	{ 33,  0},
	{ 34,  1},
	{ 35,  1},
	{ 36,  2},
	{ 37,  2},
	{ 38,  3},
	{ 39,  3},
	{ 40,  4},
	{ 41,  4},
	{ 42,  5},
	{ 43,  5},
	{ 44,  6},
	{ 45,  6},
	{ 46,  7},
	{ 47,  7},
	{ 48,  8},
	{ 49,  8},
	{ 50,  9},
	{ 51,  9},
	{ 52, 10},
	{ 53, 10},
	{ 54, 11},
	{ 55, 11},
	{ 56, 12},
	{ 57, 12},
	{ 58, 13},
	{ 59, 13},
	{ -1, -1},
};
static const rct_xy8 _scrollpos28[] = {
	{  6, 13},
	{  7, 13},
	{  8, 12},
	{  9, 12},
	{ 10, 11},
	{ 11, 11},
	{ 12, 10},
	{ 13, 10},
	{ 14,  9},
	{ 15,  9},
	{ 16,  8},
	{ 17,  8},
	{ 18,  7},
	{ 19,  7},
	{ 20,  6},
	{ 21,  6},
	{ 22,  5},
	{ 23,  5},
	{ 24,  4},
	{ 25,  4},
	{ 26,  3},
	{ 27,  3},
	{ 28,  2},
	{ 29,  2},
	{ 30,  1},
	{ 31,  1},
	{ 32,  0},
	{ 33,  0},
	{ -1, -1},
};
static const rct_xy8 _scrollpos29[] = {
	{ 30,  0},
	{ 31,  0},
	{ 32,  1},
	{ 33,  1},
	{ 34,  2},
	{ 35,  2},
	{ 36,  3},
	{ 37,  3},
	{ 38,  4},
	{ 39,  4},
	{ 40,  5},
	{ 41,  5},
	{ 42,  6},
	{ 43,  6},
	{ 44,  7},
	{ 45,  7},
	{ 46,  8},
	{ 47,  8},
	{ 48,  9},
	{ 49,  9},
	{ 50, 10},
	{ 51, 10},
	{ 52, 11},
	{ 53, 11},
	{ 54, 12},
	{ 55, 12},
	{ 56, 13},
	{ 57, 13},
	{ -1, -1},
};
static const rct_xy8 _scrollpos30[] = {
	{  2, 30},
	{  3, 30},
	{  4, 29},
	{  5, 29},
	{  6, 28},
	{  7, 28},
	{  8, 27},
	{  9, 27},
	{ 10, 26},
	{ 11, 26},
	{ 12, 25},
	{ 13, 25},
	{ 14, 24},
	{ 15, 24},
	{ 16, 23},
	{ 17, 23},
	{ 18, 22},
	{ 19, 22},
	{ 20, 21},
	{ 21, 21},
	{ 22, 20},
	{ 23, 20},
	{ 24, 19},
	{ 25, 19},
	{ 26, 18},
	{ 27, 18},
	{ 28, 17},
	{ 29, 17},
	{ 30, 16},
	{ 31, 16},
	{ 32, 15},
	{ 33, 15},
	{ 34, 14},
	{ 35, 14},
	{ 36, 13},
	{ 37, 13},
	{ 38, 12},
	{ 39, 12},
	{ 40, 11},
	{ 41, 11},
	{ 42, 10},
	{ 43, 10},
	{ 44,  9},
	{ 45,  9},
	{ 46,  8},
	{ 47,  8},
	{ 48,  7},
	{ 49,  7},
	{ 50,  6},
	{ 51,  6},
	{ 52,  5},
	{ 53,  5},
	{ 54,  4},
	{ 55,  4},
	{ 56,  3},
	{ 57,  3},
	{ 58,  2},
	{ 59,  2},
	{ 60,  1},
	{ 61,  1},
	{ 62,  0},
	{ -1, -1},
};
static const rct_xy8 _scrollpos31[] = {
	{  1,  0},
	{  2,  1},
	{  3,  1},
	{  4,  2},
	{  5,  2},
	{  6,  3},
	{  7,  3},
	{  8,  4},
	{  9,  4},
	{ 10,  5},
	{ 11,  5},
	{ 12,  6},
	{ 13,  6},
	{ 14,  7},
	{ 15,  7},
	{ 16,  8},
	{ 17,  8},
	{ 18,  9},
	{ 19,  9},
	{ 20, 10},
	{ 21, 10},
	{ 22, 11},
	{ 23, 11},
	{ 24, 12},
	{ 25, 12},
	{ 26, 13},
	{ 27, 13},
	{ 28, 14},
	{ 29, 14},
	{ 30, 15},
	{ 31, 15},
	{ 32, 16},
	{ 33, 16},
	{ 34, 17},
	{ 35, 17},
	{ 36, 18},
	{ 37, 18},
	{ 38, 19},
	{ 39, 19},
	{ 40, 20},
	{ 41, 20},
	{ 42, 21},
	{ 43, 21},
	{ 44, 22},
	{ 45, 22},
	{ 46, 23},
	{ 47, 23},
	{ 48, 24},
	{ 49, 24},
	{ 50, 25},
	{ 51, 25},
	{ 52, 26},
	{ 53, 26},
	{ 54, 27},
	{ 55, 27},
	{ 56, 28},
	{ 57, 28},
	{ 58, 29},
	{ 59, 29},
	{ 60, 30},
	{ 61, 30},
	{ -1, -1},
};
static const rct_xy8 _scrollpos32[] = {
	{ 12,  0},
	{ 13,  1},
	{ 14,  1},
	{ 15,  2},
	{ 16,  2},
	{ 17,  3},
	{ 18,  3},
	{ 19,  4},
	{ 20,  4},
	{ 21,  5},
	{ 22,  5},
	{ 23,  6},
	{ 24,  6},
	{ 25,  7},
	{ 26,  7},
	{ 27,  8},
	{ 28,  8},
	{ 29,  9},
	{ 30,  9},
	{ 31, 10},
	{ 32, 10},
	{ 33, 11},
	{ 34, 11},
	{ 35, 12},
	{ 36, 12},
	{ 37, 13},
	{ 38, 13},
	{ 39, 14},
	{ 40, 14},
	{ 41, 15},
	{ 42, 15},
	{ 43, 16},
	{ 44, 16},
	{ 45, 17},
	{ 46, 17},
	{ 47, 18},
	{ 48, 18},
	{ 49, 19},
	{ 50, 19},
	{ -1, -1},
};
static const rct_xy8 _scrollpos33[] = {
	{ 12, 20},
	{ 13, 20},
	{ 14, 19},
	{ 15, 19},
	{ 16, 18},
	{ 17, 18},
	{ 18, 17},
	{ 19, 17},
	{ 20, 16},
	{ 21, 16},
	{ 22, 15},
	{ 23, 15},
	{ 24, 14},
	{ 25, 14},
	{ 26, 13},
	{ 27, 13},
	{ 28, 12},
	{ 29, 12},
	{ 30, 11},
	{ 31, 11},
	{ 32, 10},
	{ 33, 10},
	{ 34,  9},
	{ 35,  9},
	{ 36,  8},
	{ 37,  8},
	{ 38,  7},
	{ 39,  7},
	{ 40,  6},
	{ 41,  6},
	{ 42,  5},
	{ 43,  5},
	{ 44,  4},
	{ 45,  4},
	{ 46,  3},
	{ 47,  3},
	{ 48,  2},
	{ 49,  2},
	{ 50,  1},
	{ 51,  1},
	{ -1, -1},
};
static const rct_xy8 _scrollpos34[] = {
	{  2, 14},
	{  3, 14},
	{  4, 13},
	{  5, 13},
	{  6, 12},
	{  7, 12},
	{  8, 11},
	{  9, 11},
	{ 10, 10},
	{ 11, 10},
	{ 12,  9},
	{ 13,  9},
	{ 14,  8},
	{ 15,  8},
	{ 16,  7},
	{ 17,  7},
	{ 18,  6},
	{ 19,  6},
	{ 20,  5},
	{ 21,  5},
	{ 22,  4},
	{ 23,  4},
	{ 24,  3},
	{ 25,  3},
	{ 26,  2},
	{ 27,  2},
	{ 28,  1},
	{ 29,  1},
	{ 30,  0},
	{ -1, -1},
};
static const rct_xy8 _scrollpos35[] = {
	{ 33,  0},
	{ 34,  0},
	{ 35,  1},
	{ 36,  1},
	{ 37,  2},
	{ 38,  2},
	{ 39,  3},
	{ 40,  3},
	{ 41,  4},
	{ 42,  4},
	{ 43,  5},
	{ 44,  5},
	{ 45,  6},
	{ 46,  6},
	{ 47,  7},
	{ 48,  7},
	{ 49,  8},
	{ 50,  8},
	{ 51,  9},
	{ 52,  9},
	{ 53, 10},
	{ 54, 10},
	{ 55, 11},
	{ 56, 11},
	{ 57, 12},
	{ 58, 12},
	{ 59, 13},
	{ 60, 13},
	{ 61, 14},
	{ -1, -1},
};
static const rct_xy8 _scrollpos36[] = {
	{  4,  0},
	{  5,  1},
	{  6,  2},
	{  7,  3},
	{  8,  3},
	{  9,  4},
	{ 10,  5},
	{ 11,  5},
	{ 12,  6},
	{ 13,  6},
	{ 14,  7},
	{ 15,  7},
	{ 16,  8},
	{ 17,  8},
	{ 18,  9},
	{ 19,  9},
	{ 20, 10},
	{ 21, 10},
	{ 22, 10},
	{ 23, 11},
	{ 24, 11},
	{ 25, 11},
	{ 26, 12},
	{ 27, 12},
	{ 28, 12},
	{ 29, 12},
	{ 30, 12},
	{ -1, -1},
};
static const rct_xy8 _scrollpos37[] = {
	{ 32, 13},
	{ 33, 12},
	{ 34, 12},
	{ 35, 12},
	{ 36, 12},
	{ 37, 11},
	{ 38, 11},
	{ 39, 11},
	{ 40, 10},
	{ 41, 10},
	{ 42, 10},
	{ 43,  9},
	{ 44,  9},
	{ 45,  8},
	{ 46,  8},
	{ 47,  7},
	{ 48,  7},
	{ 49,  6},
	{ 50,  6},
	{ 51,  5},
	{ 52,  5},
	{ 53,  4},
	{ 54,  3},
	{ 55,  3},
	{ 56,  2},
	{ 57,  1},
	{ 58,  0},
	{ -1, -1},
};

static const rct_xy8* _scrollPositions[38] = {
	_scrollpos0,
	_scrollpos1,
	_scrollpos2,
	_scrollpos3,
	_scrollpos4,
	_scrollpos5,
	_scrollpos6,
	_scrollpos7,
	_scrollpos8,
	_scrollpos9,
	_scrollpos10,
	_scrollpos11,
	_scrollpos12,
	_scrollpos13,
	_scrollpos14,
	_scrollpos15,
	_scrollpos16,
	_scrollpos17,
	_scrollpos18,
	_scrollpos19,
	_scrollpos20,
	_scrollpos21,
	_scrollpos22,
	_scrollpos23,
	_scrollpos24,
	_scrollpos25,
	_scrollpos26,
	_scrollpos27,
	_scrollpos28,
	_scrollpos29,
	_scrollpos30,
	_scrollpos31,
	_scrollpos32,
	_scrollpos33,
	_scrollpos34,
	_scrollpos35,
	_scrollpos36,
	_scrollpos37,
};

/**
 *
 *  rct2: 0x006C42D9
 * @param stringId (ax)
 * @param scroll (cx)
 * @param scrollingMode (bp)
 * @returns ebx
 */
int scrolling_text_setup(rct_string_id stringId, uint16 scroll, uint16 scrollingMode)
{
	if (TempForScrollText) {
		memcpy(gCommonFormatArgs, (const void*)0x013CE952, 16);
	}

	rct_drawpixelinfo* dpi = unk_140E9A8;

	if (dpi->zoom_level != 0) return 0x626;

	RCT2_GLOBAL(RCT2_ADDRESS_DRAW_SCROLL_NEXT_ID, uint32)++;

	int scrollIndex = scrolling_text_get_matching_or_oldest(stringId, scroll, scrollingMode);
	if (scrollIndex >= 0x606) return scrollIndex;

	// Setup scrolling text
	uint32 stringArgs0, stringArgs1;
	memcpy(&stringArgs0, gCommonFormatArgs + 0, sizeof(uint32));
	memcpy(&stringArgs1, gCommonFormatArgs + 4, sizeof(uint32));

	rct_draw_scroll_text* scrollText = &gDrawScrollTextList[scrollIndex];
	scrollText->string_id = stringId;
	scrollText->string_args_0 = stringArgs0;
	scrollText->string_args_1 = stringArgs1;
	scrollText->position = scroll;
	scrollText->mode = scrollingMode;
	scrollText->id = RCT2_GLOBAL(RCT2_ADDRESS_DRAW_SCROLL_NEXT_ID, uint32);

	// Create the string to draw
	utf8 scrollString[256];
	scrolling_text_format(scrollString, scrollText);

	const rct_xy8* scrollingModePositions = _scrollPositions[scrollingMode];

	memset(scrollText->bitmap, 0, 320 * 8);
	if (gUseTrueTypeFont) {
		scrolling_text_set_bitmap_for_ttf(scrollString, scroll, scrollText->bitmap, scrollingModePositions);
	} else {
		scrolling_text_set_bitmap_for_sprite(scrollString, scroll, scrollText->bitmap, scrollingModePositions);
	}

	uint32 imageId = 0x606 + scrollIndex;
	drawing_engine_invalidate_image(imageId);
	return imageId;
}

void scrolling_text_set_bitmap_for_sprite(utf8 *text, int scroll, uint8 *bitmap, const rct_xy8 *scrollPositionOffsets)
{
	uint8 characterColour = scrolling_text_get_colour(gCommonFormatArgs[7]);

	utf8 *ch = text;
	while (true) {
		uint32 codepoint = utf8_get_next(ch, (const utf8**)&ch);

		// If at the end of the string loop back to the start
		if (codepoint == 0) {
			ch = text;
			continue;
		}

		// Set any change in colour
		if (codepoint <= FORMAT_COLOUR_CODE_END && codepoint >= FORMAT_COLOUR_CODE_START){
			codepoint -= FORMAT_COLOUR_CODE_START;
			characterColour = g1Elements[4914].offset[codepoint * 4];
			continue;
		}

		// If another type of control character ignore
		if (codepoint < 32) continue;

		int characterWidth = font_sprite_get_codepoint_width(FONT_SPRITE_BASE_TINY, codepoint);
		uint8 *characterBitmap = font_sprite_get_codepoint_bitmap(codepoint);
		for (; characterWidth != 0; characterWidth--, characterBitmap++) {
			// Skip any none displayed columns
			if (scroll != 0) {
				scroll--;
				continue;
			}

			rct_xy8 scrollPosition = *scrollPositionOffsets;
			if (scrollPosition.xy == (uint16)-1) return;
			if ((sint16)scrollPosition.xy > -1) {
				uint8 *dst = &bitmap[scrollPosition.xy];
				for (uint8 char_bitmap = *characterBitmap; char_bitmap != 0; char_bitmap >>= 1){
					if (char_bitmap & 1) *dst = characterColour;

					// Jump to next row
					dst += 64;
				}
			}
			scrollPositionOffsets++;
		}
	}
}

void scrolling_text_set_bitmap_for_ttf(utf8 *text, int scroll, uint8 *bitmap, const rct_xy8 *scrollPositionOffsets)
{
	TTFFontDescriptor *fontDesc = ttf_get_font_from_sprite_base(FONT_SPRITE_BASE_TINY);
	if (fontDesc->font == NULL) {
		scrolling_text_set_bitmap_for_sprite(text, scroll, bitmap, scrollPositionOffsets);
		return;
	}

	// Currently only supports one colour
	uint8 colour = 0;

	utf8 *dstCh = text;
	utf8 *ch = text;
	int codepoint;
	while ((codepoint = utf8_get_next(ch, (const utf8**)&ch)) != 0) {
		if (utf8_is_format_code(codepoint)) {
			if (codepoint >= FORMAT_COLOUR_CODE_START && codepoint <= FORMAT_COLOUR_CODE_END) {
				colour = (uint8)codepoint;
			}
		} else {
			dstCh = utf8_write_codepoint(dstCh, codepoint);
		}
	}
	*dstCh = 0;

	if (colour == 0) {
		colour = scrolling_text_get_colour(gCommonFormatArgs[7]);
	} else {
		colour = RCT2_GLOBAL(0x009FF048, uint8*)[(colour - FORMAT_COLOUR_CODE_START) * 4];
	}

	SDL_Surface *surface = ttf_surface_cache_get_or_add(fontDesc->font, text);
	if (surface == NULL) {
		return;
	}

	if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) == -1) {
		return;
	}

	int pitch = surface->pitch;
	int width = surface->w;
	int height = surface->h;
	uint8 *src = surface->pixels;

	// Offset
	height -= 3;
	src += 3 * pitch;
	height = min(height, 8);

	int x = 0;
	while (true) {
		// Skip any none displayed columns
		if (scroll == 0) {
			rct_xy8 scrollPosition = *scrollPositionOffsets;
			if (scrollPosition.xy == (uint16)-1) return;
			if ((sint16)scrollPosition.xy > -1) {
				uint8 *dst = &bitmap[scrollPosition.xy];

				for (int y = 0; y < height; y++) {
					if (src[y * pitch + x] != 0) *dst = colour;

					// Jump to next row
					dst += 64;
				}
			}
			scrollPositionOffsets++;
		} else {
			scroll--;
		}

		x++;
		if (x >= width) x = 0;
	}

	if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
}
