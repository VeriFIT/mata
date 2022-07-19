/* tests-parser-dispatch.cc -- tests of Parser dispatch functions
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

#include <mata/parser.hh>
#include <mata/vm-dispatch.hh>

using namespace Mata::Parser;
using namespace Mata::VM;

TEST_CASE("Mata::VM::find_dispatcher(\"Parsec\")")
{
	SECTION("copy 1")
	{
		ParsedSection parsec;
		parsec.type = "NFA";
		parsec.dict = {
				{"States", {"1", "2", "8" }},
				{"Alphabet", {"a", "b", "c"}},
			};

		parsec.body = {
				{"1", "a", "2"},
				{"3", "c", "b", "4"},
				{ },
			};

		VMValue res = find_dispatcher(Mata::TYPE_PARSEC)("copy", {{Mata::TYPE_PARSEC, &parsec}});
		REQUIRE(Mata::TYPE_PARSEC == res.type);
		const ParsedSection* parsec_copy =
			static_cast<const ParsedSection*>(res.get_ptr());
		REQUIRE((*parsec_copy == parsec));
		delete parsec_copy;
	}

	SECTION("copy 2")
	{
		ParsedSection parsec;
		parsec.type = "NFA";
		parsec.dict = {
				{"States", {"1", "2", "8" }},
				{"Alphabet", {"a", "b", "c"}},
			};

		parsec.body = {
				{"1", "a", "2"},
				{"3", "c", "b", "4"},
				{ },
			};

		VMValue res = find_dispatcher(Mata::TYPE_PARSEC)("copy", {{Mata::TYPE_PARSEC, &parsec}});
		parsec.body.pop_back();   // remove an element from body
		REQUIRE(Mata::TYPE_PARSEC == res.type);
		const ParsedSection* parsec_copy =
			static_cast<const ParsedSection*>(res.get_ptr());
		REQUIRE((*parsec_copy != parsec));
		delete parsec_copy;
	}

	SECTION("invalid function")
	{
		VMValue res = find_dispatcher(Mata::TYPE_PARSEC)("barrel-roll", { });
		REQUIRE(Mata::TYPE_NOT_A_VALUE == res.type);
	}
}
