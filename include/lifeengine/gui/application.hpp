#pragma once

#include "lifeengine/core.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
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

struct GridPoint {
    int col = 0;
    int row = 0;
};

struct ScreenPoint {
    double x = 0.0;
    double y = 0.0;
};

class Viewport {
public:
    void reset();
    void panBy(double dx, double dy);
    void zoomAt(double localX, double localY, double factor);
    GridPoint screenToGrid(double localX, double localY, int cellSize) const;
    ScreenPoint gridToScreen(int col, int row, int cellSize) const;
    double scaledCellSize(int cellSize) const;

    double scale() const;
    double offsetX() const;
    double offsetY() const;

private:
    double scale_ = 1.0;
    double offsetX_ = 0.0;
    double offsetY_ = 0.0;
};

class WorldDocument {
public:
    WorldDocument(int cols = 160, int rows = 100, int cellSize = 5, std::uint32_t seed = 1);

    void reset(bool withOrigin = true);
    void clear();
    void resizeToCanvas(int canvasWidth, int canvasHeight, int cellSize);
    void randomizeWalls(double densityPercent = 4.0);

    WorldEnvironment& world();
    const WorldEnvironment& world() const;

private:
    WorldEnvironment world_;
};

class SimulationRuntime {
public:
    void setRunning(bool running);
    void toggleRunning();
    bool running() const;
    void setTargetFps(int fps);
    int targetFps() const;
    void advance(WorldEnvironment& world, std::chrono::milliseconds elapsed);
    void stepOnce(WorldEnvironment& world);
    void resetAccumulator();

private:
    bool running_ = true;
    int targetFps_ = 60;
    double stepAccumulator_ = 0.0;
};

class EditorModel {
public:
    EditorModel(int cellSize = 13, std::uint32_t seed = 2);

    void setDefaultOrganism();
    bool addCellAtGrid(int col, int row, CellState state);
    bool removeCellAtGrid(int col, int row);
    bool rotateEyeAtGrid(int col, int row);
    Organism& organism();
    const Organism& organism() const;
    WorldEnvironment& world();
    const WorldEnvironment& world() const;

private:
    std::pair<int, int> toLocal(int col, int row) const;
    void repaintOrganism();

    WorldEnvironment editorWorld_;
    Organism* organism_ = nullptr;
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
    bool dropEditorOrganismAt(int col, int row);
    void applyRulesFromSettings();
    StatsSnapshot stats() const;

    WorldEnvironment& world();
    const WorldEnvironment& world() const;
    WorldDocument& worldDocument();
    const WorldDocument& worldDocument() const;
    SimulationRuntime& runtime();
    const SimulationRuntime& runtime() const;
    EditorModel& editor();
    const EditorModel& editor() const;
    GuiSettings& settings();
    const GuiSettings& settings() const;

private:
    void applyCellBrush(int col, int row, CellState state, bool killBlocking, CellState ignoredState);
    void killBrush(int col, int row);

    WorldDocument worldDocument_;
    SimulationRuntime runtime_;
    EditorModel editor_;
    GuiSettings settings_;
};

int runNativeGui(int showCommand);

} // namespace lifeengine::gui
