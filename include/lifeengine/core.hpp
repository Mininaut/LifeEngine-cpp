#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lifeengine {

enum class CellState {
    Empty = 0,
    Food,
    Wall,
    Mouth,
    Producer,
    Mover,
    Killer,
    Armor,
    Eye,
};

constexpr std::size_t CellStateCount = 9;

const char* stateName(CellState state);
CellState stateFromName(const std::string& name);
std::size_t stateIndex(CellState state);
const std::array<CellState, 6>& livingStates();

enum class Direction {
    Up = 0,
    Right = 1,
    Down = 2,
    Left = 3,
};

Direction opposite(Direction dir);
Direction rotateRight(Direction dir);
std::pair<int, int> directionScalar(Direction dir);

class Random {
public:
    explicit Random(std::uint32_t seed = std::random_device{}());

    void seed(std::uint32_t value);
    bool chancePercent(double percent);
    double unit();
    int intExclusive(int upper);
    int intInclusive(int lower, int upper);
    Direction direction();
    std::pair<int, int> scalarDirection();

private:
    std::mt19937 engine_;
};

struct Hyperparameters {
    int lifespanMultiplier = 100;
    double foodProdProb = 5.0;
    std::vector<std::pair<int, int>> killableNeighbors;
    std::vector<std::pair<int, int>> edibleNeighbors;
    std::vector<std::pair<int, int>> growableNeighbors;

    bool useGlobalMutability = false;
    int globalMutability = 5;
    double addProb = 33.0;
    double changeProb = 33.0;
    double removeProb = 33.0;

    bool rotationEnabled = true;
    bool foodBlocksReproduction = true;
    bool moversCanProduce = false;
    bool instaKill = false;
    int lookRange = 20;
    bool seeThroughSelf = false;
    double foodDropProb = 0.0;
    int extraMoverFoodCost = 0;
    int maxOrganisms = -1;

    Hyperparameters();
};

struct WorldConfig {
    bool clearWallsOnReset = false;
    bool autoReset = true;
    bool autoPause = false;
};

class Organism;
class BodyCell;
class WorldEnvironment;

struct GridCell {
    CellState state = CellState::Empty;
    int col = 0;
    int row = 0;
    int x = 0;
    int y = 0;
    Organism* owner = nullptr;
    BodyCell* cellOwner = nullptr;

    GridCell() = default;
    GridCell(CellState state, int col, int row, int x, int y);
    void setType(CellState nextState);
};

class GridMap {
public:
    GridMap(int cols = 0, int rows = 0, int cellSize = 1);

    void resize(int cols, int rows, int cellSize);
    void fillGrid(CellState state, bool ignoreWalls = false);
    GridCell* cellAt(int col, int row) {
        if (!isValidLoc(col, row)) {
            return nullptr;
        }
        return &grid_[static_cast<std::size_t>(col * rows + row)];
    }

    const GridCell* cellAt(int col, int row) const {
        if (!isValidLoc(col, row)) {
            return nullptr;
        }
        return &grid_[static_cast<std::size_t>(col * rows + row)];
    }

    void setCellType(int col, int row, CellState state);
    void setCellOwner(int col, int row, BodyCell* cellOwner);

    bool isValidLoc(int col, int row) const {
        return col < cols && row < rows && col >= 0 && row >= 0;
    }

    std::pair<int, int> center() const;
    std::pair<int, int> xyToColRow(int x, int y) const;

    int cols = 0;
    int rows = 0;
    int cellSize = 1;

private:
    std::vector<GridCell> grid_;
};

struct Observation {
    GridCell* cell = nullptr;
    int distance = 0;
    Direction direction = Direction::Up;
};

enum class Decision {
    Neutral = 0,
    Retreat = 1,
    Chase = 2,
};

class Brain {
public:
    explicit Brain(Organism* owner = nullptr);

    void setOwner(Organism* owner);
    void copyFrom(const Brain& other);
    void randomizeDecisions(Random& rng, bool randomizeAll = false);
    void observe(const Observation& observation);
    bool decide();
    void mutate(Random& rng);
    Decision decisionFor(CellState state) const;
    void setDecision(CellState state, Decision decision);

private:
    Decision randomDecision(Random& rng) const;

    Organism* owner_ = nullptr;
    std::array<Decision, CellStateCount> decisions_{};
    std::vector<Observation> observations_;
};

class Anatomy {
public:
    explicit Anatomy(Organism* owner = nullptr);

    void setOwner(Organism* owner);
    void clear();
    bool canAddCellAt(int col, int row) const;
    BodyCell* addDefaultCell(CellState state, int col, int row);
    BodyCell* addRandomizedCell(CellState state, int col, int row, Random& rng);
    BodyCell* addInheritedCell(const BodyCell& parent);
    BodyCell* replaceCell(CellState state, int col, int row, bool randomize, Random& rng);
    bool removeCell(int col, int row, bool allowCenterRemoval = false);
    BodyCell* getLocalCell(int col, int row);
    const BodyCell* getLocalCell(int col, int row) const;
    std::vector<BodyCell*> neighborsOfCell(int col, int row);
    BodyCell* randomCell(Random& rng);
    const BodyCell* randomCell(Random& rng) const;
    bool isEqual(const Anatomy& other) const;
    void checkTypeChange();
    void updateBirthDistance(int locCol, int locRow);

    const std::vector<std::unique_ptr<BodyCell>>& cells() const;
    std::vector<std::unique_ptr<BodyCell>>& cells();

    bool isProducer = false;
    bool isMover = false;
    bool hasEyes = false;
    int birthDistance = 4;

private:
    Organism* owner_ = nullptr;
    std::vector<std::unique_ptr<BodyCell>> cells_;
};

class BodyCell {
public:
    BodyCell(CellState state, Organism* org, int locCol, int locRow);

    void initInherit(const BodyCell& parent);
    void initRandom(Random& rng);
    void initDefault();
    void performFunction();
    int realCol() const;
    int realRow() const;
    GridCell* realCell() const;
    int rotatedCol(Direction dir) const;
    int rotatedRow(Direction dir) const;
    Direction absoluteDirection() const;
    Observation look() const;

    CellState state = CellState::Empty;
    Organism* org = nullptr;
    int locCol = 0;
    int locRow = 0;
    Direction direction = Direction::Up;
};

struct Species {
    std::string name;
    int population = 1;
    int cumulativePop = 1;
    int startTick = 0;
    int endTick = -1;
    bool extinct = false;
    std::array<int, CellStateCount> cellCounts{};

    Species();
    Species(const Anatomy& anatomy, int startTick);
    void calcAnatomyDetails(const Anatomy& anatomy);
    void addPop();
    int lifespan() const;
};

class FossilRecord {
public:
    void setEnv(WorldEnvironment* env);
    std::shared_ptr<Species> addSpecies(Organism& org, const std::shared_ptr<Species>& ancestor = nullptr);
    void addSpeciesObj(const std::shared_ptr<Species>& species);
    bool fossilize(const std::shared_ptr<Species>& species);
    void decreasePop(const std::shared_ptr<Species>& species);
    int numExtantSpecies() const;
    int numExtinctSpecies() const;
    std::shared_ptr<Species> mostPopulousSpecies() const;
    void clearRecord();
    void updateData();
    void calcCellCountAverages();

    int minDiscard = 10;
    std::size_t recordSizeLimit = 500;
    std::vector<int> tickRecord;
    std::vector<int> popCounts;
    std::vector<int> speciesCounts;
    std::vector<double> averageMutRates;
    std::vector<double> averageCells;
    std::vector<std::array<double, CellStateCount>> averageCellCounts;
    std::unordered_map<std::string, std::shared_ptr<Species>> extantSpecies;
    std::unordered_map<std::string, std::shared_ptr<Species>> extinctSpecies;

private:
    WorldEnvironment* env_ = nullptr;
};

class Organism {
public:
    Organism(int col, int row, WorldEnvironment* env, const Organism* parent = nullptr);

    void inherit(const Organism& parent);
    int foodNeeded() const;
    int lifespan() const;
    int maxHealth() const;
    void reproduce();
    bool mutate();
    bool calcRandomChance(double percent);
    bool attemptMove();
    bool attemptRotate();
    void changeDirection(Direction dir);
    bool isStraightPath(int c1, int r1, int c2, int r2, const Organism* parent) const;
    bool isPassableCell(const GridCell* cell, const Organism* parent) const;
    bool isClear(int col, int row, Direction rotation) const;
    void harm();
    void die();
    void updateGrid();
    bool update();
    GridCell* realCell(const BodyCell& localCell, int col, int row, Direction rotation) const;
    bool isNatural() const;

    int c = 0;
    int r = 0;
    WorldEnvironment* env = nullptr;
    int lifetime = 0;
    int foodCollected = 0;
    bool living = true;
    Anatomy anatomy;
    Direction direction = Direction::Down;
    Direction rotation = Direction::Up;
    bool canRotate = true;
    int moveCount = 0;
    int moveRange = 4;
    int ignoreBrainFor = 0;
    int mutability = 5;
    int damage = 0;
    Brain brain;
    std::shared_ptr<Species> species;
};

class WorldEnvironment {
public:
    WorldEnvironment(int cellSize, int cols, int rows, std::uint32_t seed = 1);

    void update();
    void removeOrganisms(const std::vector<std::size_t>& organismIndexes);
    void originOfLife();
    Organism& addOrganism(std::unique_ptr<Organism> organism);
    bool canAddOrganism() const;
    double averageMutability() const;
    void changeCell(int col, int row, CellState state, BodyCell* owner);
    void clearWalls();
    void clearOrganisms();
    void clearDeadOrganisms();
    void generateFood();
    bool reset(bool resetLife = true);
    int organismCount() const;

    GridMap gridMap;
    std::vector<std::unique_ptr<Organism>> organisms;
    std::vector<std::pair<int, int>> walls;
    int totalMutability = 0;
    int largestCellCount = 0;
    int resetCount = 0;
    int totalTicks = 0;
    int dataUpdateRate = 100;
    Hyperparameters hyper;
    WorldConfig config;
    Random rng;
    FossilRecord fossilRecord;

private:
    void compactOrganismsFromIndexes(std::vector<std::size_t>& organismIndexes);

    std::vector<std::size_t> removalScratch_;
};

std::unique_ptr<Organism> generateRandomOrganism(WorldEnvironment& env, int organismLayers = 4, double cellSpawnChance = 0.75);

} // namespace lifeengine
