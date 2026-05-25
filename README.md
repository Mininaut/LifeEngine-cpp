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
.\lifeengine_gui.exe
```

Or build through CMake + Ninja while still using MinGW64 `g++`:

```powershell
cd cpp
cmake --preset mingw-ninja
cmake --build --preset mingw-ninja
ctest --preset mingw-ninja
.\build\lifeengine.exe 1000
.\build\lifeengine_gui.exe
```

The C++ API lives in `include/lifeengine/core.hpp`, with implementation in
`src/core.cpp`. The CLI in `src/main.cpp` runs a headless simulation and prints
basic stats. The native GUI is split into a backend-neutral model in
`include/lifeengine/gui/application.hpp` / `src/gui/application.cpp`, a shared
grid renderer in `include/lifeengine/gui/renderer.hpp` / `src/gui/renderer.cpp`,
and a replaceable Win32/GDI backend in `src/gui/win32_app.cpp`.

The Win32 GUI includes a separate 15x15 organism editor: choose a cell type,
left-click the editor grid to add or replace cells, left-click an eye to rotate
it, right-click to remove cells, and use `Drop` to place the edited organism in
the simulation.
