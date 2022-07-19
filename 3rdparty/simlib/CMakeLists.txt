add_library(simlib src/explicit_lts_sim.cc)

set(cxx_compiler_flags
        -Wall
        -Wfloat-equal
        -fms-extensions
        -fdiagnostics-show-option
        -std=c++14
        -Wctor-dtor-privacy
        -Weffc++
        -fPIC
        -fno-strict-aliasing
        )

foreach(flag ${cxx_compiler_flags})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endforeach()

target_include_directories(simlib PUBLIC ./include/)
