/* tests-nfa-dispatch.cc -- tests of NFA dispatch functions
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "../3rdparty/catch.hpp"

#include <mata/nfa.hh>
#include <mata/vm-dispatch.hh>

using namespace Mata::VM;
using namespace Mata::Nfa;

/*
TEST_CASE("Mata::VM::find_dispatcher(\"NFA\")")
{
	SECTION("construct")
	{
		Mata::Parser::ParsedSection parsec;
		parsec.type = Mata::Nfa::TYPE_NFA;

		VMValue res = find_dispatcher(Mata::Nfa::TYPE_NFA)("construct",
			{{Mata::TYPE_PARSEC, &parsec}});
		REQUIRE(Mata::Nfa::TYPE_NFA == res.type);
		const Nfa* aut = static_cast<const Nfa*>(res.get_ptr());
		REQUIRE(aut->trans_empty());
		REQUIRE(aut->initialstates.empty());
		REQUIRE(aut->finalstates.empty());
		delete aut;
	}

	SECTION("no parameters")
	{
		CHECK_THROWS_WITH(find_dispatcher(Mata::Nfa::TYPE_NFA)("barrel-roll", { }),
			Catch::Contains("with no arguments"));
	}

	SECTION("invalid function")
	{
		std::string str = "arg1";
		VMValue res = find_dispatcher(Mata::Nfa::TYPE_NFA)("barrel-roll",
			{{Mata::TYPE_STR, &str}});
		REQUIRE(Mata::TYPE_NOT_A_VALUE == res.type);
	}

	SECTION("invalid arguments 1")
	{
		CHECK_THROWS_WITH(find_dispatcher(Mata::Nfa::TYPE_NFA)("construct", {{"Foo", nullptr}}),
			Catch::Contains("invalid type"));
	}

	SECTION("aux")
	{
		WARN_PRINT("Insufficient testing of Mata::VM::find_dispatcher(\"NFA\")");
	}
}
*/
