$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
    g++ -std=c++17 -Wall -Wextra -pedantic -O2 -pipe -Iinclude src/core.cpp tests/core_tests.cpp -o lifeengine_tests.exe
    .\lifeengine_tests.exe

    g++ -std=c++17 -Wall -Wextra -pedantic -O2 -pipe -Iinclude src/core.cpp src/main.cpp -o lifeengine.exe
}
finally {
    Pop-Location
}
