#include "lifeengine/gui/application.hpp"
#include "lifeengine/gui/renderer.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

class CountingSurface : public lifeengine::gui::RenderSurface {
public:
    void fillRect(const lifeengine::gui::Rect& rect, std::uint32_t) override {
        ++fills;
        rects.push_back(rect);
    }

    void drawEyeSlit(const lifeengine::gui::Rect&, lifeengine::Direction, std::uint32_t) override {
        ++eyeSlits;
    }

    int fills = 0;
    int eyeSlits = 0;
    std::vector<lifeengine::gui::Rect> rects;
};

void testFoodBrush() {
    lifeengine::gui::SimulationGuiModel model(20, 20, 5, 1);
    model.setRunning(false);
    model.setBrushSize(0);
    model.setTool(lifeengine::gui::ToolMode::Food);

    model.applyToolAt(3, 4);
    check(model.world().gridMap.cellAt(3, 4)->state == lifeengine::CellState::Food, "food brush places food");

    model.applyToolAt(3, 4, true);
    check(model.world().gridMap.cellAt(3, 4)->state == lifeengine::CellState::Empty, "food brush secondary clears food");
}

void testWallBrushKillsBlockingOrganism() {
    lifeengine::gui::SimulationGuiModel model(20, 20, 5, 2);
    model.setRunning(false);
    model.setBrushSize(0);
    model.setTool(lifeengine::gui::ToolMode::Wall);
    auto [centerCol, centerRow] = model.world().gridMap.center();

    model.applyToolAt(centerCol, centerRow);

    check(model.world().resetCount == 1, "wall brush death triggers auto reset");
    check(model.world().organismCount() == 1, "wall brush auto reset reseeds organism");
}

void testKillBrush() {
    lifeengine::gui::SimulationGuiModel model(20, 20, 5, 3);
    model.setRunning(false);
    model.setBrushSize(0);
    model.setTool(lifeengine::gui::ToolMode::Kill);
    auto [centerCol, centerRow] = model.world().gridMap.center();

    model.applyToolAt(centerCol, centerRow);

    check(model.world().resetCount == 1, "kill brush death triggers auto reset");
    check(model.world().organismCount() == 1, "kill brush auto reset reseeds organism");
}

void testResizeGridToCanvas() {
    lifeengine::gui::SimulationGuiModel model(20, 20, 5, 4);
    model.resizeGridToCanvas(200, 100, 10);

    check(model.world().gridMap.cols == 20, "resize grid cols");
    check(model.world().gridMap.rows == 10, "resize grid rows");
    check(model.settings().cellSize == 10, "resize stores cell size");
    check(model.world().organismCount() == 1, "resize reseeds origin");
}

void testAdvanceOnlyWhenRunning() {
    lifeengine::gui::SimulationGuiModel model(20, 20, 5, 5);
    model.setRunning(false);
    model.advance(std::chrono::milliseconds(1000));
    check(model.world().totalTicks == 0, "paused advance does not tick");

    model.setRunning(true);
    model.setTargetFps(10);
    model.advance(std::chrono::milliseconds(1000));
    check(model.world().totalTicks == 10, "running advance ticks by fps");
}

void testMaxSpeedTarget() {
    lifeengine::gui::SimulationGuiModel model(20, 20, 5, 6);
    model.setTargetFps(2000);

    check(model.settings().targetFps == 5000, "max speed slider maps to high step rate");
}

void testViewportTransforms() {
    lifeengine::gui::Viewport viewport;
    viewport.panBy(10.0, 20.0);
    viewport.zoomAt(50.0, 50.0, 2.0);

    lifeengine::gui::GridPoint grid = viewport.screenToGrid(50.0, 50.0, 5);
    lifeengine::gui::ScreenPoint screen = viewport.gridToScreen(grid.col, grid.row, 5);

    check(viewport.scale() == 2.0, "viewport zoom stores scale");
    check(grid.col >= 0 && grid.row >= 0, "viewport screen to grid");
    check(screen.x <= 50.0 && screen.y <= 50.0, "viewport grid to screen");

    viewport.reset();
    check(viewport.scale() == 1.0 && viewport.offsetX() == 0.0 && viewport.offsetY() == 0.0, "viewport reset");
}

void testEditorWorldIsSeparateFromSimulationWorld() {
    lifeengine::gui::SimulationGuiModel model(20, 20, 5, 7);
    auto [worldCenterCol, worldCenterRow] = model.world().gridMap.center();
    auto [editorCenterCol, editorCenterRow] = model.editor().world().gridMap.center();

    model.editor().addCellAtGrid(editorCenterCol + 1, editorCenterRow, lifeengine::CellState::Producer);

    check(model.editor().organism().anatomy.cells().size() == 2, "editor adds cell in editor organism");
    check(model.world().organisms[0]->anatomy.cells().size() == 3, "main world origin organism unchanged by editor");
    check(model.world().gridMap.cellAt(worldCenterCol + 1, worldCenterRow)->state != lifeengine::CellState::Producer,
          "editor grid edits do not paint simulation world");
}

void testEditorEyeRotationAndCenterProtection() {
    lifeengine::gui::EditorModel editor;
    auto [centerCol, centerRow] = editor.world().gridMap.center();

    editor.addCellAtGrid(centerCol + 1, centerRow, lifeengine::CellState::Eye);
    lifeengine::Direction before = editor.organism().anatomy.getLocalCell(1, 0)->direction;
    bool rotated = editor.rotateEyeAtGrid(centerCol + 1, centerRow);
    bool removedCenter = editor.removeCellAtGrid(centerCol, centerRow);

    check(rotated, "editor rotates eye");
    check(editor.organism().anatomy.getLocalCell(1, 0)->direction == lifeengine::rotateRight(before), "editor eye direction advances");
    check(!removedCenter, "editor protects center cell");
}

void testDropEditorOrganismIntoWorld() {
    lifeengine::gui::SimulationGuiModel model(30, 30, 5, 8);
    model.clearWorld();
    auto [centerCol, centerRow] = model.world().gridMap.center();
    auto [editorCenterCol, editorCenterRow] = model.editor().world().gridMap.center();
    model.editor().addCellAtGrid(editorCenterCol + 1, editorCenterRow, lifeengine::CellState::Producer);

    bool dropped = model.dropEditorOrganismAt(centerCol, centerRow);

    check(dropped, "drop editor organism succeeds in empty world");
    check(model.world().organismCount() == 1, "drop editor organism adds world organism");
    check(model.world().organisms[0]->anatomy.cells().size() == 2, "drop editor organism copies editor anatomy");
    check(model.world().fossilRecord.numExtantSpecies() == 1, "drop editor organism creates world species");
}

void testRenderGridUsesSurfaceAbstraction() {
    lifeengine::gui::SimulationGuiModel model(10, 10, 5, 9);
    lifeengine::gui::Viewport viewport;
    CountingSurface surface;
    lifeengine::gui::GridRenderOptions options{
        &model.settings().palette,
        &viewport,
        model.settings().cellSize,
        {0, 0, 50, 50},
        true,
        true,
        0};

    lifeengine::gui::renderGrid(model.world(), surface, options);

    check(surface.fills == 100, "renderer fills visible grid cells");

    CountingSurface nonEmptySurface;
    options.paintEmpty = false;
    options.cellOverlap = 0;
    lifeengine::gui::renderGrid(model.world(), nonEmptySurface, options);

    check(nonEmptySurface.fills > 0, "renderer paints living cells when empty cells are skipped");
    check(nonEmptySurface.fills < surface.fills, "renderer can skip empty cells for faster paints");
}

void testRenderGridAvoidsFractionalZoomOverlap() {
    lifeengine::gui::SimulationGuiModel model(10, 10, 5, 10);
    lifeengine::gui::Viewport viewport;
    viewport.zoomAt(0.0, 0.0, 0.3);
    CountingSurface surface;
    lifeengine::gui::GridRenderOptions options{
        &model.settings().palette,
        &viewport,
        model.settings().cellSize,
        {0, 0, 20, 20},
        true,
        true,
        0};

    lifeengine::gui::renderGrid(model.world(), surface, options);

    for (int col = 1; col < model.world().gridMap.cols; ++col) {
        const lifeengine::gui::Rect& previous = surface.rects[(col - 1) * model.world().gridMap.rows];
        const lifeengine::gui::Rect& current = surface.rects[col * model.world().gridMap.rows];
        check(previous.right == current.left, "fractional zoom horizontal cells share one edge");
    }
    for (int row = 1; row < model.world().gridMap.rows; ++row) {
        const lifeengine::gui::Rect& previous = surface.rects[row - 1];
        const lifeengine::gui::Rect& current = surface.rects[row];
        check(previous.bottom == current.top, "fractional zoom vertical cells share one edge");
    }
}

} // namespace

int main() {
    testFoodBrush();
    testWallBrushKillsBlockingOrganism();
    testKillBrush();
    testResizeGridToCanvas();
    testAdvanceOnlyWhenRunning();
    testMaxSpeedTarget();
    testViewportTransforms();
    testEditorWorldIsSeparateFromSimulationWorld();
    testEditorEyeRotationAndCenterProtection();
    testDropEditorOrganismIntoWorld();
    testRenderGridUsesSurfaceAbstraction();
    testRenderGridAvoidsFractionalZoomOverlap();

    if (failures != 0) {
        std::cerr << failures << " GUI model test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "all GUI model tests passed\n";
    return EXIT_SUCCESS;
}
