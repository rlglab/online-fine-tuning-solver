file(GLOB SRCS *.cpp)

add_library(common ${SRCS})
target_include_directories(
    common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    common
    config
    environment
)