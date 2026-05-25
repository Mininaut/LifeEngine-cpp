CXX := g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -O2 -pipe -Iinclude
CORE := src/core.cpp

.PHONY: all test run

all: test run

test:
	$(CXX) $(CXXFLAGS) $(CORE) tests/core_tests.cpp -o lifeengine_tests.exe
	./lifeengine_tests.exe

run:
	$(CXX) $(CXXFLAGS) $(CORE) src/main.cpp -o lifeengine.exe
