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

TEST_CASE("Vata2::Afa::construct() correct calls")
{ // {{{
	Afa aut;
	Vata2::Parser::ParsedSection parsec;

	SECTION("construct an empty automaton")
	{
		parsec.type = Vata2::Afa::TYPE_AFA;

		construct(&aut, parsec);

		// REQUIRE(is_lang_empty(aut));
	}

	SECTION("construct a simple non-empty automaton accepting the empty word")
	{
		parsec.type = Vata2::Afa::TYPE_AFA;
		parsec.dict.insert({"Initial", {"q1"}});
		parsec.dict.insert({"Final", {"q1"}});

		construct(&aut, parsec);

		// REQUIRE(!is_lang_empty(aut));
	}

	SECTION("construct an automaton with more than one initial/final states")
	{
		parsec.type = Vata2::Afa::TYPE_AFA;
		parsec.dict.insert({"Initial", {"q1", "q2"}});
		parsec.dict.insert({"Final", {"q1", "q2", "q3"}});

		construct(&aut, parsec);

		// REQUIRE(aut.initialstates.size() == 2);
		// REQUIRE(aut.finalstates.size() == 3);
	}

	SECTION("construct a simple non-empty automaton accepting only the word 'a'")
	{
		parsec.type = Vata2::Afa::TYPE_AFA;
		parsec.dict.insert({"Initial", {"q1"}});
		parsec.dict.insert({"Final", {"q2"}});
		parsec.body = { {"q1", "a AND q2"} };

		construct(&aut, parsec);
	}
} // }}}
