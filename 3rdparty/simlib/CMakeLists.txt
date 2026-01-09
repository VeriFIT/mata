add_library(simlib OBJECT src/explicit_lts_sim.cc)

target_compile_features(simlib PUBLIC cxx_std_20)

add_library(simlib_headers INTERFACE)
target_include_directories(simlib_headers
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(simlib PUBLIC simlib_headers)
