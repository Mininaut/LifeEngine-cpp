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

void testUncheckedGridScanWorkload() {
    lifeengine::WorldEnvironment env(1, 100, 80, 101);

    int emptyCells = 0;
    int coordinateSum = 0;
    for (int pass = 0; pass < 25; ++pass) {
        for (int col = 0; col < env.gridMap.cols; ++col) {
            for (int row = 0; row < env.gridMap.rows; ++row) {
                const lifeengine::GridCell* cell = env.gridMap.cellAtUnchecked(col, row);
                if (cell->state == lifeengine::CellState::Empty) {
                    ++emptyCells;
                }
                coordinateSum += cell->col + cell->row;
            }
        }
    }

    check(emptyCells == 100 * 80 * 25, "unchecked grid scan visits every empty cell");
    check(coordinateSum == 25 * ((80 * 99 * 100 / 2) + (100 * 79 * 80 / 2)), "unchecked grid scan reads stable coordinates");
}

void testRandomOrganismGenerationWorkloadDeterminism() {
    lifeengine::WorldEnvironment first(1, 60, 60, 202);
    lifeengine::WorldEnvironment second(1, 60, 60, 202);

    std::size_t firstCells = 0;
    std::size_t secondCells = 0;
    for (int i = 0; i < 200; ++i) {
        auto firstOrganism = lifeengine::generateRandomOrganism(first, 5, 0.80);
        auto secondOrganism = lifeengine::generateRandomOrganism(second, 5, 0.80);
        firstCells += firstOrganism->anatomy.cells().size();
        secondCells += secondOrganism->anatomy.cells().size();
        check(firstOrganism->isNatural(), "first generated organism remains natural");
        check(secondOrganism->isNatural(), "second generated organism remains natural");
    }

    check(firstCells == secondCells, "random organism workload is deterministic by seed");
    check(firstCells > 200, "random organism workload creates multi-cell organisms");
}

void testSimulationUpdateWorkload() {
    lifeengine::WorldEnvironment env(5, 120, 80, 303);
    env.config.autoReset = false;
    env.originOfLife();

    for (int i = 0; i < 5000; ++i) {
        env.update();
    }

    check(env.totalTicks == 5000, "simulation workload advances every requested tick");
    check(env.largestCellCount >= 3, "simulation workload preserves largest cell accounting");
    check(env.fossilRecord.tickRecord.size() <= env.fossilRecord.recordSizeLimit, "simulation workload respects fossil cap");
}

void testFossilRecordCapWorkload() {
    lifeengine::WorldEnvironment env(1, 10, 10, 404);
    env.fossilRecord.recordSizeLimit = 128;

    for (int i = 0; i < 1000; ++i) {
        env.totalTicks = i;
        env.fossilRecord.updateData();
    }

    check(env.fossilRecord.tickRecord.size() == 128, "fossil workload caps tick record");
    check(env.fossilRecord.popCounts.size() == 128, "fossil workload caps pop record");
    check(env.fossilRecord.averageCellCounts.size() == 128, "fossil workload caps cell count record");
    check(env.fossilRecord.tickRecord.front() == 872, "fossil workload drops oldest records");
}

} // namespace

int main() {
    testUncheckedGridScanWorkload();
    testRandomOrganismGenerationWorkloadDeterminism();
    testSimulationUpdateWorkload();
    testFossilRecordCapWorkload();

    if (failures != 0) {
        std::cerr << failures << " perf smoke test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "all perf smoke tests passed\n";
    return EXIT_SUCCESS;
}
