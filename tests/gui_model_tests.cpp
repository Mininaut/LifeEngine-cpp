#include "lifeengine/gui/application.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

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

    check(model.world().gridMap.cellAt(centerCol, centerRow)->state == lifeengine::CellState::Wall, "wall brush places wall");
    check(model.world().organismCount() == 0, "wall brush removes killed organism");
}

void testKillBrush() {
    lifeengine::gui::SimulationGuiModel model(20, 20, 5, 3);
    model.setRunning(false);
    model.setBrushSize(0);
    model.setTool(lifeengine::gui::ToolMode::Kill);
    auto [centerCol, centerRow] = model.world().gridMap.center();

    model.applyToolAt(centerCol, centerRow);

    check(model.world().organismCount() == 0, "kill brush removes organism");
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

} // namespace

int main() {
    testFoodBrush();
    testWallBrushKillsBlockingOrganism();
    testKillBrush();
    testResizeGridToCanvas();
    testAdvanceOnlyWhenRunning();

    if (failures != 0) {
        std::cerr << failures << " GUI model test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "all GUI model tests passed\n";
    return EXIT_SUCCESS;
}
