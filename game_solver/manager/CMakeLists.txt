file(GLOB_RECURSE SRCS *.cpp)

add_library(manager ${SRCS})
target_include_directories(
    manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    manager
    trainer
    worker
)