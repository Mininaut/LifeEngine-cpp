#include "lifeengine/core.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lifeengine {
namespace {

const std::array<std::pair<int, int>, 4> kAdjacent = {
    std::make_pair(0, 1),
    std::make_pair(0, -1),
    std::make_pair(1, 0),
    std::make_pair(-1, 0),
};

const std::array<std::pair<int, int>, 8> kAllNeighbors = {
    std::make_pair(0, 1),
    std::make_pair(0, -1),
    std::make_pair(1, 0),
    std::make_pair(-1, 0),
    std::make_pair(-1, -1),
    std::make_pair(1, 1),
    std::make_pair(-1, 1),
    std::make_pair(1, -1),
};

const std::array<CellState, CellStateCount> kAllStates = {
    CellState::Empty,
    CellState::Food,
    CellState::Wall,
    CellState::Mouth,
    CellState::Producer,
    CellState::Mover,
    CellState::Killer,
    CellState::Armor,
    CellState::Eye,
};

const std::array<CellState, 6> kLivingStates = {
    CellState::Mouth,
    CellState::Producer,
    CellState::Mover,
    CellState::Killer,
    CellState::Armor,
    CellState::Eye,
};

const std::array<Direction, 4> kOppositeDirections = {
    Direction::Down,
    Direction::Left,
    Direction::Up,
    Direction::Right,
};

const std::array<Direction, 4> kRotateRightDirections = {
    Direction::Right,
    Direction::Down,
    Direction::Left,
    Direction::Up,
};

const std::array<std::pair<int, int>, 4> kDirectionScalars = {
    std::make_pair(0, -1),
    std::make_pair(1, 0),
    std::make_pair(0, 1),
    std::make_pair(-1, 0),
};

std::string randomSpeciesName(Random& rng) {
    static constexpr char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string name;
    name.reserve(10);
    for (int i = 0; i < 10; ++i) {
        name.push_back(alphabet[rng.intExclusive(36)]);
    }
    return name;
}

Decision randomDecisionValue(Random& rng) {
    return static_cast<Decision>(rng.intExclusive(3));
}

} // namespace

const char* stateName(CellState state) {
    switch (state) {
    case CellState::Empty:
        return "empty";
    case CellState::Food:
        return "food";
    case CellState::Wall:
        return "wall";
    case CellState::Mouth:
        return "mouth";
    case CellState::Producer:
        return "producer";
    case CellState::Mover:
        return "mover";
    case CellState::Killer:
        return "killer";
    case CellState::Armor:
        return "armor";
    case CellState::Eye:
        return "eye";
    }
    return "empty";
}

CellState stateFromName(const std::string& name) {
    for (CellState state : kAllStates) {
        if (name == stateName(state)) {
            return state;
        }
    }
    throw std::invalid_argument("unknown cell state: " + name);
}

std::size_t stateIndex(CellState state) {
    return static_cast<std::size_t>(state);
}

const std::array<CellState, 6>& livingStates() {
    return kLivingStates;
}

Direction opposite(Direction dir) {
    return kOppositeDirections[static_cast<std::size_t>(dir)];
}

Direction rotateRight(Direction dir) {
    return kRotateRightDirections[static_cast<std::size_t>(dir)];
}

std::pair<int, int> directionScalar(Direction dir) {
    return kDirectionScalars[static_cast<std::size_t>(dir)];
}

Random::Random(std::uint32_t seedValue) : engine_(seedValue) {}

void Random::seed(std::uint32_t value) {
    engine_.seed(value);
}

bool Random::chancePercent(double percent) {
    if (percent <= 0.0) {
        return false;
    }
    if (percent >= 100.0) {
        return true;
    }
    std::uniform_real_distribution<double> dist(0.0, 100.0);
    return dist(engine_) < percent;
}

double Random::unit() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(engine_);
}

int Random::intExclusive(int upper) {
    std::uniform_int_distribution<int> dist(0, upper - 1);
    return dist(engine_);
}

int Random::intInclusive(int lower, int upper) {
    std::uniform_int_distribution<int> dist(lower, upper);
    return dist(engine_);
}

Direction Random::direction() {
    return static_cast<Direction>(intExclusive(4));
}

std::pair<int, int> Random::scalarDirection() {
    return directionScalar(direction());
}

Hyperparameters::Hyperparameters() {
    killableNeighbors.assign(kAdjacent.begin(), kAdjacent.end());
    edibleNeighbors.assign(kAdjacent.begin(), kAdjacent.end());
    growableNeighbors.assign(kAdjacent.begin(), kAdjacent.end());
}

GridCell::GridCell(CellState initialState, int initialCol, int initialRow)
    : state(initialState), col(initialCol), row(initialRow) {}

void GridCell::setType(CellState nextState) {
    state = nextState;
}

GridMap::GridMap(int initialCols, int initialRows, int initialCellSize) {
    resize(initialCols, initialRows, initialCellSize);
}

void GridMap::resize(int nextCols, int nextRows, int nextCellSize) {
    cols = nextCols;
    rows = nextRows;
    cellSize = nextCellSize;
    grid_.clear();
    grid_.reserve(static_cast<std::size_t>(cols * rows));
    for (int c = 0; c < cols; ++c) {
        for (int r = 0; r < rows; ++r) {
            grid_.emplace_back(CellState::Empty, c, r);
        }
    }
}

void GridMap::fillGrid(CellState state, bool ignoreWalls) {
    for (auto& cell : grid_) {
        if (ignoreWalls && cell.state == CellState::Wall) {
            continue;
        }
        cell.setType(state);
        cell.owner = nullptr;
        cell.cellOwner = nullptr;
    }
}

void GridMap::setCellType(int col, int row, CellState state) {
    if (GridCell* cell = cellAt(col, row)) {
        cell->setType(state);
    }
}

void GridMap::setCellOwner(int col, int row, BodyCell* cellOwner) {
    if (GridCell* cell = cellAt(col, row)) {
        cell->cellOwner = cellOwner;
        cell->owner = cellOwner == nullptr ? nullptr : cellOwner->org;
    }
}

std::pair<int, int> GridMap::center() const {
    return {cols / 2, rows / 2};
}

std::pair<int, int> GridMap::xyToColRow(int x, int y) const {
    int col = x / cellSize;
    int row = y / cellSize;
    col = std::clamp(col, 0, cols - 1);
    row = std::clamp(row, 0, rows - 1);
    return {col, row};
}

Brain::Brain(Organism* owner) : owner_(owner) {
    observations_.reserve(4);
    decisions_.fill(Decision::Neutral);
    decisions_[stateIndex(CellState::Food)] = Decision::Chase;
    decisions_[stateIndex(CellState::Killer)] = Decision::Retreat;
}

void Brain::setOwner(Organism* owner) {
    owner_ = owner;
}

void Brain::copyFrom(const Brain& other) {
    decisions_ = other.decisions_;
}

void Brain::randomizeDecisions(Random& rng, bool randomizeAll) {
    if (randomizeAll) {
        decisions_[stateIndex(CellState::Food)] = randomDecision(rng);
        decisions_[stateIndex(CellState::Killer)] = randomDecision(rng);
    }
    decisions_[stateIndex(CellState::Mouth)] = randomDecision(rng);
    decisions_[stateIndex(CellState::Producer)] = randomDecision(rng);
    decisions_[stateIndex(CellState::Mover)] = randomDecision(rng);
    decisions_[stateIndex(CellState::Armor)] = randomDecision(rng);
    decisions_[stateIndex(CellState::Eye)] = randomDecision(rng);
}

void Brain::observe(const Observation& observation) {
    observations_.push_back(observation);
}

bool Brain::decide() {
    Decision decision = Decision::Neutral;
    int closest = owner_->env->hyper.lookRange + 1;
    Direction moveDirection = Direction::Up;
    for (const Observation& observation : observations_) {
        if (observation.cell == nullptr || observation.cell->owner == owner_) {
            continue;
        }
        if (observation.distance < closest) {
            decision = decisions_[stateIndex(observation.cell->state)];
            moveDirection = observation.direction;
            closest = observation.distance;
        }
    }
    observations_.clear();
    if (decision == Decision::Chase) {
        owner_->changeDirection(moveDirection);
        return true;
    }
    if (decision == Decision::Retreat) {
        owner_->changeDirection(opposite(moveDirection));
        return true;
    }
    return false;
}

void Brain::mutate(Random& rng) {
    decisions_[stateIndex(kAllStates[rng.intExclusive(static_cast<int>(kAllStates.size()))])] = randomDecision(rng);
    decisions_[stateIndex(CellState::Empty)] = Decision::Neutral;
}

Decision Brain::decisionFor(CellState state) const {
    return decisions_[stateIndex(state)];
}

void Brain::setDecision(CellState state, Decision decision) {
    decisions_[stateIndex(state)] = decision;
}

Decision Brain::randomDecision(Random& rng) const {
    return randomDecisionValue(rng);
}

Anatomy::Anatomy(Organism* owner) : owner_(owner) {}

void Anatomy::setOwner(Organism* owner) {
    owner_ = owner;
    for (auto& cell : cells_) {
        cell->org = owner;
    }
}

void Anatomy::clear() {
    cells_.clear();
    isProducer = false;
    isMover = false;
    hasEyes = false;
    birthDistance = 4;
}

void Anatomy::reserveCells(std::size_t count) {
    cells_.reserve(count);
}

bool Anatomy::canAddCellAt(int col, int row) const {
    return getLocalCell(col, row) == nullptr;
}

BodyCell* Anatomy::addDefaultCell(CellState state, int col, int row) {
    auto cell = std::make_unique<BodyCell>(state, owner_, col, row);
    cell->initDefault();
    BodyCell* result = cell.get();
    cells_.push_back(std::move(cell));
    markTypePresent(state);
    return result;
}

BodyCell* Anatomy::addRandomizedCell(CellState state, int col, int row, Random& rng) {
    if (state == CellState::Eye && !hasEyes && owner_ != nullptr) {
        owner_->brain.randomizeDecisions(rng);
    }
    auto cell = std::make_unique<BodyCell>(state, owner_, col, row);
    cell->initRandom(rng);
    BodyCell* result = cell.get();
    cells_.push_back(std::move(cell));
    markTypePresent(state);
    return result;
}

BodyCell* Anatomy::addInheritedCell(const BodyCell& parent) {
    auto cell = std::make_unique<BodyCell>(parent.state, owner_, parent.locCol, parent.locRow);
    cell->initInherit(parent);
    BodyCell* result = cell.get();
    cells_.push_back(std::move(cell));
    markTypePresent(parent.state);
    return result;
}

BodyCell* Anatomy::replaceCell(CellState state, int col, int row, bool randomize, Random& rng) {
    removeCell(col, row, true);
    if (randomize) {
        return addRandomizedCell(state, col, row, rng);
    }
    return addDefaultCell(state, col, row);
}

bool Anatomy::removeCell(int col, int row, bool allowCenterRemoval) {
    if (col == 0 && row == 0 && !allowCenterRemoval) {
        return false;
    }
    for (auto it = cells_.begin(); it != cells_.end(); ++it) {
        if ((*it)->locCol == col && (*it)->locRow == row) {
            cells_.erase(it);
            checkTypeChange();
            return true;
        }
    }
    checkTypeChange();
    return true;
}

BodyCell* Anatomy::getLocalCell(int col, int row) {
    for (auto& cell : cells_) {
        if (cell->locCol == col && cell->locRow == row) {
            return cell.get();
        }
    }
    return nullptr;
}

const BodyCell* Anatomy::getLocalCell(int col, int row) const {
    for (const auto& cell : cells_) {
        if (cell->locCol == col && cell->locRow == row) {
            return cell.get();
        }
    }
    return nullptr;
}

bool Anatomy::hasNeighborAt(int col, int row) const {
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            if (getLocalCell(col + x, row + y) != nullptr) {
                return true;
            }
        }
    }
    return false;
}

std::vector<BodyCell*> Anatomy::neighborsOfCell(int col, int row) {
    std::vector<BodyCell*> neighbors;
    neighbors.reserve(9);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            if (BodyCell* neighbor = getLocalCell(col + x, row + y)) {
                neighbors.push_back(neighbor);
            }
        }
    }
    return neighbors;
}

BodyCell* Anatomy::randomCell(Random& rng) {
    if (cells_.empty()) {
        return nullptr;
    }
    return cells_[rng.intExclusive(static_cast<int>(cells_.size()))].get();
}

const BodyCell* Anatomy::randomCell(Random& rng) const {
    if (cells_.empty()) {
        return nullptr;
    }
    return cells_[rng.intExclusive(static_cast<int>(cells_.size()))].get();
}

bool Anatomy::isEqual(const Anatomy& other) const {
    if (cells_.size() != other.cells_.size()) {
        return false;
    }
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        const BodyCell& mine = *cells_[i];
        const BodyCell& theirs = *other.cells_[i];
        if (mine.locCol != theirs.locCol || mine.locRow != theirs.locRow || mine.state != theirs.state) {
            return false;
        }
    }
    return true;
}

void Anatomy::checkTypeChange() {
    isProducer = false;
    isMover = false;
    hasEyes = false;
    for (const auto& cell : cells_) {
        markTypePresent(cell->state);
    }
}

void Anatomy::updateBirthDistance(int locCol, int locRow) {
    int distance = std::max(std::abs(locRow) * 2 + 2, std::abs(locCol) * 2 + 2);
    birthDistance = std::max(birthDistance, distance);
}

const std::vector<std::unique_ptr<BodyCell>>& Anatomy::cells() const {
    return cells_;
}

std::vector<std::unique_ptr<BodyCell>>& Anatomy::cells() {
    return cells_;
}

void Anatomy::markTypePresent(CellState state) {
    if (state == CellState::Producer) {
        isProducer = true;
    } else if (state == CellState::Mover) {
        isMover = true;
    } else if (state == CellState::Eye) {
        hasEyes = true;
    }
}

BodyCell::BodyCell(CellState initialState, Organism* initialOrg, int initialLocCol, int initialLocRow)
    : state(initialState), org(initialOrg), locCol(initialLocCol), locRow(initialLocRow) {
    if (org != nullptr) {
        org->anatomy.updateBirthDistance(locCol, locRow);
    }
}

void BodyCell::initInherit(const BodyCell& parent) {
    locCol = parent.locCol;
    locRow = parent.locRow;
    direction = parent.direction;
}

void BodyCell::initRandom(Random& rng) {
    if (state == CellState::Eye) {
        direction = rng.direction();
    }
}

void BodyCell::initDefault() {
    if (state == CellState::Eye) {
        direction = Direction::Up;
    }
}

void BodyCell::performFunction() {
    WorldEnvironment& env = *org->env;
    switch (state) {
    case CellState::Mouth: {
        int realC = realCol();
        int realR = realRow();
        for (const auto& loc : env.hyper.edibleNeighbors) {
            GridCell* cell = env.gridMap.cellAt(realC + loc.first, realR + loc.second);
            if (cell != nullptr && cell->state == CellState::Food) {
                env.changeCellUnchecked(cell->col, cell->row, CellState::Empty, nullptr);
                ++org->foodCollected;
            }
        }
        break;
    }
    case CellState::Producer: {
        if (org->anatomy.isMover && !env.hyper.moversCanProduce) {
            return;
        }
        if (env.rng.chancePercent(env.hyper.foodProdProb)) {
            int realC = realCol();
            int realR = realRow();
            const auto& neighbors = env.hyper.growableNeighbors;
            auto loc = neighbors[env.rng.intExclusive(static_cast<int>(neighbors.size()))];
            GridCell* cell = env.gridMap.cellAt(realC + loc.first, realR + loc.second);
            if (cell != nullptr && cell->state == CellState::Empty) {
                env.changeCellUnchecked(cell->col, cell->row, CellState::Food, nullptr);
            }
        }
        break;
    }
    case CellState::Killer: {
        int c = realCol();
        int r = realRow();
        for (const auto& loc : env.hyper.killableNeighbors) {
            GridCell* neighbor = env.gridMap.cellAt(c + loc.first, r + loc.second);
            if (neighbor == nullptr || neighbor->owner == nullptr || neighbor->owner == org ||
                !neighbor->owner->living || neighbor->state == CellState::Armor) {
                continue;
            }
            bool hitKiller = neighbor->state == CellState::Killer;
            neighbor->owner->harm();
            if (env.hyper.instaKill && hitKiller) {
                org->harm();
            }
        }
        break;
    }
    case CellState::Eye:
        org->brain.observe(look());
        break;
    default:
        break;
    }
}

int BodyCell::realCol() const {
    return org->c + rotatedCol(org->rotation);
}

int BodyCell::realRow() const {
    return org->r + rotatedRow(org->rotation);
}

GridCell* BodyCell::realCell() const {
    auto [offsetCol, offsetRow] = rotatedOffset(org->rotation);
    return org->env->gridMap.cellAt(org->c + offsetCol, org->r + offsetRow);
}

std::pair<int, int> BodyCell::rotatedOffset(Direction dir) const {
    switch (dir) {
    case Direction::Up:
        return {locCol, locRow};
    case Direction::Down:
        return {-locCol, -locRow};
    case Direction::Left:
        return {locRow, -locCol};
    case Direction::Right:
        return {-locRow, locCol};
    }
    return {locCol, locRow};
}

int BodyCell::rotatedCol(Direction dir) const {
    auto [offsetCol, offsetRow] = rotatedOffset(dir);
    (void)offsetRow;
    return offsetCol;
}

int BodyCell::rotatedRow(Direction dir) const {
    auto [offsetCol, offsetRow] = rotatedOffset(dir);
    (void)offsetCol;
    return offsetRow;
}

Direction BodyCell::absoluteDirection() const {
    return static_cast<Direction>((static_cast<int>(org->rotation) + static_cast<int>(direction)) % 4);
}

Observation BodyCell::look() const {
    WorldEnvironment& env = *org->env;
    Direction dir = absoluteDirection();
    auto [addCol, addRow] = directionScalar(dir);
    int startCol = realCol();
    int startRow = realRow();
    int col = startCol;
    int row = startRow;
    GridCell* cell = nullptr;
    for (int i = 0; i < env.hyper.lookRange; ++i) {
        col += addCol;
        row += addRow;
        cell = env.gridMap.cellAt(col, row);
        if (cell == nullptr) {
            break;
        }
        if (cell->owner == org && env.hyper.seeThroughSelf) {
            continue;
        }
        if (cell->state != CellState::Empty) {
            int distance = std::abs(startCol - col) + std::abs(startRow - row);
            return {cell, distance, dir};
        }
    }
    return {cell, env.hyper.lookRange, dir};
}

Species::Species() = default;

Species::Species(const Anatomy& anatomy, int initialStartTick) : startTick(initialStartTick) {
    calcAnatomyDetails(anatomy);
}

void Species::calcAnatomyDetails(const Anatomy& anatomy) {
    cellCounts.fill(0);
    for (const auto& cell : anatomy.cells()) {
        ++cellCounts[stateIndex(cell->state)];
    }
}

void Species::addPop() {
    ++population;
    ++cumulativePop;
}

int Species::lifespan() const {
    return endTick - startTick;
}

void FossilRecord::setEnv(WorldEnvironment* env) {
    env_ = env;
    tickRecord.clear();
    popCounts.clear();
    speciesCounts.clear();
    averageMutRates.clear();
    averageCells.clear();
    averageCellCounts.clear();
    reserveRecordCapacity();
    updateData();
}

std::shared_ptr<Species> FossilRecord::addSpecies(Organism& org, const std::shared_ptr<Species>&) {
    auto species = std::make_shared<Species>(org.anatomy, env_ == nullptr ? 0 : env_->totalTicks);
    species->name = env_ == nullptr ? "species" : randomSpeciesName(env_->rng);
    extantSpecies[species->name] = species;
    org.species = species;
    return species;
}

void FossilRecord::addSpeciesObj(const std::shared_ptr<Species>& species) {
    if (species != nullptr) {
        extantSpecies[species->name] = species;
    }
}

bool FossilRecord::fossilize(const std::shared_ptr<Species>& species) {
    if (species == nullptr || extantSpecies.find(species->name) == extantSpecies.end()) {
        return false;
    }
    species->endTick = env_ == nullptr ? species->endTick : env_->totalTicks;
    extantSpecies.erase(species->name);
    if (species->cumulativePop >= minDiscard) {
        extinctSpecies[species->name] = species;
        return true;
    }
    return false;
}

void FossilRecord::decreasePop(const std::shared_ptr<Species>& species) {
    if (species == nullptr) {
        return;
    }
    --species->population;
    if (species->population <= 0) {
        species->extinct = true;
        fossilize(species);
    }
}

int FossilRecord::numExtantSpecies() const {
    return static_cast<int>(extantSpecies.size());
}

int FossilRecord::numExtinctSpecies() const {
    return static_cast<int>(extinctSpecies.size());
}

std::shared_ptr<Species> FossilRecord::mostPopulousSpecies() const {
    std::shared_ptr<Species> best;
    int maxPop = 0;
    for (const auto& [name, species] : extantSpecies) {
        (void)name;
        if (species->population > maxPop) {
            maxPop = species->population;
            best = species;
        }
    }
    return best;
}

void FossilRecord::clearRecord() {
    extantSpecies.clear();
    extinctSpecies.clear();
    tickRecord.clear();
    popCounts.clear();
    speciesCounts.clear();
    averageMutRates.clear();
    averageCells.clear();
    averageCellCounts.clear();
    reserveRecordCapacity();
    updateData();
}

void FossilRecord::updateData() {
    if (env_ == nullptr) {
        return;
    }
    reserveRecordCapacity();
    tickRecord.push_back(env_->totalTicks);
    popCounts.push_back(static_cast<int>(env_->organisms.size()));
    speciesCounts.push_back(numExtantSpecies());
    averageMutRates.push_back(env_->averageMutability());
    calcCellCountAverages();
    if (tickRecord.size() > recordSizeLimit) {
        std::size_t overflow = tickRecord.size() - recordSizeLimit;
        tickRecord.erase(tickRecord.begin(), tickRecord.begin() + static_cast<std::ptrdiff_t>(overflow));
        popCounts.erase(popCounts.begin(), popCounts.begin() + static_cast<std::ptrdiff_t>(overflow));
        speciesCounts.erase(speciesCounts.begin(), speciesCounts.begin() + static_cast<std::ptrdiff_t>(overflow));
        averageMutRates.erase(averageMutRates.begin(), averageMutRates.begin() + static_cast<std::ptrdiff_t>(overflow));
        averageCells.erase(averageCells.begin(), averageCells.begin() + static_cast<std::ptrdiff_t>(overflow));
        averageCellCounts.erase(averageCellCounts.begin(), averageCellCounts.begin() + static_cast<std::ptrdiff_t>(overflow));
    }
}

void FossilRecord::calcCellCountAverages() {
    std::array<double, CellStateCount> cellCounts{};
    double totalOrg = 0.0;
    bool first = true;
    int extantCount = numExtantSpecies();
    for (const auto& [name, species] : extantSpecies) {
        (void)name;
        if (!first && extantCount > 10 && species->cumulativePop < minDiscard) {
            continue;
        }
        for (CellState state : livingStates()) {
            cellCounts[stateIndex(state)] += static_cast<double>(species->cellCounts[stateIndex(state)] * species->population);
        }
        totalOrg += species->population;
        first = false;
    }
    if (totalOrg == 0.0) {
        averageCells.push_back(0.0);
        averageCellCounts.push_back(cellCounts);
        return;
    }
    double totalCells = 0.0;
    for (CellState state : livingStates()) {
        totalCells += cellCounts[stateIndex(state)];
        cellCounts[stateIndex(state)] /= totalOrg;
    }
    averageCells.push_back(totalCells / totalOrg);
    averageCellCounts.push_back(cellCounts);
}

void FossilRecord::reserveRecordCapacity() {
    if (recordSizeLimit == 0) {
        return;
    }
    if (tickRecord.capacity() >= recordSizeLimit &&
        popCounts.capacity() >= recordSizeLimit &&
        speciesCounts.capacity() >= recordSizeLimit &&
        averageMutRates.capacity() >= recordSizeLimit &&
        averageCells.capacity() >= recordSizeLimit &&
        averageCellCounts.capacity() >= recordSizeLimit) {
        return;
    }
    tickRecord.reserve(recordSizeLimit);
    popCounts.reserve(recordSizeLimit);
    speciesCounts.reserve(recordSizeLimit);
    averageMutRates.reserve(recordSizeLimit);
    averageCells.reserve(recordSizeLimit);
    averageCellCounts.reserve(recordSizeLimit);
}

Organism::Organism(int col, int row, WorldEnvironment* initialEnv, const Organism* parent)
    : c(col),
      r(row),
      env(initialEnv),
      anatomy(this),
      canRotate(initialEnv == nullptr ? true : initialEnv->hyper.rotationEnabled),
      brain(this) {
    if (parent != nullptr) {
        inherit(*parent);
    }
}

void Organism::inherit(const Organism& parent) {
    moveRange = parent.moveRange;
    mutability = parent.mutability;
    species = parent.species;
    anatomy.reserveCells(parent.anatomy.cells().size());
    for (const auto& cell : parent.anatomy.cells()) {
        anatomy.addInheritedCell(*cell);
    }
    if (parent.anatomy.isMover && parent.anatomy.hasEyes) {
        brain.copyFrom(parent.brain);
    }
}

int Organism::foodNeeded() const {
    return anatomy.isMover ? static_cast<int>(anatomy.cells().size()) + env->hyper.extraMoverFoodCost
                           : static_cast<int>(anatomy.cells().size());
}

int Organism::lifespan() const {
    return static_cast<int>(anatomy.cells().size()) * env->hyper.lifespanMultiplier;
}

int Organism::maxHealth() const {
    return static_cast<int>(anatomy.cells().size());
}

void Organism::reproduce() {
    auto child = std::make_unique<Organism>(0, 0, env, this);
    if (env->hyper.rotationEnabled) {
        child->rotation = env->rng.direction();
    }
    int probability = mutability;
    if (env->hyper.useGlobalMutability) {
        probability = env->hyper.globalMutability;
    } else {
        if (env->rng.chancePercent(50.0)) {
            ++child->mutability;
        } else {
            child->mutability = std::max(1, child->mutability - 1);
        }
    }

    bool mutated = false;
    if (env->rng.chancePercent(probability)) {
        if (child->anatomy.isMover && env->rng.chancePercent(10.0)) {
            if (child->anatomy.hasEyes) {
                child->brain.mutate(env->rng);
            }
            child->moveRange += env->rng.intInclusive(-2, 2);
            child->moveRange = std::max(1, child->moveRange);
        } else {
            mutated = child->mutate();
        }
    }

    auto directionStep = env->rng.scalarDirection();
    int offset = env->rng.intInclusive(0, 2);
    int baseMovement = anatomy.birthDistance;
    int newC = c + (directionStep.first * baseMovement) + (directionStep.first * offset);
    int newR = r + (directionStep.second * baseMovement) + (directionStep.second * offset);

    if (child->isClear(newC, newR, child->rotation) &&
        child->isStraightPath(newC, newR, c, r, this) &&
        env->canAddOrganism()) {
        child->c = newC;
        child->r = newR;
        Organism& childRef = env->addOrganism(std::move(child));
        if (mutated) {
            env->fossilRecord.addSpecies(childRef, species);
        } else if (childRef.species != nullptr) {
            childRef.species->addPop();
        }
    }
    foodCollected = std::max(foodCollected - foodNeeded(), 0);
}

bool Organism::mutate() {
    bool added = false;
    bool changed = false;
    bool removed = false;
    if (calcRandomChance(env->hyper.addProb)) {
        BodyCell* branch = anatomy.randomCell(env->rng);
        CellState state = livingStates()[env->rng.intExclusive(static_cast<int>(livingStates().size()))];
        auto growthDirection = kAllNeighbors[env->rng.intExclusive(static_cast<int>(kAllNeighbors.size()))];
        int col = branch->locCol + growthDirection.first;
        int row = branch->locRow + growthDirection.second;
        if (anatomy.canAddCellAt(col, row)) {
            added = true;
            anatomy.addRandomizedCell(state, col, row, env->rng);
        }
    }
    if (calcRandomChance(env->hyper.changeProb)) {
        BodyCell* cell = anatomy.randomCell(env->rng);
        CellState state = livingStates()[env->rng.intExclusive(static_cast<int>(livingStates().size()))];
        anatomy.replaceCell(state, cell->locCol, cell->locRow, true, env->rng);
        changed = true;
    }
    if (calcRandomChance(env->hyper.removeProb)) {
        if (anatomy.cells().size() > 1) {
            BodyCell* cell = anatomy.randomCell(env->rng);
            removed = anatomy.removeCell(cell->locCol, cell->locRow);
        }
    }
    return added || changed || removed;
}

bool Organism::calcRandomChance(double percent) {
    return env->rng.chancePercent(percent);
}

bool Organism::attemptMove() {
    auto step = directionScalar(direction);
    int newC = c + step.first;
    int newR = r + step.second;
    if (isClear(newC, newR, rotation)) {
        for (const auto& cell : anatomy.cells()) {
            auto [offsetCol, offsetRow] = cell->rotatedOffset(rotation);
            env->changeCellUnchecked(c + offsetCol, r + offsetRow, CellState::Empty, nullptr);
        }
        c = newC;
        r = newR;
        updateGrid();
        return true;
    }
    return false;
}

bool Organism::attemptRotate() {
    if (!canRotate) {
        direction = env->rng.direction();
        moveCount = 0;
        return true;
    }
    Direction newRotation = env->rng.direction();
    if (isClear(c, r, newRotation)) {
        for (const auto& cell : anatomy.cells()) {
            auto [offsetCol, offsetRow] = cell->rotatedOffset(rotation);
            env->changeCellUnchecked(c + offsetCol, r + offsetRow, CellState::Empty, nullptr);
        }
        rotation = newRotation;
        direction = env->rng.direction();
        updateGrid();
        moveCount = 0;
        return true;
    }
    return false;
}

void Organism::changeDirection(Direction dir) {
    direction = dir;
    moveCount = 0;
}

bool Organism::isStraightPath(int c1, int r1, int c2, int r2, const Organism* parent) const {
    if (c1 == c2) {
        if (r1 > r2) {
            std::swap(r1, r2);
        }
        for (int row = r1; row != r2; ++row) {
            if (!isPassableCell(env->gridMap.cellAt(c1, row), parent)) {
                return false;
            }
        }
        return true;
    }
    if (c1 > c2) {
        std::swap(c1, c2);
    }
    for (int col = c1; col != c2; ++col) {
        if (!isPassableCell(env->gridMap.cellAt(col, r1), parent)) {
            return false;
        }
    }
    return true;
}

bool Organism::isPassableCell(const GridCell* cell, const Organism* parent) const {
    return cell != nullptr &&
           (cell->state == CellState::Empty || cell->owner == this || cell->owner == parent || cell->state == CellState::Food);
}

bool Organism::isClear(int col, int row, Direction candidateRotation) const {
    for (const auto& localCell : anatomy.cells()) {
        const GridCell* cell = realCell(*localCell, col, row, candidateRotation);
        if (cell == nullptr) {
            return false;
        }
        if (cell->owner == this || cell->state == CellState::Empty ||
            (!env->hyper.foodBlocksReproduction && cell->state == CellState::Food)) {
            continue;
        }
        return false;
    }
    return true;
}

void Organism::harm() {
    ++damage;
    if (damage >= maxHealth() || env->hyper.instaKill) {
        die();
    }
}

void Organism::die() {
    if (!living) {
        return;
    }
    for (const auto& cell : anatomy.cells()) {
        auto [offsetCol, offsetRow] = cell->rotatedOffset(rotation);
        env->changeCellUnchecked(c + offsetCol, r + offsetRow, CellState::Food, nullptr);
    }
    env->fossilRecord.decreasePop(species);
    living = false;
}

void Organism::updateGrid() {
    for (const auto& cell : anatomy.cells()) {
        auto [offsetCol, offsetRow] = cell->rotatedOffset(rotation);
        env->changeCellUnchecked(c + offsetCol, r + offsetRow, cell->state, cell.get());
    }
}

bool Organism::update() {
    ++lifetime;
    if (lifetime > lifespan()) {
        die();
        return living;
    }
    if (foodCollected >= foodNeeded()) {
        reproduce();
    }
    for (const auto& cell : anatomy.cells()) {
        cell->performFunction();
        if (!living) {
            return living;
        }
    }
    if (anatomy.isMover) {
        ++moveCount;
        bool changedDirection = false;
        if (ignoreBrainFor == 0) {
            changedDirection = brain.decide();
        } else {
            --ignoreBrainFor;
        }
        bool moved = attemptMove();
        if ((moveCount > moveRange && !changedDirection) || !moved) {
            bool rotated = attemptRotate();
            if (!rotated) {
                changeDirection(env->rng.direction());
                if (changedDirection) {
                    ignoreBrainFor = moveRange + 1;
                }
            }
        }
    }
    return living;
}

GridCell* Organism::realCell(const BodyCell& localCell, int col, int row, Direction candidateRotation) const {
    auto [offsetCol, offsetRow] = localCell.rotatedOffset(candidateRotation);
    return env->gridMap.cellAt(col + offsetCol, row + offsetRow);
}

bool Organism::isNatural() const {
    bool foundCenter = false;
    if (anatomy.cells().empty()) {
        return false;
    }
    for (std::size_t i = 0; i < anatomy.cells().size(); ++i) {
        const BodyCell& cell = *anatomy.cells()[i];
        for (std::size_t j = i + 1; j < anatomy.cells().size(); ++j) {
            const BodyCell& other = *anatomy.cells()[j];
            if (cell.locCol == other.locCol && cell.locRow == other.locRow) {
                return false;
            }
        }
        if (cell.locCol == 0 && cell.locRow == 0) {
            foundCenter = true;
        }
    }
    return foundCenter;
}

WorldEnvironment::WorldEnvironment(int cellSize, int cols, int rows, std::uint32_t seed)
    : gridMap(cols, rows, cellSize), rng(seed) {
    fossilRecord.setEnv(this);
}

void WorldEnvironment::update() {
    std::size_t startSize = organisms.size();
    removalScratch_.clear();
    if (removalScratch_.capacity() < startSize / 8) {
        removalScratch_.reserve(startSize / 8);
    }
    for (std::size_t i = 0; i < startSize; ++i) {
        Organism& organism = *organisms[i];
        if (!organism.living || !organism.update()) {
            removalScratch_.push_back(i);
        }
    }
    compactOrganismsFromIndexes(removalScratch_, true);
    if (hyper.foodDropProb > 0.0) {
        generateFood();
    }
    ++totalTicks;
    if (totalTicks % dataUpdateRate == 0) {
        fossilRecord.updateData();
    }
}

void WorldEnvironment::removeOrganisms(const std::vector<std::size_t>& organismIndexes) {
    if (organismIndexes.empty()) {
        return;
    }
    std::vector<std::size_t> sorted = organismIndexes;
    compactOrganismsFromIndexes(sorted);
}

void WorldEnvironment::compactOrganismsFromIndexes(std::vector<std::size_t>& organismIndexes, bool alreadySorted) {
    if (organismIndexes.empty()) {
        return;
    }
    int startPop = static_cast<int>(organisms.size());
    if (!alreadySorted) {
        std::sort(organismIndexes.begin(), organismIndexes.end());
    }
    organismIndexes.erase(std::unique(organismIndexes.begin(), organismIndexes.end()), organismIndexes.end());

    std::size_t write = 0;
    std::size_t removeCursor = 0;
    for (std::size_t read = 0; read < organisms.size(); ++read) {
        while (removeCursor < organismIndexes.size() && organismIndexes[removeCursor] < read) {
            ++removeCursor;
        }
        if (removeCursor < organismIndexes.size() && organismIndexes[removeCursor] == read) {
            totalMutability -= organisms[read]->mutability;
            ++removeCursor;
            continue;
        }
        if (write != read) {
            organisms[write] = std::move(organisms[read]);
        }
        ++write;
    }
    organisms.resize(write);

    if (organisms.empty() && startPop > 0) {
        if (config.autoReset && !config.autoPause) {
            ++resetCount;
            reset(true);
        }
    }
}

void WorldEnvironment::originOfLife() {
    auto [centerCol, centerRow] = gridMap.center();
    auto organism = std::make_unique<Organism>(centerCol, centerRow, this);
    organism->anatomy.addDefaultCell(CellState::Mouth, 0, 0);
    organism->anatomy.addDefaultCell(CellState::Producer, 1, 1);
    organism->anatomy.addDefaultCell(CellState::Producer, -1, -1);
    Organism& ref = addOrganism(std::move(organism));
    fossilRecord.addSpecies(ref, nullptr);
}

Organism& WorldEnvironment::addOrganism(std::unique_ptr<Organism> organism) {
    organism->updateGrid();
    totalMutability += organism->mutability;
    if (static_cast<int>(organism->anatomy.cells().size()) > largestCellCount) {
        largestCellCount = static_cast<int>(organism->anatomy.cells().size());
    }
    organisms.push_back(std::move(organism));
    return *organisms.back();
}

bool WorldEnvironment::canAddOrganism() const {
    return static_cast<int>(organisms.size()) < hyper.maxOrganisms || hyper.maxOrganisms < 0;
}

double WorldEnvironment::averageMutability() const {
    if (organisms.empty()) {
        return 0.0;
    }
    if (hyper.useGlobalMutability) {
        return static_cast<double>(hyper.globalMutability);
    }
    return static_cast<double>(totalMutability) / static_cast<double>(organisms.size());
}

void WorldEnvironment::changeCell(int col, int row, CellState state, BodyCell* owner) {
    GridCell* cell = gridMap.cellAt(col, row);
    if (cell == nullptr) {
        return;
    }
    changeCellUnchecked(col, row, state, owner);
}

void WorldEnvironment::changeCellUnchecked(int col, int row, CellState state, BodyCell* owner) {
    GridCell* cell = gridMap.cellAtUnchecked(col, row);
    bool addedNewWall = state == CellState::Wall && cell->state != CellState::Wall;
    cell->setType(state);
    cell->cellOwner = owner;
    cell->owner = owner == nullptr ? nullptr : owner->org;
    if (addedNewWall) {
        walls.push_back({col, row});
    }
}

void WorldEnvironment::clearWalls() {
    for (const auto& wall : walls) {
        GridCell* cell = gridMap.cellAt(wall.first, wall.second);
        if (cell != nullptr && cell->state == CellState::Wall) {
            changeCellUnchecked(wall.first, wall.second, CellState::Empty, nullptr);
        }
    }
    walls.clear();
}

void WorldEnvironment::clearOrganisms() {
    for (auto& organism : organisms) {
        organism->die();
    }
    organisms.clear();
}

void WorldEnvironment::clearDeadOrganisms() {
    removalScratch_.clear();
    if (removalScratch_.capacity() < organisms.size() / 8) {
        removalScratch_.reserve(organisms.size() / 8);
    }
    for (std::size_t i = 0; i < organisms.size(); ++i) {
        if (!organisms[i]->living) {
            removalScratch_.push_back(i);
        }
    }
    compactOrganismsFromIndexes(removalScratch_, true);
}

void WorldEnvironment::generateFood() {
    int numFood = std::max(static_cast<int>(std::floor(gridMap.cols * gridMap.rows * hyper.foodDropProb / 50000.0)), 1);
    for (int i = 0; i < numFood; ++i) {
        if (rng.chancePercent(hyper.foodDropProb)) {
            int col = rng.intExclusive(gridMap.cols);
            int row = rng.intExclusive(gridMap.rows);
            GridCell* cell = gridMap.cellAtUnchecked(col, row);
            if (cell->state == CellState::Empty) {
                changeCellUnchecked(col, row, CellState::Food, nullptr);
            }
        }
    }
}

bool WorldEnvironment::reset(bool resetLife) {
    organisms.clear();
    gridMap.fillGrid(CellState::Empty, !config.clearWallsOnReset);
    if (config.clearWallsOnReset) {
        walls.clear();
    }
    totalMutability = 0;
    totalTicks = 0;
    fossilRecord.clearRecord();
    if (resetLife) {
        originOfLife();
    }
    return true;
}

int WorldEnvironment::organismCount() const {
    return static_cast<int>(organisms.size());
}

std::unique_ptr<Organism> generateRandomOrganism(WorldEnvironment& env, int organismLayers, double cellSpawnChance) {
    auto [centerCol, centerRow] = env.gridMap.center();
    auto organism = std::make_unique<Organism>(centerCol, centerRow, &env);
    int diameter = std::max(1, (organismLayers * 2) + 1);
    organism->anatomy.reserveCells(static_cast<std::size_t>(diameter * diameter));
    organism->anatomy.addDefaultCell(CellState::Mouth, 0, 0);

    for (int layer = 1; layer <= organismLayers; ++layer) {
        bool someCellSpawned = false;
        double spawnChance = cellSpawnChance - (static_cast<double>(layer - 1) / organismLayers);

        auto trySpawn = [&](int x, int y) {
            if (organism->anatomy.hasNeighborAt(x, y) && env.rng.unit() < spawnChance) {
                CellState state = livingStates()[env.rng.intExclusive(static_cast<int>(livingStates().size()))];
                organism->anatomy.addRandomizedCell(state, x, y, env.rng);
                someCellSpawned = true;
            }
        };

        int y = -layer;
        for (int x = -layer; x <= layer; ++x) {
            trySpawn(x, y);
        }
        y = layer;
        for (int x = -layer; x <= layer; ++x) {
            trySpawn(x, y);
        }
        int x = -layer;
        for (y = -layer + 1; y <= layer - 1; ++y) {
            trySpawn(x, y);
        }
        x = layer;
        for (y = -layer + 1; y <= layer - 1; ++y) {
            trySpawn(x, y);
        }
        if (!someCellSpawned) {
            break;
        }
    }

    organism->brain.randomizeDecisions(env.rng, true);
    return organism;
}

} // namespace lifeengine
