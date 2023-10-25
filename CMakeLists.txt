cmake_minimum_required(VERSION 3.16)

project(game_solver)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Torch REQUIRED)
find_package(Boost COMPONENTS system thread regex)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -g -Wall -mpopcnt -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wall -Wno-unused-function -O0")

add_subdirectory(game_solver)
add_subdirectory(game_solver/common)
add_subdirectory(game_solver/manager)
add_subdirectory(game_solver/trainer)
add_subdirectory(game_solver/trainer/learner)
add_subdirectory(game_solver/worker)
add_subdirectory(minizero/minizero/actor)
add_subdirectory(minizero/minizero/config)
add_subdirectory(minizero/minizero/console)
add_subdirectory(minizero/minizero/environment)
add_subdirectory(minizero/minizero/learner)
add_subdirectory(minizero/minizero/network)
add_subdirectory(minizero/minizero/utils)
add_subdirectory(minizero/minizero/zero)

string(TOLOWER "${GAME_TYPE}_solver" EXE_FILE_NAME)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${EXE_FILE_NAME})