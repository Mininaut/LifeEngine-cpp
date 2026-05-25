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
Invoke-Native g++ -std=c++17 -Wall -Wextra -pedantic -O2 -pipe -Iinclude src/core.cpp tests/core_tests.cpp -o lifeengine_tests.exe
Invoke-Native .\lifeengine_tests.exe
Invoke-Native g++ -std=c++17 -Wall -Wextra -pedantic -O2 -pipe -Iinclude src/core.cpp src/gui/application.cpp src/gui/renderer.cpp tests/gui_model_tests.cpp -o lifeengine_gui_model_tests.exe
Invoke-Native .\lifeengine_gui_model_tests.exe

Invoke-Native g++ -std=c++17 -Wall -Wextra -pedantic -O2 -pipe -Iinclude src/core.cpp src/main.cpp -o lifeengine.exe
Invoke-Native g++ -std=c++17 -Wall -Wextra -pedantic -O2 -pipe -Iinclude src/core.cpp src/gui/application.cpp src/gui/renderer.cpp src/gui/main.cpp src/gui/win32_app.cpp -o lifeengine_gui.exe -lcomctl32 -lgdi32 -luser32
}
finally {
    Pop-Location
}
