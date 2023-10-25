file(GLOB SRCS *.cpp)

add_library(trainer ${SRCS})
target_include_directories(
    trainer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    trainer
    actor
    common
    zero
)