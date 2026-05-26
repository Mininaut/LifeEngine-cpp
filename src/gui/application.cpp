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

void Viewport::reset() {
    scale_ = 1.0;
    offsetX_ = 0.0;
    offsetY_ = 0.0;
}

void Viewport::panBy(double dx, double dy) {
    offsetX_ += dx;
    offsetY_ += dy;
}

void Viewport::zoomAt(double localX, double localY, double factor, int cellSize) {
    double oldCellPixels = scaledCellSize(cellSize);
    double minScale = 1.0 / static_cast<double>(std::max(1, cellSize));
    double nextScale = std::clamp(scale_ * factor, minScale, 32.0);
    if (nextScale == scale_) {
        return;
    }
    double worldX = (localX - offsetX_) / oldCellPixels;
    double worldY = (localY - offsetY_) / oldCellPixels;
    scale_ = nextScale;
    double nextCellPixels = scaledCellSize(cellSize);
    offsetX_ = localX - (worldX * nextCellPixels);
    offsetY_ = localY - (worldY * nextCellPixels);
}

GridPoint Viewport::screenToGrid(double localX, double localY, int cellSize) const {
    double cellPixels = scaledCellSize(cellSize);
    return {
        static_cast<int>(std::floor((localX - offsetX_) / cellPixels)),
        static_cast<int>(std::floor((localY - offsetY_) / cellPixels)),
    };
}

ScreenPoint Viewport::gridToScreen(int col, int row, int cellSize) const {
    double cellPixels = scaledCellSize(cellSize);
    return {
        offsetX_ + (col * cellPixels),
        offsetY_ + (row * cellPixels),
    };
}

double Viewport::scaledCellSize(int cellSize) const {
    return std::max(1.0, static_cast<double>(cellSize) * scale_);
}

double Viewport::scale() const {
    return scale_;
}

double Viewport::offsetX() const {
    return offsetX_;
}

double Viewport::offsetY() const {
    return offsetY_;
}

WorldDocument::WorldDocument(int cols, int rows, int cellSize, std::uint32_t seed)
    : world_(cellSize, cols, rows, seed) {
    world_.originOfLife();
}

void WorldDocument::reset(bool withOrigin) {
    world_.reset(withOrigin);
}

void WorldDocument::clear() {
    reset(false);
}

void WorldDocument::resizeToCanvas(int canvasWidth, int canvasHeight, int cellSize) {
    int clampedCellSize = std::clamp(cellSize, 1, 50);
    int cols = std::max(1, canvasWidth / clampedCellSize);
    int rows = std::max(1, canvasHeight / clampedCellSize);
    world_.gridMap.resize(cols, rows, clampedCellSize);
    world_.reset(true);
}

void WorldDocument::randomizeWalls(double densityPercent) {
    world_.clearWalls();
    double density = std::clamp(densityPercent, 0.0, 100.0);
    for (int col = 0; col < world_.gridMap.cols; ++col) {
        for (int row = 0; row < world_.gridMap.rows; ++row) {
            GridCell* cell = world_.gridMap.cellAtUnchecked(col, row);
            if (cell->owner != nullptr) {
                continue;
            }
            if (world_.rng.chancePercent(density)) {
                world_.changeCellUnchecked(col, row, CellState::Wall, nullptr);
            }
        }
    }
}

WorldEnvironment& WorldDocument::world() {
    return world_;
}

const WorldEnvironment& WorldDocument::world() const {
    return world_;
}

void SimulationRuntime::setRunning(bool running) {
    running_ = running;
}

void SimulationRuntime::toggleRunning() {
    running_ = !running_;
}

bool SimulationRuntime::running() const {
    return running_;
}

void SimulationRuntime::setTargetFps(int fps) {
    if (fps >= 2000) {
        targetFps_ = 5000;
    } else {
        targetFps_ = std::max(1, fps);
    }
}

int SimulationRuntime::targetFps() const {
    return targetFps_;
}

void SimulationRuntime::advance(WorldEnvironment& world, std::chrono::milliseconds elapsed) {
    if (!running_) {
        stepAccumulator_ = 0.0;
        return;
    }

    stepAccumulator_ += static_cast<double>(elapsed.count()) * targetFps_ / 1000.0;
    int steps = std::min(static_cast<int>(stepAccumulator_), 1000);
    stepAccumulator_ -= steps;
    for (int i = 0; i < steps; ++i) {
        stepOnce(world);
    }
}

void SimulationRuntime::stepOnce(WorldEnvironment& world) {
    world.update();
}

void SimulationRuntime::resetAccumulator() {
    stepAccumulator_ = 0.0;
}

EditorModel::EditorModel(int cellSize, std::uint32_t seed)
    : editorWorld_(cellSize, 15, 15, seed) {
    editorWorld_.config.autoReset = false;
    setDefaultOrganism();
}

void EditorModel::setDefaultOrganism() {
    editorWorld_.reset(false);
    auto [centerCol, centerRow] = editorWorld_.gridMap.center();
    auto organism = std::make_unique<Organism>(centerCol, centerRow, &editorWorld_);
    organism->anatomy.addDefaultCell(CellState::Mouth, 0, 0);
    organism_ = &editorWorld_.addOrganism(std::move(organism));
    editorWorld_.fossilRecord.addSpecies(*organism_);
}

bool EditorModel::addCellAtGrid(int col, int row, CellState state) {
    auto [locCol, locRow] = toLocal(col, row);
    BodyCell* existing = organism_->anatomy.getLocalCell(locCol, locRow);
    if (existing != nullptr) {
        organism_->anatomy.replaceCell(state, locCol, locRow, false, editorWorld_.rng);
        repaintOrganism();
        return true;
    }
    if (!organism_->anatomy.canAddCellAt(locCol, locRow)) {
        return false;
    }
    organism_->anatomy.addDefaultCell(state, locCol, locRow);
    repaintOrganism();
    return true;
}

bool EditorModel::removeCellAtGrid(int col, int row) {
    auto [locCol, locRow] = toLocal(col, row);
    if (locCol == 0 && locRow == 0) {
        return false;
    }
    bool removed = organism_->anatomy.removeCell(locCol, locRow);
    repaintOrganism();
    return removed;
}

bool EditorModel::rotateEyeAtGrid(int col, int row) {
    auto [locCol, locRow] = toLocal(col, row);
    BodyCell* cell = organism_->anatomy.getLocalCell(locCol, locRow);
    if (cell == nullptr || cell->state != CellState::Eye) {
        return false;
    }
    cell->direction = rotateRight(cell->direction);
    repaintOrganism();
    return true;
}

Organism& EditorModel::organism() {
    return *organism_;
}

const Organism& EditorModel::organism() const {
    return *organism_;
}

WorldEnvironment& EditorModel::world() {
    return editorWorld_;
}

const WorldEnvironment& EditorModel::world() const {
    return editorWorld_;
}

std::pair<int, int> EditorModel::toLocal(int col, int row) const {
    auto [centerCol, centerRow] = editorWorld_.gridMap.center();
    return {col - centerCol, row - centerRow};
}

void EditorModel::repaintOrganism() {
    editorWorld_.gridMap.fillGrid(CellState::Empty);
    organism_->updateGrid();
}

SimulationGuiModel::SimulationGuiModel(int cols, int rows, int cellSize, std::uint32_t seed)
    : worldDocument_(cols, rows, cellSize, seed), editor_(13, seed + 1) {
    settings_.cellSize = cellSize;
    runtime_.setRunning(settings_.running);
    runtime_.setTargetFps(settings_.targetFps);
}

void SimulationGuiModel::setRunning(bool running) {
    settings_.running = running;
    runtime_.setRunning(running);
}

void SimulationGuiModel::toggleRunning() {
    runtime_.toggleRunning();
    settings_.running = runtime_.running();
}

void SimulationGuiModel::setTargetFps(int fps) {
    runtime_.setTargetFps(fps);
    settings_.targetFps = runtime_.targetFps();
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
    runtime_.advance(worldDocument_.world(), elapsed);
}

void SimulationGuiModel::stepOnce() {
    runtime_.stepOnce(worldDocument_.world());
}

void SimulationGuiModel::resetWorld(bool withOrigin) {
    runtime_.resetAccumulator();
    worldDocument_.reset(withOrigin);
}

void SimulationGuiModel::clearWorld() {
    resetWorld(false);
}

void SimulationGuiModel::resizeGridToCanvas(int canvasWidth, int canvasHeight, int cellSize) {
    settings_.cellSize = std::clamp(cellSize, 1, 50);
    worldDocument_.resizeToCanvas(canvasWidth, canvasHeight, settings_.cellSize);
}

void SimulationGuiModel::randomizeWalls(double densityPercent) {
    worldDocument_.randomizeWalls(densityPercent);
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

bool SimulationGuiModel::dropEditorOrganismAt(int col, int row) {
    auto organism = std::make_unique<Organism>(col, row, &worldDocument_.world(), &editor_.organism());
    if (!organism->isClear(col, row, organism->rotation)) {
        return false;
    }
    organism->species.reset();
    Organism& dropped = worldDocument_.world().addOrganism(std::move(organism));
    worldDocument_.world().fossilRecord.addSpecies(dropped);
    return true;
}

void SimulationGuiModel::applyRulesFromSettings() {
    // Reserved for future UI state that needs validation before touching the core.
}

StatsSnapshot SimulationGuiModel::stats() const {
    StatsSnapshot snapshot;
    const WorldEnvironment& env = worldDocument_.world();
    snapshot.ticks = env.totalTicks;
    snapshot.organisms = env.organismCount();
    snapshot.species = env.fossilRecord.numExtantSpecies();
    snapshot.largestCellCount = env.largestCellCount;
    snapshot.resetCount = env.resetCount;
    snapshot.averageMutability = env.averageMutability();
    if (auto species = env.fossilRecord.mostPopulousSpecies()) {
        std::ostringstream out;
        out << species->name << " (" << species->population << ")";
        snapshot.topSpecies = out.str();
    } else {
        snapshot.topSpecies = "none";
    }
    return snapshot;
}

WorldEnvironment& SimulationGuiModel::world() {
    return worldDocument_.world();
}

const WorldEnvironment& SimulationGuiModel::world() const {
    return worldDocument_.world();
}

WorldDocument& SimulationGuiModel::worldDocument() {
    return worldDocument_;
}

const WorldDocument& SimulationGuiModel::worldDocument() const {
    return worldDocument_;
}

SimulationRuntime& SimulationGuiModel::runtime() {
    return runtime_;
}

const SimulationRuntime& SimulationGuiModel::runtime() const {
    return runtime_;
}

EditorModel& SimulationGuiModel::editor() {
    return editor_;
}

const EditorModel& SimulationGuiModel::editor() const {
    return editor_;
}

GuiSettings& SimulationGuiModel::settings() {
    return settings_;
}

const GuiSettings& SimulationGuiModel::settings() const {
    return settings_;
}

void SimulationGuiModel::applyCellBrush(int col, int row, CellState state, bool killBlocking, CellState ignoredState) {
    WorldEnvironment& world = worldDocument_.world();
    for (int dc = -settings_.brushSize; dc <= settings_.brushSize; ++dc) {
        for (int dr = -settings_.brushSize; dr <= settings_.brushSize; ++dr) {
            GridCell* cell = world.gridMap.cellAt(col + dc, row + dr);
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
            world.changeCellUnchecked(cell->col, cell->row, state, nullptr);
        }
    }
    world.clearDeadOrganisms();
}

void SimulationGuiModel::killBrush(int col, int row) {
    WorldEnvironment& world = worldDocument_.world();
    std::unordered_set<Organism*> killed;
    int brushDiameter = (settings_.brushSize * 2) + 1;
    killed.reserve(static_cast<std::size_t>(brushDiameter * brushDiameter));
    for (int dc = -settings_.brushSize; dc <= settings_.brushSize; ++dc) {
        for (int dr = -settings_.brushSize; dr <= settings_.brushSize; ++dr) {
            GridCell* cell = world.gridMap.cellAt(col + dc, row + dr);
            if (cell != nullptr && cell->owner != nullptr && killed.insert(cell->owner).second) {
                cell->owner->die();
            }
        }
    }
    world.clearDeadOrganisms();
}

} // namespace lifeengine::gui
