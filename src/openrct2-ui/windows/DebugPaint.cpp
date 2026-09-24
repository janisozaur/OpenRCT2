/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/Guard.hpp>
#include <openrct2/drawing/Drawing.Screen.h>
#include <openrct2/drawing/Drawing.String.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Font.h>
#include <openrct2/drawing/Foveation.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/interface/ColourWithFlags.h>
#include <openrct2/localisation/Language.h>
#include <openrct2/localisation/LocalisationService.h>
#include <openrct2/paint/Paint.h>
#include <openrct2/paint/tile_element/Paint.TileElement.h>
#include <openrct2/ui/UiContext.h>
#include <openrct2/ui/WindowManager.h>

namespace OpenRCT2::Ui::Windows
{
    enum WindowDebugPaintWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TOGGLE_SHOW_WIDE_PATHS,
        WIDX_TOGGLE_SHOW_BLOCKED_TILES,
        WIDX_TOGGLE_SHOW_SEGMENT_HEIGHTS,
        WIDX_TOGGLE_SHOW_BOUND_BOXES,
        WIDX_TOGGLE_SHOW_DIRTY_VISUALS,
        WIDX_TOGGLE_STABLE_PAINT_SORT,
        WIDX_TOGGLE_FORCE_REDRAW,
        WIDX_TOGGLE_FOVEATION,
        WIDX_TOGGLE_FOVEATION_CURSOR,
    };

    static constexpr ScreenSize kWindowSize = { 200, 8 + (15 * 9) + 8 };

    // clang-format off
    static constexpr Widget window_debug_paint_widgets[] = {
        makeWidget({0,          0}, kWindowSize,                   WidgetType::frame,    WindowColour::primary                                        ),
        makeWidget({8, 8 + 15 * 0}, {         185,            12}, WidgetType::checkbox, WindowColour::secondary, STR_DEBUG_PAINT_SHOW_WIDE_PATHS     ),
        makeWidget({8, 8 + 15 * 1}, {         185,            12}, WidgetType::checkbox, WindowColour::secondary, STR_DEBUG_PAINT_SHOW_BLOCKED_TILES  ),
        makeWidget({8, 8 + 15 * 2}, {         185,            12}, WidgetType::checkbox, WindowColour::secondary, STR_DEBUG_PAINT_SHOW_SEGMENT_HEIGHTS),
        makeWidget({8, 8 + 15 * 3}, {         185,            12}, WidgetType::checkbox, WindowColour::secondary, STR_DEBUG_PAINT_SHOW_BOUND_BOXES    ),
        makeWidget({8, 8 + 15 * 4}, {         185,            12}, WidgetType::checkbox, WindowColour::secondary, STR_DEBUG_PAINT_SHOW_DIRTY_VISUALS  ),
        makeWidget({8, 8 + 15 * 5}, {         185,            12}, WidgetType::checkbox, WindowColour::secondary, STR_DEBUG_PAINT_STABLE_SORT  ),
        makeWidget({8, 8 + 15 * 6}, {         185,            12}, WidgetType::checkbox, WindowColour::secondary, STR_DEBUG_PAINT_FORCE_REDRAW  ),
        makeWidget({8, 8 + 15 * 7}, {         185,            12}, WidgetType::checkbox, WindowColour::secondary                                       ),
        makeWidget({8, 8 + 15 * 8}, {         185,            12}, WidgetType::checkbox, WindowColour::secondary                                       ),
    };
    // clang-format on

    class DebugPaintWindow final : public Window
    {
    private:
        int32_t ResizeLanguage = LANGUAGE_UNDEFINED;

    public:
        void onOpen() override
        {
            setWidgets(window_debug_paint_widgets);

            initScrollWidgets();
            WindowPushOthersBelow(*this);

            colours[0] = ColourWithFlags{ Drawing::Colour::black }.withFlag(ColourFlag::translucent, true);
            colours[1] = Drawing::Colour::grey;

            ResizeLanguage = LANGUAGE_UNDEFINED;
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_TOGGLE_SHOW_WIDE_PATHS:
                    gPaintWidePathsAsGhost = !gPaintWidePathsAsGhost;
                    Drawing::GfxInvalidateScreen();
                    break;

                case WIDX_TOGGLE_SHOW_BLOCKED_TILES:
                    gPaintBlockedTiles = !gPaintBlockedTiles;
                    Drawing::GfxInvalidateScreen();
                    break;

                case WIDX_TOGGLE_SHOW_SEGMENT_HEIGHTS:
                    gShowSupportSegmentHeights = !gShowSupportSegmentHeights;
                    Drawing::GfxInvalidateScreen();
                    break;

                case WIDX_TOGGLE_SHOW_BOUND_BOXES:
                    gPaintBoundingBoxes = !gPaintBoundingBoxes;
                    Drawing::GfxInvalidateScreen();
                    break;

                case WIDX_TOGGLE_SHOW_DIRTY_VISUALS:
                    gShowDirtyVisuals = !gShowDirtyVisuals;
                    Drawing::GfxInvalidateScreen();
                    break;

                case WIDX_TOGGLE_STABLE_PAINT_SORT:
                    gPaintStableSort = !gPaintStableSort;
                    Drawing::GfxInvalidateScreen();
                    break;

                case WIDX_TOGGLE_FORCE_REDRAW:
                    gPaintForceRedraw = !gPaintForceRedraw;
                    Drawing::GfxInvalidateScreen();
                    break;

                case WIDX_TOGGLE_FOVEATION:
                    Drawing::gFoveatedRenderingSettings.enabled = !Drawing::gFoveatedRenderingSettings.enabled;
                    Drawing::GfxInvalidateScreen();
                    break;

                case WIDX_TOGGLE_FOVEATION_CURSOR:
                    Drawing::gFoveationFollowsCursor = !Drawing::gFoveationFollowsCursor;
                    Drawing::GfxInvalidateScreen();
                    break;
            }
        }

        void onUpdate() override
        {
        }

        void onPrepareDraw() override
        {
            const auto& ls = GetContext()->GetLocalisationService();
            const auto currentLanguage = ls.GetCurrentLanguage();
            if (ResizeLanguage != currentLanguage)
            {
                ResizeLanguage = currentLanguage;
                invalidate();

                // Find the width of the longest string
                int16_t newWidth = 0;
                for (size_t widgetIndex = WIDX_TOGGLE_SHOW_WIDE_PATHS; widgetIndex <= WIDX_TOGGLE_SHOW_DIRTY_VISUALS;
                     widgetIndex++)
                {
                    const auto& stringIdx = widgets[widgetIndex].text;
                    auto string = ls.GetString(stringIdx);
                    Guard::ArgumentNotNull(string);
                    const auto strWidth = Drawing::getStringWidth(string, FontStyle::medium);
                    newWidth = std::max<int16_t>(strWidth, newWidth);
                }

                // Add padding for both sides (8) and the offset for the text after the checkbox (15)
                newWidth += 8 * 2 + 15;

                width = newWidth;
                maxWidth = newWidth;
                minWidth = newWidth;
                widgets[WIDX_BACKGROUND].right = newWidth - 1;
                widgets[WIDX_TOGGLE_SHOW_WIDE_PATHS].right = newWidth - 8;
                widgets[WIDX_TOGGLE_SHOW_BLOCKED_TILES].right = newWidth - 8;
                widgets[WIDX_TOGGLE_SHOW_SEGMENT_HEIGHTS].right = newWidth - 8;
                widgets[WIDX_TOGGLE_SHOW_BOUND_BOXES].right = newWidth - 8;
                widgets[WIDX_TOGGLE_SHOW_DIRTY_VISUALS].right = newWidth - 8;

                invalidate();
            }

            setCheckboxValue(WIDX_TOGGLE_SHOW_WIDE_PATHS, gPaintWidePathsAsGhost);
            setCheckboxValue(WIDX_TOGGLE_SHOW_BLOCKED_TILES, gPaintBlockedTiles);
            setCheckboxValue(WIDX_TOGGLE_SHOW_SEGMENT_HEIGHTS, gShowSupportSegmentHeights);
            setCheckboxValue(WIDX_TOGGLE_SHOW_BOUND_BOXES, gPaintBoundingBoxes);
            setCheckboxValue(WIDX_TOGGLE_SHOW_DIRTY_VISUALS, gShowDirtyVisuals);
            setCheckboxValue(WIDX_TOGGLE_STABLE_PAINT_SORT, gPaintStableSort);
            setCheckboxValue(WIDX_TOGGLE_FORCE_REDRAW, gPaintForceRedraw);
            setCheckboxValue(WIDX_TOGGLE_FOVEATION, Drawing::gFoveatedRenderingSettings.enabled);
            setCheckboxValue(WIDX_TOGGLE_FOVEATION_CURSOR, Drawing::gFoveationFollowsCursor);
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);

            auto screenCoords = windowPos
                + ScreenCoordsXY{ widgets[WIDX_TOGGLE_FOVEATION].left + 15, widgets[WIDX_TOGGLE_FOVEATION].top };
            drawText(rt, screenCoords, "Enable foveated rendering");

            screenCoords = windowPos
                + ScreenCoordsXY{ widgets[WIDX_TOGGLE_FOVEATION_CURSOR].left + 15, widgets[WIDX_TOGGLE_FOVEATION_CURSOR].top };
            drawText(rt, screenCoords, "Foveation follows cursor");
        }
    };

    WindowBase* DebugPaintOpen()
    {
        auto* windowMgr = GetWindowManager();
        auto* window = windowMgr->FocusOrCreate<DebugPaintWindow>(
            WindowClass::debugPaint, { 16, ContextGetHeight() - 16 - 33 - kWindowSize.height }, kWindowSize,
            { WindowFlag::stickToFront, WindowFlag::transparent, WindowFlag::noTitleBar });

        return window;
    }
} // namespace OpenRCT2::Ui::Windows
