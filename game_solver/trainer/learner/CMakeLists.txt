file(GLOB SRCS *.cpp)

find_package(pybind11 REQUIRED)
pybind11_add_module(pybind_pcn_learner ${SRCS})
target_link_libraries(
    pybind_pcn_learner
    PUBLIC
    config
    common
    environment
    utils
)