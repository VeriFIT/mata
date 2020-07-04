// TODO: some header

#include "../3rdparty/catch.hpp"

#include <unordered_set>

#include <vata2/afa.hh>
using namespace Vata2::Afa;
using namespace Vata2::util;
using namespace Vata2::Parser;

TEST_CASE("Vata2::Afa::Trans::operator<<")
{ // {{{
	Trans trans(1, "(A and 2) or (B and 3)");

	REQUIRE(std::to_string(trans) == "(1, (A and 2) or (B and 3))");
} // }}}
