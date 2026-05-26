CXX := g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -O3 -DNDEBUG -pipe -Iinclude
GUI_LDFLAGS ?= -mwindows
CORE := src/core.cpp

.PHONY: all test run gui

all: test run gui

test:
	$(CXX) $(CXXFLAGS) $(CORE) tests/core_tests.cpp -o lifeengine_tests.exe
	./lifeengine_tests.exe
	$(CXX) $(CXXFLAGS) $(CORE) src/gui/application.cpp src/gui/renderer.cpp tests/gui_model_tests.cpp -o lifeengine_gui_model_tests.exe
	./lifeengine_gui_model_tests.exe
	$(CXX) $(CXXFLAGS) $(CORE) tests/perf_smoke_tests.cpp -o lifeengine_perf_smoke_tests.exe
	./lifeengine_perf_smoke_tests.exe

run:
	$(CXX) $(CXXFLAGS) $(CORE) src/main.cpp -o lifeengine.exe

gui:
	$(CXX) $(CXXFLAGS) $(GUI_LDFLAGS) $(CORE) src/gui/application.cpp src/gui/renderer.cpp src/gui/main.cpp src/gui/win32_app.cpp -o lifeengine_gui.exe -lcomctl32 -lgdi32 -luser32
