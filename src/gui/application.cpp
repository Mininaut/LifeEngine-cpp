#include "lifeengine/gui/application.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace lifeengine::gui {
namespace {

constexpr CellState kNoIgnoredState = static_cast<CellState>(-1);

} // namespace

std::uint32_t Palette::colorFor(CellState state) const {
    switch (state) {
    case CellState::Empty:
        return empty;
    case CellState::Food:
        return food;
    case CellState::Wall:
        return wall;
    case CellState::Mouth:
        return mouth;
    case CellState::Producer:
        return producer;
    case CellState::Mover:
        return mover;
    case CellState::Killer:
        return killer;
    case CellState::Armor:
        return armor;
    case CellState::Eye:
        return eye;
    }
    return empty;
}

SimulationGuiModel::SimulationGuiModel(int cols, int rows, int cellSize, std::uint32_t seed)
    : world_(cellSize, cols, rows, seed) {
    settings_.cellSize = cellSize;
    world_.originOfLife();
}

void SimulationGuiModel::setRunning(bool running) {
    settings_.running = running;
}

void SimulationGuiModel::toggleRunning() {
    settings_.running = !settings_.running;
}

void SimulationGuiModel::setTargetFps(int fps) {
    if (fps >= 300) {
        settings_.targetFps = 1000;
    } else {
        settings_.targetFps = std::max(1, fps);
    }
}

void SimulationGuiModel::setBrushSize(int brushSize) {
    settings_.brushSize = std::clamp(brushSize, 0, 15);
}

void SimulationGuiModel::setTool(ToolMode tool) {
    settings_.tool = tool;
}

void SimulationGuiModel::setRenderEnabled(bool enabled) {
    settings_.renderEnabled = enabled;
}

void SimulationGuiModel::toggleRenderEnabled() {
    settings_.renderEnabled = !settings_.renderEnabled;
}

void SimulationGuiModel::advance(std::chrono::milliseconds elapsed) {
    if (!settings_.running) {
        stepAccumulator_ = 0.0;
        return;
    }

    stepAccumulator_ += static_cast<double>(elapsed.count()) * settings_.targetFps / 1000.0;
    int steps = std::min(static_cast<int>(stepAccumulator_), 250);
    stepAccumulator_ -= steps;

    for (int i = 0; i < steps; ++i) {
        stepOnce();
    }
}

void SimulationGuiModel::stepOnce() {
    world_.update();
}

void SimulationGuiModel::resetWorld(bool withOrigin) {
    stepAccumulator_ = 0.0;
    world_.reset(withOrigin);
}

void SimulationGuiModel::clearWorld() {
    resetWorld(false);
}

void SimulationGuiModel::resizeGridToCanvas(int canvasWidth, int canvasHeight, int cellSize) {
    settings_.cellSize = std::clamp(cellSize, 1, 50);
    int cols = std::max(1, canvasWidth / settings_.cellSize);
    int rows = std::max(1, canvasHeight / settings_.cellSize);
    world_.gridMap.resize(cols, rows, settings_.cellSize);
    world_.reset(true);
}

void SimulationGuiModel::randomizeWalls(double densityPercent) {
    world_.clearWalls();
    double density = std::clamp(densityPercent, 0.0, 100.0);
    for (int col = 0; col < world_.gridMap.cols; ++col) {
        for (int row = 0; row < world_.gridMap.rows; ++row) {
            GridCell* cell = world_.gridMap.cellAt(col, row);
            if (cell == nullptr || cell->owner != nullptr) {
                continue;
            }
            if (world_.rng.chancePercent(density)) {
                world_.changeCell(col, row, CellState::Wall, nullptr);
            }
        }
    }
}

void SimulationGuiModel::applyToolAt(int col, int row, bool secondary) {
    switch (settings_.tool) {
    case ToolMode::Food:
        applyCellBrush(col, row, secondary ? CellState::Empty : CellState::Food, false, CellState::Wall);
        break;
    case ToolMode::Wall:
        applyCellBrush(col, row, secondary ? CellState::Empty : CellState::Wall, !secondary, secondary ? CellState::Food : kNoIgnoredState);
        break;
    case ToolMode::Kill:
        killBrush(col, row);
        break;
    case ToolMode::Inspect:
        break;
    }
}

void SimulationGuiModel::applyRulesFromSettings() {
    // Reserved for future UI state that needs validation before touching the core.
}

StatsSnapshot SimulationGuiModel::stats() const {
    StatsSnapshot snapshot;
    snapshot.ticks = world_.totalTicks;
    snapshot.organisms = world_.organismCount();
    snapshot.species = world_.fossilRecord.numExtantSpecies();
    snapshot.largestCellCount = world_.largestCellCount;
    snapshot.resetCount = world_.resetCount;
    snapshot.averageMutability = world_.averageMutability();
    if (auto species = world_.fossilRecord.mostPopulousSpecies()) {
        std::ostringstream out;
        out << species->name << " (" << species->population << ")";
        snapshot.topSpecies = out.str();
    } else {
        snapshot.topSpecies = "none";
    }
    return snapshot;
}

WorldEnvironment& SimulationGuiModel::world() {
    return world_;
}

const WorldEnvironment& SimulationGuiModel::world() const {
    return world_;
}

GuiSettings& SimulationGuiModel::settings() {
    return settings_;
}

const GuiSettings& SimulationGuiModel::settings() const {
    return settings_;
}

void SimulationGuiModel::applyCellBrush(int col, int row, CellState state, bool killBlocking, CellState ignoredState) {
    for (int dc = -settings_.brushSize; dc <= settings_.brushSize; ++dc) {
        for (int dr = -settings_.brushSize; dr <= settings_.brushSize; ++dr) {
            GridCell* cell = world_.gridMap.cellAt(col + dc, row + dr);
            if (cell == nullptr) {
                continue;
            }
            if (killBlocking && cell->owner != nullptr) {
                cell->owner->die();
            } else if (cell->owner != nullptr) {
                continue;
            }
            if (cell->state == ignoredState) {
                continue;
            }
            world_.changeCell(cell->col, cell->row, state, nullptr);
        }
    }
    world_.clearDeadOrganisms();
}

void SimulationGuiModel::killBrush(int col, int row) {
    std::unordered_set<Organism*> killed;
    for (int dc = -settings_.brushSize; dc <= settings_.brushSize; ++dc) {
        for (int dr = -settings_.brushSize; dr <= settings_.brushSize; ++dr) {
            GridCell* cell = world_.gridMap.cellAt(col + dc, row + dr);
            if (cell != nullptr && cell->owner != nullptr && killed.insert(cell->owner).second) {
                cell->owner->die();
            }
        }
    }
    world_.clearDeadOrganisms();
}

} // namespace lifeengine::gui
