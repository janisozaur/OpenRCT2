# OpenRCT2 UI v2 Guidance Document: Independent Scaling and Modern Layouts

## Introduction
This document outlines the architecture and implementation plan for "UI v2", a modern UI system for OpenRCT2 designed to support HiDPI displays, fractional scaling, and flexible layouts.

The primary goals are:
1. **Independent Scaling**: The UI can be scaled (e.g., 1.5x, 2.0x) independently of the game world's zoom level.
2. **Modern Layout Engine**: Using Facebook Yoga (Flexbox) to replace absolute pixel positioning.
3. **High Fidelity**: Support for antialiased fonts (FreeType) and high-resolution assets.
4. **Decoupled Architecture**: Separation of UI logic, layout, and rendering.

---

## 1. Core Architecture

### 1.1 Rendering Pipeline
Currently, OpenRCT2 renders everything into a single 8-bit paletted buffer. For UI v2:
- **Game World**: Continues to render into the paletted buffer (or its own layer).
- **UI Overlay**: Rendered into a separate **RGBA32** buffer at the "scaled" resolution.
- **Composition**: The UI layer is composited over the game world. If the game world is 1080p and the UI scale is 1.5x, the UI is effectively rendered at 1080p density but logic-wise it behaves as a smaller canvas, then upscaled/downscaled or ideally rendered directly at the target pixel density.

### 1.2 Layout Engine (Yoga)
Yoga will handle the calculation of widget positions and sizes.
- Each `Window` in v2 will own a tree of Yoga nodes (`YGNodeRef`).
- Layout is calculated during a "Reflow" pass whenever the window is resized or content changes.
- Coordinates are stored as floats in Yoga and converted to scaled integers for the rendering engine.

### 1.3 Fluent C++ API
To maintain a declarative feel within C++, a fluent builder pattern is recommended:

```cpp
auto window = FlexWindow::Create("Settings")
    .SetPadding(10)
    .AddChild(
        FlexRow::Create()
            .SetJustifyContent(YGJustifyCenter)
            .AddChild(FlexText::Create("Volume"))
            .AddChild(FlexSlider::Create(0, 100, currentVolume))
    );
```

---

## 2. Decoupling the UI Layer

### 2.1 Coordinate Systems
We must distinguish between:
- **Logical Pixels**: The coordinates used in window definitions (e.g., a button is 100 units wide).
- **Scaled Pixels**: Logical pixels * UI Scale.
- **Physical Pixels**: The actual pixels on the screen/window.

The `Drawing::RenderTarget` should be updated to handle a `scale` factor, or a new `UIRenderTarget` should be introduced that works in the RGBA space.

### 2.2 Font System
- Move away from `SPR_FONT_*` sprites for UI v2.
- Use `FreeTypeFont` to render glyphs directly into the RGBA buffer.
- Implement **Dynamic Font Scaling**: Instead of scaling a rendered bitmap, the font is re-rasterized at the exact physical size needed for the current UI scale to ensure maximum crispness.

---

## 3. Implementation Guideline

### Step 1: Yoga Integration
1. Add Yoga as a dependency (via `cmake/download.cmake` or system package).
2. Create a `FlexWidget` base class that wraps a `YGNodeRef`.

### Step 2: UI v2 Base Classes
1. **FlexWindow**: A new window class inheriting from `WindowBase` (or a common ancestor) that overrides `onDraw` and handles the Yoga layout pass.
2. **UIRenderContext**: A class that manages the RGBA buffer and provides high-level drawing primitives (rounded rects, antialiased lines, FreeType text).

### Step 3: Migration Path
1. Implement the top toolbar or a simple dialog (like "About") in UI v2 first.
2. Use a "Bridge" widget to embed legacy paletted viewports (like the park view) inside a Yoga-managed layout.

---

## 4. Prototype Example ("Hello World")

Below is a conceptual implementation of a modern "About" window.

```cpp
#include <openrct2-ui/interface/FlexWindow.h>
#include <openrct2-ui/widgets/FlexButton.h>
#include <openrct2-ui/widgets/FlexText.h>
#include <yoga/Yoga.h>

namespace OpenRCT2::Ui::Windows
{
    class AboutWindowV2 final : public FlexWindow
    {
    public:
        AboutWindowV2() : FlexWindow(WindowClass::about)
        {
            // Define the layout using the Fluent API
            SetRoot(
                FlexColumn::Create()
                    .SetPadding(20)
                    .SetAlignItems(YGAlignCenter)
                    .AddChildren({
                        FlexImage::Create(SPR_G2_LOGO),
                        FlexText::Create("OpenRCT2")
                            .SetFontSize(24)
                            .SetMarginTop(10),
                        FlexText::Create(gVersionInfoFull)
                            .SetColor(Colours::Grey),
                        FlexButton::Create("Close", [this]() { Close(); })
                            .SetMarginTop(20)
                            .SetPadding(10, 20)
                    })
            );
        }

        void OnUpdate() override
        {
            // Logic update if needed
        }
    };

    void AboutOpenV2()
    {
        auto* windowMgr = GetWindowManager();
        windowMgr->Create<AboutWindowV2>();
    }
}
```

---

## 5. Next Steps for Developers
1. **Refactor RenderTarget**: Introduce `IDrawingContext` which can be implemented by both the legacy paletted engine and the new RGBA engine.
2. **Yoga Integration**: Set up the build system to include Yoga.
3. **Widget Library**: Build out basic `FlexWidget` implementations (Button, Checkbox, Slider, etc.).
