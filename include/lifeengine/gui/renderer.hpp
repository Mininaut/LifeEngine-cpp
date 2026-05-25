#pragma once

#include "lifeengine/gui/application.hpp"

#include <cstdint>

namespace lifeengine::gui {

struct Rect {
    int left;
    int top;
    int right;
    int bottom;

    int width() const;
    int height() const;
};

class RenderSurface {
public:
    virtual ~RenderSurface() = default;
    virtual void fillRect(const Rect& rect, std::uint32_t rgb) = 0;
    virtual void drawEyeSlit(const Rect& cellRect, Direction direction, std::uint32_t rgb) = 0;
};

struct GridRenderOptions {
    const Palette* palette;
    const Viewport* viewport;
    int cellSize;
    Rect target;
    bool drawEyes = true;
};

void renderGrid(const WorldEnvironment& world, RenderSurface& surface, const GridRenderOptions& options);

} // namespace lifeengine::gui
