/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <SDL.h>
#include <algorithm>
#include <bitset>
#include <iterator>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/Input.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/BitSet.hpp>
#include <openrct2/core/String.hpp>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.String.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/interface/ColourWithFlags.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/localisation/Language.h>
#include <openrct2/ui/WindowManager.h>
#include <string>

using namespace OpenRCT2::Drawing;

namespace OpenRCT2::Ui::Windows
{
    constexpr int32_t kDropdownItemHeight = 12;
    constexpr int32_t kDropdownItemHeightTouch = 24;
    // Padding to the left of an item, where a marker can be drawn.
    static constexpr int32_t kDropdownItemLeftPadding = 10;

    static constexpr std::array<uint8_t, 57> kAppropriateImageDropdownItemsPerRow = {
        1, 1, 1, 1, 2, 2, 3, 3, 4, 3, // 10
        5, 4, 4, 5, 5, 5, 4, 5, 6, 5, // 20
        5, 7, 4, 5, 6, 5, 6, 6, 6, 6, // 30
        6, 8, 8, 8, 9, 9, 9, 9, 9, 9, // 40
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 50
        9, 9, 9, 9, 9, 9, 9,          // 56
    };

    enum
    {
        WIDX_BACKGROUND,
    };

    static constexpr Widget kWindowDropdownWidgets[] = {
        makeWidget({ 0, 0 }, { 1, 1 }, WidgetType::imgBtn, WindowColour::primary),
    };

    Dropdown::DropdownState gDropdown{};

    class DropdownWindow final : public Window
    {
        int32_t NumColumns = 1;
        int32_t NumRows = 1;
        int32_t ItemWidth = 1;
        int32_t ItemHeight = 1;
        int32_t ItemPadding = 0;
        bool ListVertically = true;
        bool IsSearchable = false;
        int32_t MaxRowsPerColumn = Dropdown::kItemsMaxSize;
        ScreenCoordsXY BaseScreenPos;
        int32_t BaseExtraY;

        void FilterItems()
        {
            gDropdown.numFilteredItems = 0;
            if (gDropdown.searchText[0] == '\0')
            {
                for (int32_t i = 0; i < gDropdown.numItems; i++)
                {
                    gDropdown.filteredItems[gDropdown.numFilteredItems++] = i;
                }
            }
            else
            {
                for (int32_t i = 0; i < gDropdown.numItems; i++)
                {
                    if (gDropdown.items[i].isSeparator())
                        continue;

                    if (String::fuzzyContains(gDropdown.items[i].text, gDropdown.searchText))
                    {
                        gDropdown.filteredItems[gDropdown.numFilteredItems++] = i;
                    }
                }
            }

            if (gDropdown.numFilteredItems > 0)
            {
                if (!ListVertically)
                {
                    NumColumns = std::max(1, NumColumns);
                    NumRows = (gDropdown.numFilteredItems + NumColumns - 1) / NumColumns;
                }
                else
                {
                    int32_t availableHeight = getSpaceUntilBottom(BaseScreenPos, BaseExtraY);
                    if (IsSearchable)
                        availableHeight -= (ItemHeight + 2);

                    int32_t numAvailableRows = std::max(1, availableHeight / ItemHeight);
                    NumRows = std::min({ gDropdown.numFilteredItems, numAvailableRows, MaxRowsPerColumn });
                    NumColumns = (gDropdown.numFilteredItems + NumRows - 1) / NumRows;
                }
            }
            else
            {
                NumRows = 1;
                NumColumns = 1;
            }

            // Ensure highlighted index is still valid within the filtered items
            if (gDropdown.highlightedIndex != -1)
            {
                bool stillVisible = false;
                for (int32_t i = 0; i < gDropdown.numFilteredItems; i++)
                {
                    if (gDropdown.filteredItems[i] == gDropdown.highlightedIndex)
                    {
                        stillVisible = true;
                        break;
                    }
                }
                if (!stillVisible)
                {
                    gDropdown.highlightedIndex = -1;
                }
            }
        }

        void onResize() override
        {
            UpdateSizeAndPosition(BaseScreenPos, BaseExtraY);
        }

    public:
        void onOpen() override
        {
            setWidgets(kWindowDropdownWidgets);

            // Input state
            NumColumns = 1;
            NumRows = 1;
            ListVertically = true;
            IsSearchable = false;

            InputSetState(InputState::DropdownActive);
        }

        void onTextInput(WidgetIndex widgetIndex, std::string_view text) override
        {
            if (!IsSearchable)
                return;

            String::safeConcat(gDropdown.searchText, text.data(), sizeof(gDropdown.searchText));
            FilterItems();
            UpdateSizeAndPosition(BaseScreenPos, BaseExtraY);
            invalidate();
        }

        void onKeyDown(uint32_t key)
        {
            switch (key)
            {
                case SDLK_BACKSPACE:
                {
                    if (IsSearchable)
                    {
                        String::backspace(gDropdown.searchText);
                        FilterItems();
                        UpdateSizeAndPosition(BaseScreenPos, BaseExtraY);
                        invalidate();
                    }
                    break;
                }
                case SDLK_ESCAPE:
                    WindowDropdownClose();
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                {
                    int32_t index = gDropdown.highlightedIndex;
                    if (index != -1)
                    {
                        WindowDropdownSelectItem(index);
                    }
                    else
                    {
                        WindowDropdownClose();
                    }
                    break;
                }
                case SDLK_UP:
                {
                    if (gDropdown.numFilteredItems == 0)
                        break;

                    int32_t currentFilteredIdx = -1;
                    for (int32_t i = 0; i < gDropdown.numFilteredItems; i++)
                    {
                        if (gDropdown.filteredItems[i] == gDropdown.highlightedIndex)
                        {
                            currentFilteredIdx = i;
                            break;
                        }
                    }

                    if (currentFilteredIdx == -1)
                        currentFilteredIdx = gDropdown.numFilteredItems - 1;
                    else
                        currentFilteredIdx = (currentFilteredIdx + gDropdown.numFilteredItems - 1) % gDropdown.numFilteredItems;

                    gDropdown.highlightedIndex = gDropdown.filteredItems[currentFilteredIdx];
                    invalidate();
                    break;
                }
                case SDLK_DOWN:
                {
                    if (gDropdown.numFilteredItems == 0)
                        break;

                    int32_t currentFilteredIdx = -1;
                    for (int32_t i = 0; i < gDropdown.numFilteredItems; i++)
                    {
                        if (gDropdown.filteredItems[i] == gDropdown.highlightedIndex)
                        {
                            currentFilteredIdx = i;
                            break;
                        }
                    }

                    if (currentFilteredIdx == -1)
                        currentFilteredIdx = 0;
                    else
                        currentFilteredIdx = (currentFilteredIdx + 1) % gDropdown.numFilteredItems;

                    gDropdown.highlightedIndex = gDropdown.filteredItems[currentFilteredIdx];
                    invalidate();
                    break;
                }
            }
        }

        static int32_t GetDefaultRowHeight()
        {
            return Config::Get().interface.enlargedUi ? kDropdownItemHeightTouch : kDropdownItemHeight;
        }

        static int32_t GetAdditionalRowPadding()
        {
            return Config::Get().interface.enlargedUi ? 6 : 0;
        }

        void drawItem(RenderTarget& rt, ScreenCoordsXY screenCoords, int32_t i, bool highlighted)
        {
            const auto& item = gDropdown.items[i];
            switch (item.type)
            {
                case Dropdown::ItemType::regular:
                {
                    auto formatString = STR_OPTIONS_DROPDOWN_ITEM;
                    if (i < Dropdown::kItemsMaxSize && gDropdown.items[i].isChecked())
                        formatString = STR_OPTIONS_DROPDOWN_ITEM_SELECTED;

                    drawTextItem(rt, screenCoords, width, item, highlighted, formatString, colours[0].colour);
                    break;
                }
                case Dropdown::ItemType::toggle:
                {
                    auto formatString = STR_TOGGLE_OPTION;
                    if (i < Dropdown::kItemsMaxSize && gDropdown.items[i].isChecked())
                        formatString = STR_TOGGLE_OPTION_CHECKED;

                    drawTextItem(rt, screenCoords, width, item, highlighted, formatString, colours[0].colour);
                    break;
                }
                case Dropdown::ItemType::plain:
                {
                    drawTextItem(rt, screenCoords, width, item, highlighted, STR_STRING, colours[0].colour);
                    break;
                }
                case Dropdown::ItemType::image:
                {
                    GfxDrawSprite(rt, item.image, screenCoords);
                    break;
                }
                case Dropdown::ItemType::colour:
                {
                    auto image = item.image;
                    if (highlighted)
                        image = image.WithIndexOffset(1);
                    GfxDrawSprite(rt, image, screenCoords);
                    break;
                }
                case Dropdown::ItemType::separator:
                    break;
            }
        }

        void drawSeparator(RenderTarget& rt, ScreenCoordsXY screenCoords)
        {
            const auto leftTop = screenCoords + ScreenCoordsXY{ 2, (ItemHeight / 2) - 1 };
            const auto rightBottom = leftTop + ScreenCoordsXY{ ItemWidth - 4, 0 };
            const auto shadowOffset = ScreenCoordsXY{ 0, 1 };

            if (colours[0].flags.has(ColourFlag::translucent))
            {
                auto palette = kTranslucentWindowPalettes[EnumValue(colours[0].colour)];
                Rectangle::filter(rt, { leftTop, rightBottom }, palette.highlight);
                Rectangle::filter(rt, { leftTop + shadowOffset, rightBottom + shadowOffset }, palette.shadow);
            }
            else
            {
                Rectangle::fill(rt, { leftTop, rightBottom }, getColourMap(colours[0].colour).midDark);
                Rectangle::fill(
                    rt, { leftTop + shadowOffset, rightBottom + shadowOffset }, getColourMap(colours[0].colour).lightest);
            }
        }

        void drawTextItem(
            RenderTarget& rt, ScreenCoordsXY screenCoords, int32_t ddWidth, const Dropdown::Item& item, bool highlighted,
            StringId format, Colour background)
        {
            ColourWithFlags colour = { background };
            if (highlighted)
                colour.colour = Colour::white;
            if (item.isDisabled())
                colour = { background, { ColourFlag::inset } };

            auto yOffset = ItemPadding;
            Formatter ft;
            ft.Add<const utf8*>(item.text);

            drawTextEllipsised(rt, { screenCoords.x + 2, screenCoords.y + yOffset }, ddWidth - 7, format, ft, { colour });
        }

        void onDraw(RenderTarget& rt) override
        {
            drawWidgets(rt);

            int32_t yOffset = 2;
            if (IsSearchable)
            {
                // Draw search bar
                const ScreenCoordsXY searchBoxTopLeft = windowPos + ScreenCoordsXY{ 2, 2 };
                const ScreenCoordsXY searchBoxBottomRight = searchBoxTopLeft + ScreenCoordsXY{ width - 5, ItemHeight - 1 };
                Rectangle::fillInset(rt, { searchBoxTopLeft, searchBoxBottomRight }, colours[0], Rectangle::BorderStyle::inset);

                std::string searchDisplay = std::string(reinterpret_cast<const char*>(gDropdown.searchText));
                if (TextBoxCaretIsFlashed())
                {
                    searchDisplay += "_";
                }

                Formatter ft;
                ft.Add<const char*>(searchDisplay.c_str());
                drawTextEllipsised(rt, searchBoxTopLeft + ScreenCoordsXY{ 1, 1 }, width - 7, STR_STRING, ft, colours[0].colour);

                yOffset += ItemHeight + 2;
            }

            if (gDropdown.numFilteredItems == 0 && IsSearchable && gDropdown.searchText[0] != '\0')
            {
                ScreenCoordsXY screenCoords = windowPos + ScreenCoordsXY{ 2, yOffset };
                drawText(
                    rt, screenCoords + ScreenCoordsXY{ 2, 1 }, STR_NO_MATCHES_FOUND, {},
                    { { colours[0].colour, { ColourFlag::inset } } });
                return;
            }

            for (int32_t i = 0; i < gDropdown.numFilteredItems; i++)
            {
                int32_t actualIndex = gDropdown.filteredItems[i];
                ScreenCoordsXY cellCoords;
                if (ListVertically)
                    cellCoords = { i / NumRows, i % NumRows };
                else
                    cellCoords = { i % NumColumns, i / NumColumns };

                ScreenCoordsXY screenCoords = windowPos
                    + ScreenCoordsXY{ 2 + (cellCoords.x * ItemWidth), yOffset + (cellCoords.y * ItemHeight) };

                bool highlighted = (actualIndex == gDropdown.highlightedIndex);
                if (highlighted)
                {
                    // Darken the cell's background slightly when highlighted
                    const ScreenCoordsXY rightBottom = screenCoords + ScreenCoordsXY{ ItemWidth - 1, ItemHeight - 1 };
                    Rectangle::filter(rt, { screenCoords, rightBottom }, FilterPaletteID::paletteDarken3);

                    if (gDropdown.hasTooltips && gDropdown.lastTooltipHover != actualIndex)
                    {
                        gDropdown.lastTooltipHover = actualIndex;
                        const auto& item = gDropdown.items[actualIndex];
                        if (item.tooltip != kStringIdEmpty)
                        {
                            WindowTooltipShow({ item.tooltip, {} }, screenCoords);
                        }
                    }
                }

                if (gDropdown.items[actualIndex].isSeparator())
                {
                    drawSeparator(rt, screenCoords);
                }
                else if (gDropdown.cellDrawFunction.has_value())
                {
                    RenderTarget clippedRT;
                    if (ClipRenderTarget(clippedRT, rt, screenCoords, ItemWidth, ItemHeight))
                    {
                        gDropdown.cellDrawFunction.value()(clippedRT, gDropdown.items[actualIndex], highlighted);
                    }
                }
                else
                {
                    drawItem(rt, screenCoords, actualIndex, highlighted);
                }
            }
        }

        static int32_t getSpaceUntilBottom(const ScreenCoordsXY& screenPos, int32_t dropdownButtonHeight)
        {
            auto* mainWindow = WindowGetMain();
            if (mainWindow == nullptr)
                return 400;

            return std::max(1, mainWindow->height - (screenPos.y + dropdownButtonHeight + 5));
        }

        void setTextItems(
            const ScreenCoordsXY& screenPos, int32_t extraY, ColourWithFlags colour, uint8_t customItemHeight, uint8_t txtFlags,
            size_t numItems, int32_t itemWidth, int32_t numRowsPerColumn)
        {
            // Set and calculate num items, rows and columns
            ItemHeight = (txtFlags & Dropdown::Flag::CustomHeight) ? customItemHeight : GetDefaultRowHeight();
            ItemPadding = (txtFlags & Dropdown::Flag::CustomHeight) ? 0 : GetAdditionalRowPadding();
            ItemWidth = itemWidth;
            BaseScreenPos = screenPos;
            BaseExtraY = extraY;
            MaxRowsPerColumn = numRowsPerColumn > 0 ? numRowsPerColumn : Dropdown::kItemsMaxSize;

            gDropdown.numItems = static_cast<int32_t>(numItems);
            IsSearchable = (gDropdown.numItems > 10) && !(txtFlags & Dropdown::Flag::NoSearch);
            ListVertically = true;

            // Initial highlight from default index
            gDropdown.highlightedIndex = gDropdown.defaultIndex;
            FilterItems();

            UpdateSizeAndPosition(BaseScreenPos, BaseExtraY);

            if (colour.flags.has(ColourFlag::translucent))
                flags |= WindowFlag::transparent;
            colours[0] = colour;
        }

        void setImageItems(
            const ScreenCoordsXY& screenPos, int32_t extraY, ColourWithFlags colour, int32_t numItems, int32_t itemWidth,
            int32_t itemHeight, int32_t numColumns)
        {
            // Set and calculate num items, rows and columns
            ItemWidth = itemWidth;
            ItemHeight = itemHeight;
            gDropdown.numItems = numItems;
            BaseScreenPos = screenPos;
            BaseExtraY = extraY;
            NumColumns = std::max(1, numColumns);

            // There must always be at least one column and row to prevent dividing by zero
            if (gDropdown.numItems == 0)
            {
                NumRows = 1;
            }
            else
            {
                NumRows = (gDropdown.numItems + NumColumns - 1) / NumColumns;
            }

            // image dropdowns are listed horizontally
            ListVertically = false;
            IsSearchable = false;

            gDropdown.highlightedIndex = gDropdown.defaultIndex;
            FilterItems();
            UpdateSizeAndPosition(screenPos, extraY);

            if (colour.flags.has(ColourFlag::translucent))
                flags |= WindowFlag::transparent;
            colours[0] = colour;
        }

        int32_t GetIndexFromPoint(const ScreenCoordsXY& loc)
        {
            int32_t yOffset = 2;
            if (IsSearchable)
            {
                yOffset += ItemHeight + 2;
            }

            // If the mouse is in the padding/search box area, no item is selected
            if (loc.y < windowPos.y + yOffset)
                return -1;

            int32_t top = loc.y - windowPos.y - yOffset;
            if (top < 0)
                return -1;

            int32_t left = loc.x - windowPos.x;
            if (left >= width)
                return -1;
            left -= 2;
            if (left < 0 || left >= ItemWidth * NumColumns)
                return -1;

            int32_t columnNum = left / ItemWidth;
            if (columnNum >= NumColumns)
                return -1;

            int32_t rowNum = top / ItemHeight;
            if (rowNum >= NumRows)
                return -1;

            int32_t dropdownIndex;
            if (ListVertically)
                dropdownIndex = columnNum * NumRows + rowNum;
            else
                dropdownIndex = rowNum * NumColumns + columnNum;

            if (dropdownIndex >= gDropdown.numFilteredItems)
                return -1;

            return gDropdown.filteredItems[dropdownIndex];
        }

    private:
        void UpdateSizeAndPosition(const ScreenCoordsXY& screenPos, const int32_t extraY)
        {
            // Calculate position and size
            const auto ddWidth = ItemWidth * NumColumns + 3;
            auto ddHeight = ItemHeight * NumRows + 3;
            if (IsSearchable)
            {
                ddHeight += ItemHeight + 2;
            }

            int32_t screenWidth = ContextGetWidth();
            int32_t screenHeight = ContextGetHeight();
            auto boundedScreenPos = screenPos;
            if (screenPos.x + ddWidth > screenWidth)
                boundedScreenPos.x = std::max(0, screenWidth - ddWidth);
            if (screenPos.y + ddHeight > screenHeight)
                boundedScreenPos.y = std::max(0, screenHeight - ddHeight);
            widgets[WIDX_BACKGROUND].right = ddWidth;
            widgets[WIDX_BACKGROUND].bottom = ddHeight;

            invalidate();
            width = ddWidth + 1;
            height = ddHeight + 1;
            windowPos = boundedScreenPos + ScreenCoordsXY{ 0, extraY };
            invalidate();
        }
    };

    static void copyItemsToGlobal(std::span<const Dropdown::Item> items)
    {
        for (size_t i = 0; i < items.size(); i++)
        {
            gDropdown.items[i] = items[i];
        }
    }

    /**
     * Shows a text dropdown menu.
     *  rct2: 0x006ECFB9
     *
     * @param x (cx)
     * @param y (dx)
     * @param extray (di)
     * @param flags (bh)
     * @param num_items (bx)
     * @param colour (al)
     */
    void WindowDropdownShowText(
        const ScreenCoordsXY& screenPos, int32_t extray, ColourWithFlags colour, uint8_t flags, size_t num_items,
        size_t prefRowsPerColumn)
    {
        // Calculate the longest string width
        int32_t maxStringWidth = 0;
        for (size_t i = 0; i < num_items; i++)
        {
            int32_t stringWidth = getStringWidth(gDropdown.items[i].text, FontStyle::medium);
            if (gDropdown.items[i].type != Dropdown::ItemType::plain)
                stringWidth += kDropdownItemLeftPadding;
            maxStringWidth = std::max(stringWidth, maxStringWidth);
        }

        WindowDropdownShowTextCustomWidth(
            screenPos, extray, colour, 0, flags, num_items, maxStringWidth + 3, prefRowsPerColumn);

        gDropdown.cellDrawFunction = std::nullopt;
    }

    void WindowDropdownShowText(
        const ScreenCoordsXY& screenPos, int32_t extray, ColourWithFlags colour, uint8_t flags,
        std::span<const Dropdown::Item> items, size_t prefRowsPerColumn)
    {
        copyItemsToGlobal(items);
        WindowDropdownShowText(screenPos, extray, colour, flags, items.size(), prefRowsPerColumn);
    }

    /**
     * Shows a text dropdown menu.
     *  rct2: 0x006ECFB9, although 0x006ECE50 is real version
     *
     * @param x (cx)
     * @param y (dx)
     * @param extray (di)
     * @param flags (bh)
     * @param num_items (bx)
     * @param colour (al)
     * @param customItemHeight (ah) requires flag set as well
     */
    void WindowDropdownShowTextCustomWidth(
        const ScreenCoordsXY& screenPos, int32_t extray, ColourWithFlags colour, uint8_t customItemHeight, uint8_t flags,
        size_t num_items, int32_t width, size_t prefRowsPerColumn)
    {
        gInputFlags.unset(InputFlag::dropdownStayOpen, InputFlag::dropdownMouseUp);
        if (flags & Dropdown::Flag::StayOpen || Config::Get().interface.touchEnhancements)
            gInputFlags.set(InputFlag::dropdownStayOpen);

        WindowDropdownClose();

        gDropdown.searchText[0] = '\0';
        gDropdown.numFilteredItems = 0;
        gDropdown.highlightedIndex = -1;

        // Create the window (width/height position are set later)
        auto* windowMgr = GetWindowManager();
        auto* w = windowMgr->Create<DropdownWindow>(
            WindowClass::dropdown, { width, customItemHeight }, { WindowFlag::stickToFront, WindowFlag::noTitleBar });
        if (w != nullptr)
        {
            auto numRowsPerColumn = prefRowsPerColumn > 0 ? static_cast<int32_t>(prefRowsPerColumn) : Dropdown::kItemsMaxSize;
            w->setTextItems(screenPos, extray, colour, customItemHeight, flags, num_items, width, numRowsPerColumn);
        }

        gDropdown.cellDrawFunction = std::nullopt;
    }

    void WindowDropdownShowTextCustomWidth(
        const ScreenCoordsXY& screenPos, int32_t extray, ColourWithFlags colour, uint8_t custom_height, uint8_t flags,
        std::span<const Dropdown::Item> items, int32_t width, size_t prefRowsPerColumn)
    {
        copyItemsToGlobal(items);
        WindowDropdownShowTextCustomWidth(
            screenPos, extray, colour, custom_height, flags, items.size(), width, prefRowsPerColumn);
    }

    /**
     * Shows an image dropdown menu.
     *  rct2: 0x006ECFB9
     *
     * @param x (cx)
     * @param y (dx)
     * @param extray (di)
     * @param flags (bh)
     * @param numItems (bx)
     * @param colour (al)
     * @param itemWidth (bp)
     * @param itemHeight (ah)
     * @param numColumns (bl)
     */
    void WindowDropdownShowImage(
        const ScreenCoordsXY& screenPos, int32_t extray, ColourWithFlags colour, uint8_t flags, int32_t numItems,
        int32_t itemWidth, int32_t itemHeight, int32_t numColumns)
    {
        gInputFlags.unset(InputFlag::dropdownStayOpen, InputFlag::dropdownMouseUp);
        if (flags & Dropdown::Flag::StayOpen || Config::Get().interface.touchEnhancements)
            gInputFlags.set(InputFlag::dropdownStayOpen);

        // Close existing dropdown
        WindowDropdownClose();

        gDropdown.searchText[0] = '\0';
        gDropdown.numFilteredItems = 0;
        gDropdown.highlightedIndex = -1;

        // Create the window (width/height position are set later)
        auto* windowMgr = GetWindowManager();
        auto* w = windowMgr->Create<DropdownWindow>(WindowClass::dropdown, { itemWidth, itemHeight }, WindowFlag::stickToFront);
        if (w != nullptr)
        {
            w->setImageItems(screenPos, extray, colour, numItems, itemWidth, itemHeight, numColumns);
        }

        gDropdown.cellDrawFunction = std::nullopt;
    }

    void WindowDropdownShowCustom(
        const ScreenCoordsXY& screenPos, int32_t extraY, ColourWithFlags colour, uint8_t flags,
        Dropdown::CellDrawFunction drawFunction, int32_t numItems, int32_t itemWidth, int32_t itemHeight, int32_t numColumns)
    {
        // Fall back to image internals
        WindowDropdownShowImage(screenPos, extraY, colour, flags, numItems, itemWidth, itemHeight, numColumns);

        gDropdown.cellDrawFunction = drawFunction;
    }

    void WindowDropdownClose()
    {
        auto* windowMgr = GetWindowManager();
        windowMgr->CloseByClass(WindowClass::dropdown);
    }

    void WindowDropdownSelectItem(int32_t index)
    {
        auto* windowMgr = GetWindowManager();
        for (auto& w : gWindowList)
        {
            if (w->classification != WindowClass::dropdown && !w->flags.has(WindowFlag::dead))
            {
                if (gPressedWidget.windowClassification == w->classification && gPressedWidget.windowNumber == w->number)
                {
                    windowMgr->CloseByClass(WindowClass::dropdown);
                    _inputState = InputState::Normal;
                    gInputFlags.unset(InputFlag::widgetPressed);
                    windowMgr->InvalidateWidgetByNumber(w->classification, w->number, gPressedWidget.widgetIndex);
                    w->onDropdown(gPressedWidget.widgetIndex, index);
                    return;
                }
            }
        }
        windowMgr->CloseByClass(WindowClass::dropdown);
    }

    void WindowDropdownHandleKeyDown(uint32_t key)
    {
        auto* windowMgr = GetWindowManager();
        auto* w = windowMgr->FindByClass(WindowClass::dropdown);
        if (w != nullptr)
        {
            auto* ddWnd = static_cast<DropdownWindow*>(w);
            ddWnd->onKeyDown(key);
        }
    }

    void WindowDropdownHandleTextInput(std::string_view text)
    {
        auto* windowMgr = GetWindowManager();
        auto* w = windowMgr->FindByClass(WindowClass::dropdown);
        if (w != nullptr)
        {
            auto* ddWnd = static_cast<DropdownWindow*>(w);
            ddWnd->onTextInput(WIDX_BACKGROUND, text);
        }
    }

    /**
     * New function based on 6e914e
     * returns -1 if index is invalid
     */
    int32_t DropdownIndexFromPoint(const ScreenCoordsXY& loc, WindowBase* w)
    {
        if (w->classification == WindowClass::dropdown)
        {
            auto* ddWnd = static_cast<DropdownWindow*>(w);
            return ddWnd->GetIndexFromPoint(loc);
        }
        return -1;
    }

    // Colour ordered for use in colour dropdown
    static constexpr Colour kColoursDropdownOrder[] = {
        Colour::black,          Colour::saturatedRed,   Colour::darkOrange,   Colour::darkYellow,
        Colour::forestGreen,    Colour::saturatedGreen, Colour::deepWater,    Colour::darkBlue,
        Colour::violet,

        Colour::grey,           Colour::brightRed,      Colour::lightOrange,  Colour::yellow,
        Colour::mossGreen,      Colour::brightGreen,    Colour::darkWater,    Colour::lightBlue,
        Colour::brightPurple,

        Colour::white,          Colour::lightPink,      Colour::pastelOrange, Colour::brightYellow,
        Colour::chartreuse,     Colour::limeGreen,      Colour::lightWater,   Colour::icyBlue,
        Colour::lavender,

        Colour::umber,          Colour::maroon,         Colour::sepia,        Colour::saturatedBrown,
        Colour::armyGreen,      Colour::hunterGreen,    Colour::viridian,     Colour::darkPurple,
        Colour::darkPink,

        Colour::darkBrown,      Colour::bordeauxRed,    Colour::salmonPink,   Colour::lightBrown,
        Colour::darkOliveGreen, Colour::oliveGreen,     Colour::darkGreen,    Colour::lightPurple,
        Colour::brightPink,

        Colour::beige,          Colour::coralPink,      Colour::peach,        Colour::tan,
        Colour::honeyDew,       Colour::celadon,        Colour::seafoamGreen, Colour::periwinkle,
        Colour::pastelPink,

        Colour::invisible,      Colour::voidBackground,
    };

    constexpr std::array kColourTooltips = {
        STR_COLOUR_BLACK_TIP,
        STR_COLOUR_SATURATED_RED_TIP,
        STR_COLOUR_DARK_ORANGE_TIP,
        STR_COLOUR_DARK_YELLOW_TIP,
        STR_COLOUR_GRASS_GREEN_DARK_TIP,
        STR_COLOUR_SATURATED_GREEN_TIP,
        STR_COLOUR_DEEP_WATER_TIP,
        STR_COLOUR_DARK_BLUE_TIP,
        STR_COLOUR_SATURATED_PURPLE_DARK_TIP,

        STR_COLOUR_GREY_TIP,
        STR_COLOUR_BRIGHT_RED_TIP,
        STR_COLOUR_LIGHT_ORANGE_TIP,
        STR_COLOUR_YELLOW_TIP,
        STR_COLOUR_MOSS_GREEN_TIP,
        STR_COLOUR_BRIGHT_GREEN_TIP,
        STR_COLOUR_DARK_WATER_TIP,
        STR_COLOUR_LIGHT_BLUE_TIP,
        STR_COLOUR_BRIGHT_PURPLE_TIP,

        STR_COLOUR_WHITE_TIP,
        STR_COLOUR_LIGHT_PINK_TIP,
        STR_COLOUR_ORANGE_LIGHT_TIP,
        STR_COLOUR_BRIGHT_YELLOW_TIP,
        STR_COLOUR_GRASS_GREEN_LIGHT_TIP,
        STR_COLOUR_SATURATED_GREEN_LIGHT_TIP,
        STR_COLOUR_LIGHT_WATER_TIP,
        STR_COLOUR_ICY_BLUE_TIP,
        STR_COLOUR_SATURATED_PURPLE_LIGHT_TIP,

        STR_COLOUR_DULL_BROWN_DARK_TIP,
        STR_COLOUR_BORDEAUX_RED_DARK_TIP,
        STR_COLOUR_TAN_DARK_TIP,
        STR_COLOUR_SATURATED_BROWN_TIP,
        STR_COLOUR_DARK_OLIVE_DARK_TIP,
        STR_COLOUR_OLIVE_DARK_TIP,
        STR_COLOUR_DULL_GREEN_DARK_TIP,
        STR_COLOUR_DARK_PURPLE_TIP,
        STR_COLOUR_DARK_PINK_TIP,

        STR_COLOUR_DARK_BROWN_TIP,
        STR_COLOUR_BORDEAUX_RED_TIP,
        STR_COLOUR_SALMON_PINK_TIP,
        STR_COLOUR_LIGHT_BROWN_TIP,
        STR_COLOUR_DARK_OLIVE_GREEN_TIP,
        STR_COLOUR_OLIVE_GREEN_TIP,
        STR_COLOUR_DARK_GREEN_TIP,
        STR_COLOUR_LIGHT_PURPLE_TIP,
        STR_COLOUR_BRIGHT_PINK_TIP,

        STR_COLOUR_DULL_BROWN_LIGHT_TIP,
        STR_COLOUR_BORDEAUX_RED_LIGHT_TIP,
        STR_COLOUR_TAN_LIGHT_TIP,
        STR_COLOUR_SATURATED_BROWN_LIGHT_TIP,
        STR_COLOUR_DARK_OLIVE_LIGHT_TIP,
        STR_COLOUR_OLIVE_LIGHT_TIP,
        STR_COLOUR_DULL_GREEN_LIGHT_TIP,
        STR_COLOUR_DULL_PURPLE_LIGHT_TIP,
        STR_COLOUR_MAGENTA_LIGHT_TIP,

        STR_COLOUR_INVISIBLE_TIP,
        STR_COLOUR_VOID_TIP,
    };

    Colour ColourDropDownIndexToColour(uint8_t ddidx)
    {
        return kColoursDropdownOrder[ddidx];
    }

    /**
     *  rct2: 0x006ED43D
     */
    void WindowDropdownShowColour(
        WindowBase* w, Widget* widget, ColourWithFlags dropdownColour, Colour selectedColour, bool alwaysHideSpecialColours)
    {
        int32_t defaultIndex = -1;

        const bool specialColoursEnabled = !alwaysHideSpecialColours && getGameState().cheats.allowSpecialColourSchemes;
        auto numColours = specialColoursEnabled ? static_cast<uint8_t>(Drawing::kColourNumTotal) : kColourNumNormal;
        // Set items
        for (uint64_t i = 0; i < numColours; i++)
        {
            auto orderedColour = ColourDropDownIndexToColour(i);
            if (selectedColour == orderedColour)
                defaultIndex = i;

            ImageId imageId;
            if (Config::Get().interface.enlargedUi)
            {
                imageId = (orderedColour == Colour::invisible) ? ImageId(SPR_G2_ICON_PALETTE_INVISIBLE_LARGE, Colour::white)
                                                               : ImageId(SPR_G2_ICON_PALETTE_LARGE, orderedColour);
            }
            else
            {
                imageId = (orderedColour == Colour::invisible) ? ImageId(SPR_G2_ICON_PALETTE_INVISIBLE, Colour::white)
                                                               : ImageId(SPR_PALETTE_BTN, orderedColour);
            }

            gDropdown.items[i] = { .type = Dropdown::ItemType::colour, .image = imageId, .tooltip = kColourTooltips[i] };
        }

        // Show dropdown
        auto squareSize = DropdownWindow::GetDefaultRowHeight();
        WindowDropdownShowImage(
            w->windowPos + ScreenCoordsXY{ widget->left, widget->top }, widget->height(), dropdownColour,
            Dropdown::Flag::StayOpen, numColours, squareSize, squareSize,
            DropdownGetAppropriateImageDropdownItemsPerRow(static_cast<uint32_t>(numColours)));

        gDropdown.hasTooltips = true;
        gDropdown.lastTooltipHover = -1;
        gDropdown.defaultIndex = defaultIndex;
    }

    uint32_t DropdownGetAppropriateImageDropdownItemsPerRow(uint32_t numItems)
    {
        // If above the table size return the last element
        return kAppropriateImageDropdownItemsPerRow[std::min<uint32_t>(
            numItems, static_cast<uint32_t>(std::size(kAppropriateImageDropdownItemsPerRow) - 1))];
    }
} // namespace OpenRCT2::Ui::Windows

namespace OpenRCT2::Dropdown
{
    static Item StringItem(ItemType _type, const utf8* string)
    {
        auto ret = Item{ .type = _type };
        String::safeUtf8Copy(ret.text, string, sizeof(ret.text));
        return ret;
    }

    Item MenuLabel(StringId stringId)
    {
        return StringItem(ItemType::regular, LanguageGetString(stringId));
    }

    Item MenuLabel(u8string string)
    {
        return StringItem(ItemType::regular, string.c_str());
    }

    Item MenuLabel(const utf8* string)
    {
        return StringItem(ItemType::regular, string);
    }

    Item MenuLabel(StringId format, const Formatter& ft)
    {
        auto string = FormatStringIDLegacy(format, ft.Data());
        return MenuLabel(string);
    }

    Item PlainMenuLabel(u8string string)
    {
        return StringItem(ItemType::plain, string.c_str());
    }

    Item PlainMenuLabel(const utf8* string)
    {
        return StringItem(ItemType::plain, string);
    }

    Item PlainMenuLabel(StringId stringId)
    {
        return StringItem(ItemType::plain, LanguageGetString(stringId));
    }

    Item PlainMenuLabel(StringId format, const Formatter& ft)
    {
        auto string = FormatStringIDLegacy(format, ft.Data());
        return StringItem(ItemType::plain, string.c_str());
    }

    Item ToggleOption(StringId stringId)
    {
        return StringItem(ItemType::toggle, LanguageGetString(stringId));
    }
} // namespace OpenRCT2::Dropdown
