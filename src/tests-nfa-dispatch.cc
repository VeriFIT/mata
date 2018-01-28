/* tests-nfa-dispatch.cc -- tests of NFA dispatch functions
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libvata2.
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
		REQUIRE("NFA" == res.type);
		const Nfa* aut = static_cast<const Nfa*>(res.get_ptr());
		REQUIRE(aut->trans_empty());
		REQUIRE(aut->initialstates.empty());
		REQUIRE(aut->finalstates.empty());
		delete aut;
	}

	SECTION("invalid function")
	{
		VMValue res = find_dispatcher("NFA")("barrel-roll", { });
		REQUIRE("NaV" == res.type);
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
		WARN_PRINT("Insufficient testing of Vata2::VM::find_dispatcher(\"NFA\")");
	}
}
