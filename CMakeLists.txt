cmake_minimum_required(VERSION 3.25)
project(MagicDust VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE STRING "Minimum macOS version")

option(MD_COPY_PLUGIN "Copy built plugins to the user plug-in folders" OFF)
option(MD_CLANG_TIDY "Run clang-tidy on project targets" OFF)
option(MD_PERF_ASSERT "Enable performance assertions" OFF)

enable_testing()

include(cmake/Dependencies.cmake)
include(cmake/Warnings.cmake)

add_test(NAME presets_copy_guard
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-presets.sh ${CMAKE_SOURCE_DIR}/CMakePresets.json)

add_subdirectory(engine)
add_subdirectory(bank)
add_subdirectory(midiexport)
add_subdirectory(tools/md-cli)
add_subdirectory(plugin)
add_subdirectory(tests)
