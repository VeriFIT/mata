add_library(re2
        re2/bitmap256.h
        re2/compile.cc
        re2/parse.cc
        re2/perl_groups.cc
        re2/pod_array.h
        re2/prog.cc
        re2/prog.h
        re2/re2.cc
        re2/re2.h
        re2/regexp.cc
        re2/regexp.h
        re2/simplify.cc
        re2/sparse_array.h
        re2/sparse_set.h
        re2/stringpiece.cc
        re2/stringpiece.h
        re2/tostring.cc
        re2/unicode_casefold.cc
        re2/unicode_casefold.h
        re2/unicode_groups.cc
        re2/unicode_groups.h
        re2/walker-inl.h
        util/logging.h
        util/mutex.h
        util/rune.cc
        util/strutil.cc
        util/strutil.h
        util/utf.h
        util/util.h)

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

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_link_libraries(re2 pthread)
endif()

target_include_directories(re2 PUBLIC ./)
