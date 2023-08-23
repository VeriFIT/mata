add_library(simlib src/explicit_lts_sim.cc)

# simlib needs at least c++11
target_compile_features(simlib PUBLIC cxx_std_11)
# we turn off compiler extensions
set_target_properties(simlib PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(simlib PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include/")
