#include "lifeengine/core.hpp"

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    int ticks = 500;
    if (argc > 1) {
        ticks = std::atoi(argv[1]);
        if (ticks < 0) {
            ticks = 0;
        }
    }

    lifeengine::WorldEnvironment env(5, 160, 100, 1);
    env.originOfLife();

    for (int i = 0; i < ticks; ++i) {
        env.update();
    }

    std::cout << "ticks=" << env.totalTicks
              << " organisms=" << env.organismCount()
              << " species=" << env.fossilRecord.numExtantSpecies()
              << " avg_mutability=" << env.averageMutability()
              << " largest_cells=" << env.largestCellCount << '\n';
    return 0;
}
