/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../UiStringIds.h"

#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/PlatformEnvironment.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/StringIds.h>
#include <openrct2/scripting/Plugin.h>
#include <openrct2/scripting/ScriptEngine.h>
#include <openrct2/ui/UiContext.h>
#include <openrct2/ui/WindowManager.h>
#include <vector>

using namespace OpenRCT2::Drawing;
using namespace OpenRCT2::Scripting;

namespace OpenRCT2::Ui::Windows
{
    enum WindowPluginManagerWidgetIdx
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_COLUMN_NAME,
        WIDX_COLUMN_CURRENT,
        WIDX_COLUMN_AVAILABLE,
        WIDX_COLUMN_AUTHORS,
        WIDX_SCROLL,
        WIDX_OPEN_RELEASE_PAGE,
        WIDX_OPEN_FOLDER,
        WIDX_CHECK_UPDATES
    };

    static constexpr StringId kWindowTitle = STR_PLUGIN_MANAGER_TITLE;
    static constexpr ScreenSize kWindowSize = { 600, 400 };

    constexpr int32_t kNameColLeft = 4;
    constexpr int32_t kCurrentColLeft = 154;
    constexpr int32_t kAvailableColLeft = 234;
    constexpr int32_t kAuthorsColLeft = 314;

    static constexpr auto window_plugin_manager_widgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget({ kNameColLeft, 57 }, { 150, 14 }, WidgetType::tableHeader, WindowColour::primary, STR_PLUGIN_NAME),
        makeWidget(
            { kCurrentColLeft, 57 }, { 80, 14 }, WidgetType::tableHeader, WindowColour::primary, STR_PLUGIN_VERSION_CURRENT),
        makeWidget(
            { kAvailableColLeft, 57 }, { 80, 14 }, WidgetType::tableHeader, WindowColour::primary,
            STR_PLUGIN_VERSION_AVAILABLE),
        makeWidget({ kAuthorsColLeft, 57 }, { 282, 14 }, WidgetType::tableHeader, WindowColour::primary, STR_PLUGIN_AUTHORS),
        makeWidget({ kNameColLeft, 71 }, { 592, 297 }, WidgetType::scroll, WindowColour::primary, SCROLL_VERTICAL),
        makeWidget({ kNameColLeft, 377 }, { 190, 14 }, WidgetType::button, WindowColour::primary, STR_OPEN_RELEASE_PAGE),
        makeWidget({ 205, 377 }, { 190, 14 }, WidgetType::button, WindowColour::primary, STR_OPEN_PLUGIN_DIRECTORY),
        makeWidget({ 406, 377 }, { 190, 14 }, WidgetType::button, WindowColour::primary, STR_CHECK_FOR_UPDATES));

    class PluginManagerWindow final : public Window
    {
    private:
        std::vector<std::shared_ptr<Plugin>> _plugins;
        int32_t _highlightedIndex = -1;

        void RefreshPluginList()
        {
            auto& scriptEngine = GetContext()->GetScriptEngine();
            _plugins = scriptEngine.GetPlugins();
            numListItems = static_cast<uint16_t>(_plugins.size());
            invalidate();
        }

    public:
        void onOpen() override
        {
            setWidgets(window_plugin_manager_widgets);
            WindowInitScrollWidgets(*this);
            RefreshPluginList();
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_OPEN_RELEASE_PAGE:
                    if (selectedListItem >= 0 && selectedListItem < static_cast<int32_t>(_plugins.size()))
                    {
                        const auto& updateInfo = _plugins[selectedListItem]->GetUpdateInfo();
                        if (!updateInfo.ReleasePageURL.empty())
                        {
                            GetContext()->GetUiContext().OpenURL(updateInfo.ReleasePageURL);
                        }
                    }
                    break;
                case WIDX_OPEN_FOLDER:
                {
                    auto context = GetContext();
                    auto& env = context->GetPlatformEnvironment();
                    auto& uiContext = context->GetUiContext();
                    uiContext.OpenFolder(env.GetDirectoryPath(DirBase::user, DirId::plugins));
                    break;
                }
                case WIDX_CHECK_UPDATES:
                {
                    auto& scriptEngine = GetContext()->GetScriptEngine();
                    scriptEngine.CheckForPluginUpdates(true);
                    break;
                }
            }
        }

        void onUpdate() override
        {
            if (currentFrame % 32 == 0)
            {
                invalidateWidget(WIDX_SCROLL);
            }
        }

        ScreenSize onScrollGetSize(int32_t scrollIndex) override
        {
            return { 0, static_cast<int32_t>(_plugins.size() * kScrollableRowHeight) };
        }

        void onScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            selectedListItem = screenCoords.y / kScrollableRowHeight;
            invalidateWidget(WIDX_SCROLL);
        }

        void onScrollMouseOver(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            _highlightedIndex = screenCoords.y / kScrollableRowHeight;
            invalidateWidget(WIDX_SCROLL);
        }

        void onPrepareDraw() override
        {
            if (selectedListItem >= 0 && selectedListItem < static_cast<int32_t>(_plugins.size()))
            {
                const auto& updateInfo = _plugins[selectedListItem]->GetUpdateInfo();
                if (updateInfo.ReleasePageURL.empty())
                    disabledWidgets |= (1uLL << WIDX_OPEN_RELEASE_PAGE);
                else
                    disabledWidgets &= ~(1uLL << WIDX_OPEN_RELEASE_PAGE);
            }
            else
            {
                disabledWidgets |= (1uLL << WIDX_OPEN_RELEASE_PAGE);
            }
        }

        void onDraw(RenderTarget& rt) override
        {
            WindowDrawWidgets(*this, rt);
        }

        void onScrollDraw(int32_t scrollIndex, RenderTarget& rt) override
        {
            auto rtCoords = ScreenCoordsXY{ rt.x, rt.y };
            Rectangle::fill(
                rt, { rtCoords, rtCoords + ScreenCoordsXY{ rt.width - 1, rt.height - 1 } },
                getColourMap(colours[1].colour).midLight);

            for (int32_t i = 0; i < static_cast<int32_t>(_plugins.size()); i++)
            {
                int32_t y = i * kScrollableRowHeight;
                if (y > rt.y + rt.height)
                    break;
                if (y + kScrollableRowHeight < rt.y)
                    continue;

                const auto& plugin = _plugins[i];
                const auto& metadata = plugin->GetMetadata();
                const auto& updateInfo = plugin->GetUpdateInfo();

                ScreenRect rect = { { 0, y }, { widgets[WIDX_SCROLL].width() - 1, y + kScrollableRowHeight - 1 } };
                if (i == selectedListItem)
                    Rectangle::fill(rt, rect, getColourMap(colours[1].colour).darker);
                else if (i == _highlightedIndex)
                    Rectangle::fill(rt, rect, getColourMap(colours[1].colour).midDark);
                else if (i % 2 == 1)
                    Rectangle::fill(rt, rect, getColourMap(colours[1].colour).light);

                // Draw Name
                drawText(rt, { kNameColLeft, y }, metadata.Name.c_str(), { Colour::black });

                // Draw Current Version
                drawText(rt, { kCurrentColLeft, y }, metadata.Version.c_str(), { Colour::black });

                // Draw Available Version
                if (updateInfo.UpdateAvailable)
                {
                    drawText(rt, { kAvailableColLeft, y }, updateInfo.LatestVersion.c_str(), { Colour::darkGreen });
                }
                else if (!updateInfo.LatestVersion.empty())
                {
                    drawText(rt, { kAvailableColLeft, y }, updateInfo.LatestVersion.c_str(), { Colour::black });
                }

                // Draw Authors
                std::string authors;
                for (size_t a = 0; a < metadata.Authors.size(); a++)
                {
                    authors += metadata.Authors[a];
                    if (a < metadata.Authors.size() - 1)
                        authors += ", ";
                }
                drawText(rt, { kAuthorsColLeft, y }, authors.c_str(), { Colour::black });
            }
        }
    };

    WindowBase* PluginManagerOpen()
    {
        auto* windowMgr = GetWindowManager();
        auto* window = windowMgr->BringToFrontByClass(WindowClass::pluginManager);
        if (window == nullptr)
        {
            window = windowMgr->Create<PluginManagerWindow>(
                WindowClass::pluginManager, kWindowSize, { WindowFlag::stickToFront });
        }
        return window;
    }
} // namespace OpenRCT2::Ui::Windows
