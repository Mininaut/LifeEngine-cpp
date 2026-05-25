# LifeEngine C++ Core

This directory contains a C++17/MinGW64 port of the LifeEngine simulation core.
It intentionally excludes the browser DOM, jQuery controls, webpack bundle, and
canvas renderer. The port focuses on the deterministic model: grid ownership,
organism anatomy, cell behavior, movement, perception, reproduction, species,
and fossil record stats.

Build and test with MinGW64 GCC directly:

```powershell
cd cpp
.\build.ps1
.\lifeengine.exe 1000
```

Or build through CMake + Ninja while still using MinGW64 `g++`:

```powershell
cd cpp
cmake --preset mingw-ninja
cmake --build --preset mingw-ninja
ctest --preset mingw-ninja
.\build\lifeengine.exe 1000
```

The C++ API lives in `include/lifeengine/core.hpp`, with implementation in
`src/core.cpp`. The CLI in `src/main.cpp` runs a headless simulation and prints
basic stats.
