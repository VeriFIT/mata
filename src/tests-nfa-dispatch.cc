// TODO: some header

#include "../3rdparty/catch.hpp"

#include <vata2/nfa.hh>
#include <vata2/vm-dispatch.hh>

using namespace Vata2::VM;
using namespace Vata2::Nfa;

TEST_CASE("Vata2::VM::find_dispatcher(\"NFA\")")
{
	SECTION("construct")
	{
		Vata2::Parser::ParsedSection parsec;
		parsec.type = "NFA";

		VMValue res = find_dispatcher("NFA")("construct", {{"Parsec", &parsec}});
		REQUIRE("NFA" == res.first);
		const Nfa* aut = static_cast<const Nfa*>(res.second);
		REQUIRE(aut->trans_empty());
		REQUIRE(aut->initialstates.empty());
		REQUIRE(aut->finalstates.empty());
		delete aut;
	}

	SECTION("invalid function")
	{
		CHECK_THROWS_WITH(find_dispatcher("NFA")("barrel-roll", { }),
			Catch::Contains("invalid function name"));
	}

	SECTION("invalid arguments 1")
	{
		CHECK_THROWS_WITH(find_dispatcher("NFA")("construct", { }),
			Catch::Contains("does not match arity"));
	}

	SECTION("invalid arguments 2")
	{
		CHECK_THROWS_WITH(find_dispatcher("NFA")("construct", {{"Foo", nullptr}}),
			Catch::Contains("invalid type"));
	}

	SECTION("aux")
	{
		DEBUG_PRINT("Insufficient testing of Vata2::VM::find_dispatcher(\"NFA\")");
	}
}
