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

    const double cellPixels = viewport.scaledCellSize(options.cellSize);
    const double originX = static_cast<double>(options.target.left) + viewport.offsetX();
    const double originY = static_cast<double>(options.target.top) + viewport.offsetY();
    const int cellOverlap = std::max(0, options.cellOverlap);
    thread_local std::vector<int> xBounds;
    thread_local std::vector<int> yBounds;
    xBounds.resize(static_cast<std::size_t>(lastCol - firstCol + 1));
    yBounds.resize(static_cast<std::size_t>(lastRow - firstRow + 1));

    for (int col = firstCol; col <= lastCol; ++col) {
        xBounds[static_cast<std::size_t>(col - firstCol)] =
            static_cast<int>(std::round(originX + (static_cast<double>(col) * cellPixels)));
    }
    for (int row = firstRow; row <= lastRow; ++row) {
        yBounds[static_cast<std::size_t>(row - firstRow)] =
            static_cast<int>(std::round(originY + (static_cast<double>(row) * cellPixels)));
    }

    for (int col = firstCol; col < lastCol; ++col) {
        int x = xBounds[static_cast<std::size_t>(col - firstCol)];
        int nextX = xBounds[static_cast<std::size_t>(col - firstCol + 1)] + cellOverlap;
        if (x >= options.target.right) break;
        if (nextX <= options.target.left) continue;

        for (int row = firstRow; row < lastRow; ++row) {
            int y = yBounds[static_cast<std::size_t>(row - firstRow)];
            int nextY = yBounds[static_cast<std::size_t>(row - firstRow + 1)] + cellOverlap;
            if (y >= options.target.bottom) break;
            if (nextY <= options.target.top) continue;

            const GridCell* cell = world.gridMap.cellAtUnchecked(col, row);
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

            if (cell->state == CellState::Eye && options.drawEyes && cell->cellOwner != nullptr && cellRect.width() > 2 && cellRect.height() > 2) {
                surface.drawEyeSlit(cellRect, cell->cellOwner->absoluteDirection(), palette.eyeSlit);
            }
        }
    }
}

} // namespace lifeengine::gui
