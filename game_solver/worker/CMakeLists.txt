file(GLOB_RECURSE SRCS *.cpp)

add_library(worker ${SRCS})
target_include_directories(
    worker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    base
    hex
    killallgo
)
target_link_libraries(
    worker
    common
    trainer
)