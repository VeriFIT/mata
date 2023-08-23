add_library(simlib src/explicit_lts_sim.cc)

target_include_directories(simlib PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include/")
