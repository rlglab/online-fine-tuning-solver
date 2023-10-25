file(GLOB SRCS *.cpp)
add_executable(game_solver ${SRCS})
target_link_libraries(
    game_solver
    actor
    config
    console
    environment
    network
    utils
    common
    manager
    trainer
    worker
    ${Boost_LIBRARIES}
    ${TORCH_LIBRARIES}
)