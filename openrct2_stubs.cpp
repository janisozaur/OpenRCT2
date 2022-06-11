#include "openrct2/core/String.hpp"
#include "openrct2/drawing/ImageId.hpp"
#include "openrct2/drawing/Text.h"
#include "openrct2/world/Location.hpp"
struct rct_drawpixelinfo;

namespace OpenRCT2
{

}

thread_local char gCommonStringFormatBuffer[512];
void gfx_draw_string(rct_drawpixelinfo* dpi, const ScreenCoordsXY& coords, const_utf8string buffer, TextPaint textPaint)
{
}
struct FontConfiguration
{
    utf8* file_name;
    utf8* font_name;
    int32_t x_offset;
    int32_t y_offset;
    int32_t size_tiny;
    int32_t size_small;
    int32_t size_medium;
    int32_t size_big;
    int32_t height_tiny;
    int32_t height_small;
    int32_t height_medium;
    int32_t height_big;
    bool enable_hinting;
    int32_t hinting_threshold;
};
FontConfiguration gConfigFonts;
uint8_t blendColours(const uint8_t paletteIndex1, const uint8_t paletteIndex2)
{
    return 0;
}
char32_t CodepointView::iterator::GetNextCodepoint(const char* ch, const char** next)
{
    return {};
}

rct_colour_map ColourMapA[COLOUR_COUNT] = {};
int32_t font_get_line_height(FontSpriteBase fontSpriteBase)
{
    return 0;
}
bool LocalisationService_UseTrueTypeFont(OpenRCT2::IContext* context)
{
    return {};
}
uint32_t utf8_get_next(const utf8* char_ptr, const utf8** nextchar_ptr)
{
    return {};
}
namespace OpenRCT2
{
    IContext* GetContext()
    {
        return {};
    }
} // namespace OpenRCT2
uint8_t gTextPalette[0x8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
int main()
{
}
