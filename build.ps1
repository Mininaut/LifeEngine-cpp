$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
function Invoke-Native {
    & $args[0] @($args | Select-Object -Skip 1)
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $($args -join ' ')"
    }
}

Push-Location $root
try {
$objDir = Join-Path $root "build\ps1-obj"
New-Item -ItemType Directory -Force $objDir | Out-Null

$commonFlags = @("-std=c++17", "-Wall", "-Wextra", "-pedantic", "-O3", "-DNDEBUG", "-pipe", "-Iinclude")
$coreObj = Join-Path $objDir "core.o"
$appObj = Join-Path $objDir "gui_application.o"
$rendererObj = Join-Path $objDir "gui_renderer.o"

Invoke-Native g++ @commonFlags -c src/core.cpp -o $coreObj
Invoke-Native g++ @commonFlags -c src/gui/application.cpp -o $appObj
Invoke-Native g++ @commonFlags -c src/gui/renderer.cpp -o $rendererObj

Invoke-Native g++ @commonFlags $coreObj tests/core_tests.cpp -o lifeengine_tests.exe
Invoke-Native .\lifeengine_tests.exe
Invoke-Native g++ @commonFlags $coreObj $appObj $rendererObj tests/gui_model_tests.cpp -o lifeengine_gui_model_tests.exe
Invoke-Native .\lifeengine_gui_model_tests.exe
Invoke-Native g++ @commonFlags $coreObj tests/perf_smoke_tests.cpp -o lifeengine_perf_smoke_tests.exe
Invoke-Native .\lifeengine_perf_smoke_tests.exe

Invoke-Native g++ @commonFlags $coreObj src/main.cpp -o lifeengine.exe
Invoke-Native g++ @commonFlags -mwindows $coreObj $appObj $rendererObj src/gui/main.cpp src/gui/win32_app.cpp -o lifeengine_gui.exe -lcomctl32 -lgdi32 -luser32
}
finally {
    Pop-Location
}
