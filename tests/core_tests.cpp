#include "lifeengine/core.hpp"

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

lifeengine::Organism& addSingleCellOrganism(lifeengine::WorldEnvironment& env, int col, int row, lifeengine::CellState state) {
    auto organism = std::make_unique<lifeengine::Organism>(col, row, &env);
    organism->anatomy.addDefaultCell(state, 0, 0);
    lifeengine::Organism& ref = env.addOrganism(std::move(organism));
    env.fossilRecord.addSpecies(ref);
    return ref;
}

void testRotation() {
    lifeengine::WorldEnvironment env(1, 10, 10, 1);
    auto organism = std::make_unique<lifeengine::Organism>(5, 5, &env);
    lifeengine::BodyCell* cell = organism->anatomy.addDefaultCell(lifeengine::CellState::Mouth, 2, -1);

    check(cell->rotatedCol(lifeengine::Direction::Up) == 2, "up rotated col");
    check(cell->rotatedRow(lifeengine::Direction::Up) == -1, "up rotated row");
    check(cell->rotatedCol(lifeengine::Direction::Down) == -2, "down rotated col");
    check(cell->rotatedRow(lifeengine::Direction::Down) == 1, "down rotated row");
    check(cell->rotatedCol(lifeengine::Direction::Left) == -1, "left rotated col");
    check(cell->rotatedRow(lifeengine::Direction::Left) == -2, "left rotated row");
    check(cell->rotatedCol(lifeengine::Direction::Right) == 1, "right rotated col");
    check(cell->rotatedRow(lifeengine::Direction::Right) == 2, "right rotated row");
}

void testGridOwnership() {
    lifeengine::WorldEnvironment env(1, 10, 10, 2);
    lifeengine::Organism& org = addSingleCellOrganism(env, 4, 4, lifeengine::CellState::Mouth);
    lifeengine::GridCell* cell = env.gridMap.cellAt(4, 4);

    check(cell != nullptr, "grid cell exists");
    check(cell->state == lifeengine::CellState::Mouth, "grid state written");
    check(cell->owner == &org, "grid owner written");
    check(cell->cellOwner == org.anatomy.getLocalCell(0, 0), "grid cell owner written");
}

void testMouthEating() {
    lifeengine::WorldEnvironment env(1, 10, 10, 3);
    lifeengine::Organism& org = addSingleCellOrganism(env, 5, 5, lifeengine::CellState::Mouth);
    env.changeCell(5, 4, lifeengine::CellState::Food, nullptr);

    org.anatomy.getLocalCell(0, 0)->performFunction();

    check(org.foodCollected == 1, "mouth collects one food");
    check(env.gridMap.cellAt(5, 4)->state == lifeengine::CellState::Empty, "mouth clears eaten food");
}

void testProducer() {
    lifeengine::WorldEnvironment env(1, 10, 10, 4);
    env.hyper.foodProdProb = 100.0;
    env.hyper.growableNeighbors = {{1, 0}};
    addSingleCellOrganism(env, 2, 2, lifeengine::CellState::Producer);

    env.organisms[0]->anatomy.getLocalCell(0, 0)->performFunction();

    check(env.gridMap.cellAt(3, 2)->state == lifeengine::CellState::Food, "producer creates food in configured neighbor");
}

void testBrainDecision() {
    lifeengine::WorldEnvironment env(1, 20, 20, 5);
    lifeengine::Organism& org = addSingleCellOrganism(env, 10, 10, lifeengine::CellState::Mover);
    org.brain.observe({env.gridMap.cellAt(1, 1), 10, lifeengine::Direction::Left});
    env.changeCell(12, 10, lifeengine::CellState::Food, nullptr);
    org.brain.observe({env.gridMap.cellAt(12, 10), 2, lifeengine::Direction::Right});

    bool changed = org.brain.decide();

    check(changed, "brain chase changes direction");
    check(org.direction == lifeengine::Direction::Right, "brain chases closest food");
}

void testBrainTieKeepsFirstObservation() {
    lifeengine::WorldEnvironment env(1, 20, 20, 50);
    lifeengine::Organism& org = addSingleCellOrganism(env, 10, 10, lifeengine::CellState::Mover);
    env.changeCell(12, 10, lifeengine::CellState::Food, nullptr);
    env.changeCell(8, 10, lifeengine::CellState::Killer, nullptr);
    org.brain.observe({env.gridMap.cellAt(12, 10), 2, lifeengine::Direction::Right});
    org.brain.observe({env.gridMap.cellAt(8, 10), 2, lifeengine::Direction::Left});

    bool changed = org.brain.decide();

    check(changed, "brain tie changes direction");
    check(org.direction == lifeengine::Direction::Right, "brain tie keeps first closest observation");
}

void testEyeLook() {
    lifeengine::WorldEnvironment env(1, 20, 20, 6);
    auto organism = std::make_unique<lifeengine::Organism>(10, 10, &env);
    lifeengine::BodyCell* eye = organism->anatomy.addDefaultCell(lifeengine::CellState::Eye, 0, 0);
    eye->direction = lifeengine::Direction::Right;
    lifeengine::Organism& org = env.addOrganism(std::move(organism));
    env.fossilRecord.addSpecies(org);
    env.changeCell(14, 10, lifeengine::CellState::Wall, nullptr);

    lifeengine::Observation obs = eye->look();

    check(obs.cell == env.gridMap.cellAt(14, 10), "eye sees first non-empty cell");
    check(obs.distance == 4, "eye reports manhattan distance");
    check(obs.direction == lifeengine::Direction::Right, "eye reports absolute direction");
}

void testMovement() {
    lifeengine::WorldEnvironment env(1, 10, 10, 7);
    lifeengine::Organism& org = addSingleCellOrganism(env, 4, 4, lifeengine::CellState::Mover);
    org.direction = lifeengine::Direction::Right;

    bool moved = org.attemptMove();

    check(moved, "mover moves into empty cell");
    check(org.c == 5 && org.r == 4, "mover updates anchor");
    check(env.gridMap.cellAt(4, 4)->state == lifeengine::CellState::Empty, "mover clears old cell");
    check(env.gridMap.cellAt(5, 4)->owner == &org, "mover writes new owner");
}

void testBoundaryClearChecks() {
    lifeengine::WorldEnvironment env(1, 5, 5, 70);
    lifeengine::Organism& org = addSingleCellOrganism(env, 0, 0, lifeengine::CellState::Mouth);

    check(!org.isClear(-1, 0, lifeengine::Direction::Up), "isClear rejects negative col");
    check(!org.isClear(0, -1, lifeengine::Direction::Up), "isClear rejects negative row");
    check(!org.isClear(5, 0, lifeengine::Direction::Up), "isClear rejects col beyond grid");
    check(!org.isClear(0, 5, lifeengine::Direction::Up), "isClear rejects row beyond grid");
}

void testInheritedCellsRebaseOwner() {
    lifeengine::WorldEnvironment env(1, 20, 20, 80);
    auto parent = std::make_unique<lifeengine::Organism>(10, 10, &env);
    parent->anatomy.addDefaultCell(lifeengine::CellState::Mouth, 0, 0);
    parent->anatomy.addDefaultCell(lifeengine::CellState::Eye, 1, 0);
    parent->anatomy.getLocalCell(1, 0)->direction = lifeengine::Direction::Left;
    lifeengine::Organism& parentRef = env.addOrganism(std::move(parent));
    env.fossilRecord.addSpecies(parentRef);

    lifeengine::Organism child(0, 0, &env, &parentRef);

    check(child.anatomy.cells().size() == parentRef.anatomy.cells().size(), "inherit copies cell count");
    check(child.anatomy.getLocalCell(1, 0) != parentRef.anatomy.getLocalCell(1, 0), "inherit deep-copies body cells");
    check(child.anatomy.getLocalCell(1, 0)->org == &child, "inherited cell owner rebased");
    check(child.anatomy.getLocalCell(1, 0)->direction == lifeengine::Direction::Left, "inherited eye direction copied");
}

void testSeededRandomOrganismDeterminism() {
    lifeengine::WorldEnvironment first(1, 30, 30, 90);
    lifeengine::WorldEnvironment second(1, 30, 30, 90);
    auto a = lifeengine::generateRandomOrganism(first);
    auto b = lifeengine::generateRandomOrganism(second);

    check(a->anatomy.cells().size() == b->anatomy.cells().size(), "seeded random organism cell count");
    for (std::size_t i = 0; i < a->anatomy.cells().size() && i < b->anatomy.cells().size(); ++i) {
        const auto& ac = *a->anatomy.cells()[i];
        const auto& bc = *b->anatomy.cells()[i];
        check(ac.state == bc.state, "seeded random organism cell state");
        check(ac.locCol == bc.locCol && ac.locRow == bc.locRow, "seeded random organism cell location");
        check(ac.direction == bc.direction, "seeded random organism eye direction");
    }
}

void testFossilRecordCap() {
    lifeengine::WorldEnvironment env(1, 10, 10, 8);
    env.fossilRecord.recordSizeLimit = 5;
    for (int i = 0; i < 12; ++i) {
        env.totalTicks = i;
        env.fossilRecord.updateData();
    }

    check(env.fossilRecord.tickRecord.size() == 5, "fossil record cap");
    check(env.fossilRecord.tickRecord.front() == 7, "fossil record drops oldest");
}

void testOriginOfLife() {
    lifeengine::WorldEnvironment env(1, 11, 11, 9);
    env.originOfLife();

    check(env.organismCount() == 1, "origin creates one organism");
    check(env.organisms[0]->anatomy.cells().size() == 3, "origin anatomy size");
    check(env.fossilRecord.numExtantSpecies() == 1, "origin creates species");
}

void testAutoResetReseedsOrigin() {
    lifeengine::WorldEnvironment env(1, 11, 11, 10);
    env.originOfLife();
    env.organisms[0]->die();
    env.clearDeadOrganisms();

    check(env.resetCount == 1, "auto reset count increments");
    check(env.organismCount() == 1, "auto reset reseeds origin organism");
    check(env.fossilRecord.numExtantSpecies() == 1, "auto reset recreates species");
}

void testRemoveOrganismsCompactsInOnePass() {
    lifeengine::WorldEnvironment env(1, 20, 20, 11);
    env.config.autoReset = false;
    lifeengine::Organism& first = addSingleCellOrganism(env, 1, 1, lifeengine::CellState::Mouth);
    addSingleCellOrganism(env, 2, 2, lifeengine::CellState::Mouth);
    lifeengine::Organism& third = addSingleCellOrganism(env, 3, 3, lifeengine::CellState::Mouth);
    addSingleCellOrganism(env, 4, 4, lifeengine::CellState::Mouth);

    env.removeOrganisms({3, 1, 1, 99});

    check(env.organismCount() == 2, "remove organisms compacts survivors");
    check(env.organisms[0].get() == &first, "remove organisms keeps first survivor");
    check(env.organisms[1].get() == &third, "remove organisms preserves survivor order");
    check(env.totalMutability == first.mutability + third.mutability, "remove organisms updates total mutability");
}

void testRepeatedWallWritesDoNotDuplicateWallIndex() {
    lifeengine::WorldEnvironment env(1, 10, 10, 12);

    env.changeCell(3, 3, lifeengine::CellState::Wall, nullptr);
    env.changeCell(3, 3, lifeengine::CellState::Wall, nullptr);
    env.changeCell(3, 3, lifeengine::CellState::Wall, nullptr);

    check(env.walls.size() == 1, "repeated wall writes keep one wall index");
    env.clearWalls();
    check(env.gridMap.cellAt(3, 3)->state == lifeengine::CellState::Empty, "clear walls clears deduplicated wall");
    check(env.walls.empty(), "clear walls drops wall index cache");
}

} // namespace

int main() {
    testRotation();
    testGridOwnership();
    testMouthEating();
    testProducer();
    testBrainDecision();
    testBrainTieKeepsFirstObservation();
    testEyeLook();
    testMovement();
    testBoundaryClearChecks();
    testInheritedCellsRebaseOwner();
    testSeededRandomOrganismDeterminism();
    testFossilRecordCap();
    testOriginOfLife();
    testAutoResetReseedsOrigin();
    testRemoveOrganismsCompactsInOnePass();
    testRepeatedWallWritesDoNotDuplicateWallIndex();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "all core tests passed\n";
    return EXIT_SUCCESS;
}
