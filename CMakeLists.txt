cmake_minimum_required(VERSION 3.20)
project(LifeEngineCpp LANGUAGES CXX)

include(CheckIPOSupported)
check_ipo_supported(RESULT LIFEENGINE_IPO_SUPPORTED OUTPUT LIFEENGINE_IPO_ERROR LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LIFEENGINE_ENABLE_LTO "Enable release link-time optimization when the compiler supports it" ON)
if(LIFEENGINE_ENABLE_LTO AND LIFEENGINE_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
elseif(LIFEENGINE_ENABLE_LTO)
    message(STATUS "LTO not enabled: compiler/linker IPO check failed")
endif()

function(lifeengine_target_options target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-pedantic;-pipe>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)
endfunction()

add_library(lifeengine_core src/core.cpp)
target_include_directories(lifeengine_core PUBLIC include)
lifeengine_target_options(lifeengine_core)

add_library(lifeengine_gui_model src/gui/application.cpp src/gui/renderer.cpp)
target_link_libraries(lifeengine_gui_model PUBLIC lifeengine_core)
target_include_directories(lifeengine_gui_model PUBLIC include)
lifeengine_target_options(lifeengine_gui_model)

add_executable(lifeengine src/main.cpp)
target_link_libraries(lifeengine PRIVATE lifeengine_core)
lifeengine_target_options(lifeengine)

set(LIFEENGINE_GUI_SUBSYSTEM)
if(WIN32)
    list(APPEND LIFEENGINE_GUI_SUBSYSTEM WIN32)
endif()
add_executable(lifeengine_gui ${LIFEENGINE_GUI_SUBSYSTEM} src/gui/main.cpp src/gui/win32_app.cpp)
target_link_libraries(lifeengine_gui PRIVATE lifeengine_gui_model)
lifeengine_target_options(lifeengine_gui)
if(WIN32)
    target_link_libraries(lifeengine_gui PRIVATE comctl32 gdi32 user32)
endif()

add_executable(lifeengine_tests tests/core_tests.cpp)
target_link_libraries(lifeengine_tests PRIVATE lifeengine_core)
lifeengine_target_options(lifeengine_tests)

add_executable(lifeengine_gui_model_tests tests/gui_model_tests.cpp)
target_link_libraries(lifeengine_gui_model_tests PRIVATE lifeengine_gui_model)
lifeengine_target_options(lifeengine_gui_model_tests)

add_executable(lifeengine_perf_smoke_tests tests/perf_smoke_tests.cpp)
target_link_libraries(lifeengine_perf_smoke_tests PRIVATE lifeengine_core)
lifeengine_target_options(lifeengine_perf_smoke_tests)

enable_testing()
add_test(NAME lifeengine_core_tests COMMAND lifeengine_tests)
add_test(NAME lifeengine_gui_model_tests COMMAND lifeengine_gui_model_tests)
add_test(NAME lifeengine_perf_smoke_tests COMMAND lifeengine_perf_smoke_tests)
