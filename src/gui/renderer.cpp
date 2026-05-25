#include "lifeengine/gui/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

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

    GridPoint visibleStart = viewport.screenToGrid(0.0, 0.0, options.cellSize);
    GridPoint visibleEnd = viewport.screenToGrid(static_cast<double>(options.target.width()), static_cast<double>(options.target.height()), options.cellSize);
    int firstCol = std::clamp((std::min)(visibleStart.col, visibleEnd.col) - 1, 0, world.gridMap.cols);
    int lastCol = std::clamp((std::max)(visibleStart.col, visibleEnd.col) + 2, 0, world.gridMap.cols);
    int firstRow = std::clamp((std::min)(visibleStart.row, visibleEnd.row) - 1, 0, world.gridMap.rows);
    int lastRow = std::clamp((std::max)(visibleStart.row, visibleEnd.row) + 2, 0, world.gridMap.rows);

    if (firstCol >= lastCol || firstRow >= lastRow) {
        return;
    }

    std::vector<int> xBounds(static_cast<std::size_t>(lastCol - firstCol + 1));
    std::vector<int> yBounds(static_cast<std::size_t>(lastRow - firstRow + 1));

    for (int col = firstCol; col <= lastCol; ++col) {
        ScreenPoint point = viewport.gridToScreen(col, 0, options.cellSize);
        xBounds[static_cast<std::size_t>(col - firstCol)] = static_cast<int>(std::round(options.target.left + point.x));
    }
    for (int row = firstRow; row <= lastRow; ++row) {
        ScreenPoint point = viewport.gridToScreen(0, row, options.cellSize);
        yBounds[static_cast<std::size_t>(row - firstRow)] = static_cast<int>(std::round(options.target.top + point.y));
    }

    for (int col = firstCol; col < lastCol; ++col) {
        int x = xBounds[static_cast<std::size_t>(col - firstCol)];
        int nextX = xBounds[static_cast<std::size_t>(col - firstCol + 1)] + std::max(0, options.cellOverlap);
        if (x >= options.target.right) break;
        if (nextX <= options.target.left) continue;

        for (int row = firstRow; row < lastRow; ++row) {
            int y = yBounds[static_cast<std::size_t>(row - firstRow)];
            int nextY = yBounds[static_cast<std::size_t>(row - firstRow + 1)] + std::max(0, options.cellOverlap);
            if (y >= options.target.bottom) break;
            if (nextY <= options.target.top) continue;

            const GridCell* cell = world.gridMap.cellAt(col, row);
            if (!options.paintEmpty && cell->state == CellState::Empty) {
                continue;
            }
            Rect cellRect{
                (std::max)(x, options.target.left),
                (std::max)(y, options.target.top),
                (std::min)(nextX, options.target.right),
                (std::min)(nextY, options.target.bottom),
            };

            surface.fillRect(cellRect, palette.colorFor(cell->state));

            if (options.drawEyes && cell->state == CellState::Eye && cell->cellOwner != nullptr && cellRect.width() > 2 && cellRect.height() > 2) {
                surface.drawEyeSlit(cellRect, cell->cellOwner->absoluteDirection(), palette.eyeSlit);
            }
        }
    }
}

} // namespace lifeengine::gui
