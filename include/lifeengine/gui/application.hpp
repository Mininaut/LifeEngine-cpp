#pragma once

#include "lifeengine/core.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace lifeengine::gui {

enum class ToolMode {
    Food,
    Wall,
    Kill,
    Inspect,
};

struct Palette {
    std::uint32_t empty = 0x0E1318;
    std::uint32_t food = 0x2F7AB7;
    std::uint32_t wall = 0x808080;
    std::uint32_t mouth = 0xDEB14D;
    std::uint32_t producer = 0x15DE59;
    std::uint32_t mover = 0x60D4FF;
    std::uint32_t killer = 0xF82380;
    std::uint32_t armor = 0x7230DB;
    std::uint32_t eye = 0xB6C1EA;
    std::uint32_t eyeSlit = 0x0E1318;

    std::uint32_t colorFor(CellState state) const;
};

struct GuiSettings {
    bool running = true;
    bool renderEnabled = true;
    int targetFps = 60;
    int cellSize = 5;
    int brushSize = 2;
    ToolMode tool = ToolMode::Food;
    Palette palette;
};

struct StatsSnapshot {
    int ticks = 0;
    int organisms = 0;
    int species = 0;
    int largestCellCount = 0;
    int resetCount = 0;
    double averageMutability = 0.0;
    std::string topSpecies;
};

class SimulationGuiModel {
public:
    SimulationGuiModel(int cols = 160, int rows = 100, int cellSize = 5, std::uint32_t seed = 1);

    void setRunning(bool running);
    void toggleRunning();
    void setTargetFps(int fps);
    void setBrushSize(int brushSize);
    void setTool(ToolMode tool);
    void setRenderEnabled(bool enabled);
    void toggleRenderEnabled();

    void advance(std::chrono::milliseconds elapsed);
    void stepOnce();
    void resetWorld(bool withOrigin = true);
    void clearWorld();
    void resizeGridToCanvas(int canvasWidth, int canvasHeight, int cellSize);
    void randomizeWalls(double densityPercent = 4.0);
    void applyToolAt(int col, int row, bool secondary = false);
    void applyRulesFromSettings();
    StatsSnapshot stats() const;

    WorldEnvironment& world();
    const WorldEnvironment& world() const;
    GuiSettings& settings();
    const GuiSettings& settings() const;

private:
    void applyCellBrush(int col, int row, CellState state, bool killBlocking, CellState ignoredState);
    void killBrush(int col, int row);

    WorldEnvironment world_;
    GuiSettings settings_;
    double stepAccumulator_ = 0.0;
};

int runNativeGui(int showCommand);

} // namespace lifeengine::gui
