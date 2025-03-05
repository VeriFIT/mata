// TODO: some header

#include "mata/cntnfa/annotations.hh"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/cntnfa/cntnfa.hh"

using namespace mata;
using namespace mata::cntnfa;

TEST_CASE("CntNfa: Allocate annotation collection for counter NFA") {
    Nfa cntnfa{};
    cntnfa.annotation_collection.allocate(3);
    CHECK(cntnfa.num_of_annotation_sets() == 3);
}
