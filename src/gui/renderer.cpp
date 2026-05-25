#include "lifeengine/gui/renderer.hpp"

#include <algorithm>
#include <cmath>

namespace lifeengine::gui {

int Rect::width() const {
    return right - left;
}

int Rect::height() const {
    return bottom - top;
}

void renderGrid(const WorldEnvironment& world, RenderSurface& surface, const GridRenderOptions& options) {
    const Viewport& viewport = *options.viewport;
    const Palette& palette = *options.palette;
    double cellPixels = viewport.scaledCellSize(options.cellSize);
    int size = static_cast<int>(std::round(cellPixels));

    for (int col = 0; col < world.gridMap.cols; ++col) {
        ScreenPoint sp = viewport.gridToScreen(col, 0, options.cellSize);
        int x = static_cast<int>(std::round(options.target.left + sp.x));
        if (x >= options.target.right) break;
        if (x + size < options.target.left) continue;

        for (int row = 0; row < world.gridMap.rows; ++row) {
            ScreenPoint rp = viewport.gridToScreen(col, row, options.cellSize);
            int y = static_cast<int>(std::round(options.target.top + rp.y));
            if (y >= options.target.bottom) break;
            if (y + size < options.target.top) continue;

            const GridCell* cell = world.gridMap.cellAt(col, row);
            Rect cellRect{
                x,
                y,
                (std::min)(x + size, options.target.right),
                (std::min)(y + size, options.target.bottom),
            };

            surface.fillRect(cellRect, palette.colorFor(cell->state));

            if (options.drawEyes && cell->state == CellState::Eye && cell->cellOwner != nullptr && size > 2) {
                surface.drawEyeSlit(cellRect, cell->cellOwner->absoluteDirection(), palette.eyeSlit);
            }
        }
    }
}

} // namespace lifeengine::gui
