add_library(simlib OBJECT src/explicit_lts_sim.cc)

# simlib needs at least c++11
target_compile_features(simlib PUBLIC cxx_std_20)

target_include_directories(simlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")
