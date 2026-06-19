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

### 1.1 Rendering Pipeline & Layer Separation
OpenRCT2's legacy renderer uses an 8-bit indexed (paletted) buffer. To support modern HiDPI and full-color UI, we must implement a multi-layered rendering pipeline.

#### Layer 1: Game World (Legacy Palette)
- Remains at the "Game Resolution" (which might be lower than native for performance or aesthetic reasons).
- Uses the traditional 256-color palette.
- Rendered into an 8-bit buffer as it is today.

#### Layer 2: UI Overlay (Modern RGBA32)
- Rendered into a **separate RGBA32 (32-bit) surface**.
- **Pixel Density**: This surface should match the **physical display resolution** (or the OS window's resolution) to ensure text and vector elements remain sharp.
- **Transparency**: Supports per-pixel alpha, allowing for modern shadows, blurs, and semi-transparent windows without the "checkerboard" dither patterns of the legacy engine.

#### Composition
The final frame is created by:
1. Upscaling the Game World layer (if necessary) to the physical window size.
2. Compositing the UI Overlay on top using alpha blending.

---

## 2. Technical Implementation of the Rendering Layer

### 2.1 The `IDrawingContext` Interface
To decouple the UI from the underlying pixel format, we should introduce an abstraction layer.

```cpp
class IDrawingContext {
public:
    virtual ~IDrawingContext() = default;

    // Primitives using logical coordinates
    virtual void DrawRect(Rect rect, Color color) = 0;
    virtual void DrawText(Point pos, std::string_view text, FontHandle font) = 0;
    virtual void DrawImage(Point pos, ImageId image) = 0;

    // Scaling Metadata
    virtual float GetScale() const = 0; // The current UI scale factor
};
```

### 2.2 Handling HiDPI & Fractional Scaling
The system must distinguish between different coordinate spaces to handle fractional scaling (e.g., 125% or 150% OS zoom):

1. **Logical Units (Yoga)**: Floating point units used for layout (e.g., "this button is 100.0 units wide").
2. **Scaled Pixels**: Logical units * `UI_Scale`. These are the "virtual" pixels the UI thinks it has.
3. **Physical Pixels**: The actual pixels on the display.

**Formula**: `Physical_Size = Logical_Size * UI_Scale * Display_DPI_Factor`

For a window on a 4K monitor with 200% scaling and a 1.5x UI scale setting:
- A logical 100-unit button will be layout-calculated as 100.0.
- It will be rendered at `100 * 1.5 * 2.0 = 300` physical pixels wide.
- **Critical**: The UI layer's RGBA buffer must be allocated at the **Physical Pixel** size to avoid blurriness.

### 2.3 Full Colour & Antialiasing
By moving the UI to RGBA32:
- **Fonts**: FreeType can use sub-pixel antialiasing (LCD rendering) directly into the UI buffer.
- **Sprites**: High-resolution UI sprites (PNG/SVG) can be used alongside legacy sprites (which will be converted to RGBA on-the-fly or pre-cached).
- **Effects**: We can implement modern UI effects like drop shadows and rounded corners using signed distance fields (SDF) or simple alpha blending.

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

## 3. Decoupling the UI Layer

### 3.1 Coordinate Systems
As detailed in section 2.2, we must strictly separate logical layout units from physical pixels. The UI engine will handle the conversion during the layout and paint passes.

### 3.2 Font System
- Move away from `SPR_FONT_*` sprites for UI v2.
- Use `FreeTypeFont` to render glyphs directly into the RGBA buffer.
- Implement **Dynamic Font Scaling**: Instead of scaling a rendered bitmap, the font is re-rasterized at the exact physical size needed for the current UI scale to ensure maximum crispness.

---

## 4. Implementation Guideline

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

## 5. Prototype Example ("Hello World")

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

## 6. Next Steps for Developers
1. **Refactor RenderTarget**: Introduce `IDrawingContext` which can be implemented by both the legacy paletted engine and the new RGBA engine.
2. **Yoga Integration**: Set up the build system to include Yoga.
3. **Widget Library**: Build out basic `FlexWidget` implementations (Button, Checkbox, Slider, etc.).
