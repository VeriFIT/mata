/* tests-parser.cc -- tests of Parser
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

#include "catch.hpp"
#include "mata/inter-aut.hh"

#include <mata/parser.hh>
#include <mata/util.hh>

using namespace Mata::Parser;
using namespace Mata::Util;


TEST_CASE("correct use of Mata::Parser::parse_mf_section()")
{ // {{{
	ParsedSection parsec;

	SECTION("empty file")
	{
		std::string file =
			"";

		parsec = parse_mf_section(file);
		REQUIRE(parsec.empty());
	}

	SECTION("empty section")
	{
		std::string file =
			"@Type\n";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		REQUIRE(parsec.dict.empty());
		REQUIRE(parsec.body.empty());
	}

	SECTION("file with some keys")
	{
		std::string file =
			"@Type\n"
			"%key1\n"
			"%key2\n";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		REQUIRE(haskey(parsec.dict, "key1"));
		REQUIRE(parsec.dict.at("key1").empty());
		REQUIRE(haskey(parsec.dict, "key2"));
		REQUIRE(parsec.dict.at("key2").empty());
		REQUIRE(parsec.body.empty());
	}

	SECTION("file with some keys and values")
	{
		std::string file =
			"@Type\n"
			"%key1 value1\n"
			"%key2\n"
			"%key3 value3\n";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "value1");
		ref = &parsec.dict.at("key2");
		REQUIRE(ref->empty());
		ref = &parsec.dict.at("key3");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "value3");
		REQUIRE(parsec.body.empty());
	}

	SECTION("file with multiple values for some keys")
	{
		std::string file =
			"@Type\n"
			"%key1     value1.1  value1.2 value1.3			value1.4\n"
			"%key2\n";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 4);
		REQUIRE((*ref)[0] == "value1.1");
		REQUIRE((*ref)[1] == "value1.2");
		REQUIRE((*ref)[2] == "value1.3");
		REQUIRE((*ref)[3] == "value1.4");
		ref = &parsec.dict.at("key2");
		REQUIRE(ref->empty());
		REQUIRE(parsec.body.empty());
	}

	SECTION("file with some transitions")
	{
		std::string file =
			"@Type\n"
			"%key1 value1\n"
			"%key2 value2.1 value2.2     \n"
			"a\n"
			"b0 b1 b2 b3		b4    b5";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "value1");
		ref = &parsec.dict.at("key2");
		REQUIRE(ref->size() == 2);
		REQUIRE((*ref)[0] == "value2.1");
		REQUIRE((*ref)[1] == "value2.2");
		REQUIRE(parsec.body.size() == 2);
		std::vector<BodyLine> body(parsec.body.begin(), parsec.body.end());
		REQUIRE(body[0].size() == 1);
		REQUIRE(body[0][0] == "a");
		REQUIRE(body[1].size() == 6);
		REQUIRE(body[1][0] == "b0");
		REQUIRE(body[1][1] == "b1");
		REQUIRE(body[1][2] == "b2");
		REQUIRE(body[1][3] == "b3");
		REQUIRE(body[1][4] == "b4");
		REQUIRE(body[1][5] == "b5");
	}

	SECTION("file with transitions and line break")
	{
		std::string file =
		        "@Type\n"
		        "%key1 value1\n"
		        "%key2 value2.1 value2.2     \n"
		        "a\\\n"
		        "b";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "value1");
		ref = &parsec.dict.at("key2");
		REQUIRE(ref->size() == 2);
		REQUIRE((*ref)[0] == "value2.1");
		REQUIRE((*ref)[1] == "value2.2");
		REQUIRE(parsec.body.size() == 1);
		std::vector<BodyLine> body(parsec.body.begin(), parsec.body.end());
		REQUIRE(body[0].size() == 2);
		REQUIRE(body[0][0] == "a");
		REQUIRE(body[0][1] == "b");
	}

	SECTION("file with transitions and line break")
	{
		std::string file =
		        "@Type\n"
		        "%key1 value1\n"
		        "a x & !b&c|(a& !b)";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "value1");
		REQUIRE(parsec.body.size() == 1);
		std::vector<BodyLine> body(parsec.body.begin(), parsec.body.end());
		REQUIRE(body[0].size() == 14);
		REQUIRE(body[0][0] == "a");
		REQUIRE(body[0][1] == "x");
		REQUIRE(body[0][2] == "&");
		REQUIRE(body[0][3] == "!");
		REQUIRE(body[0][4] == "b");
		REQUIRE(body[0][5] == "&");
		REQUIRE(body[0][6] == "c");
		REQUIRE(body[0][7] == "|");
		REQUIRE(body[0][8] == "(");
		REQUIRE(body[0][9] == "a");
		REQUIRE(body[0][10] == "&");
		REQUIRE(body[0][11] == "!");
		REQUIRE(body[0][12] == "b");
		REQUIRE(body[0][13] == ")");
	}

	SECTION("file with comments and whitespaces")
	{
		std::string file =
			"     \n"
			"\n"
			"	\n"
			"# a comment\n"
			"    #another comment\n"
			"#\n"
			"     @Ty#pe      \n"
			"# some commment\n"
			"%key1 value1#comment#comment2\n"
			"   %key2 value2.1 # value2.2     \n"
			"\t\n"
			"a\n"
			"   b0 b1 #b2";

		parsec = parse_mf_section(file);

		REQUIRE("Ty" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "value1");
		ref = &parsec.dict.at("key2");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "value2.1");
		REQUIRE(parsec.body.size() == 2);
		std::vector<BodyLine> body(parsec.body.begin(), parsec.body.end());
		REQUIRE(body[0].size() == 1);
		REQUIRE(body[0][0] == "a");
		REQUIRE(body[1].size() == 2);
		REQUIRE(body[1][0] == "b0");
		REQUIRE(body[1][1] == "b1");
	}

	SECTION("using double quotes and escaping for names")
	{
		std::string file =
			"@Type\n"
			"%key1 \"value 1\"\n"
			"%key2 \"value2.1\" value2 2 \"value 2 3\"\n"
			"%key3 \"val#1\"    # test\n"
			"a \"\"\n"
			"%key4 \"val 1   \" \n"
			"%key5\n"
			"b0 \"b 1\" c d\n"
			"\"%key6\"\n"
			"%key7\n"
			"c 0 \"\\\"he's so cool,\\\" he said \\/\" c d\n"
			"\"a\"\n"
			"\"\"\n"
			"'\n"
			"q a q'";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "value 1");
		ref = &parsec.dict.at("key2");
		REQUIRE(ref->size() == 4);
		REQUIRE((*ref)[0] == "value2.1");
		REQUIRE((*ref)[1] == "value2");
		REQUIRE((*ref)[2] == "2");
		REQUIRE((*ref)[3] == "value 2 3");
		ref = &parsec.dict.at("key3");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "val#1");
		ref = &parsec.dict.at("key4");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "val 1   ");
		ref = &parsec.dict.at("key5");
		REQUIRE(ref->size() == 0);
		ref = &parsec.dict.at("key7");
		REQUIRE(ref->size() == 0);
		REQUIRE(parsec.body.size() == 8);
		std::vector<BodyLine> body(parsec.body.begin(), parsec.body.end());
		REQUIRE(body[0].size() == 2);
		REQUIRE(body[0][0] == "a");
		REQUIRE(body[0][1] == "");
		REQUIRE(body[1].size() == 4);
		REQUIRE(body[1][0] == "b0");
		REQUIRE(body[1][1] == "b 1");
		REQUIRE(body[1][2] == "c");
		REQUIRE(body[1][3] == "d");
		REQUIRE(body[2].size() == 1);
		REQUIRE(body[2][0] == "%key6");
		REQUIRE(body[3].size() == 5);
		REQUIRE(body[3][0] == "c");
		REQUIRE(body[3][1] == "0");
		REQUIRE(body[3][2] == "\"he's so cool,\" he said \\/");
		REQUIRE(body[3][3] == "c");
		REQUIRE(body[3][4] == "d");
		REQUIRE(body[4].size() == 1);
		REQUIRE(body[4][0] == "a");
		REQUIRE(body[5].size() == 1);
		REQUIRE(body[5][0] == "");
		REQUIRE(body[6].size() == 1);
		REQUIRE(body[6][0] == "'");
		REQUIRE(body[7].size() == 3);
		REQUIRE(body[7][0] == "q");
		REQUIRE(body[7][1] == "a");
		REQUIRE(body[7][2] == "q'");
	}

	SECTION("file with newlines among keys")
	{
		std::string file =
			"@Type\n"
			"%key1 value1.1 value1.2   # comment\n"
			"%key1    value1.3\n"
			"%key2\n"
			"%key3 \"value3\"";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 3);
		REQUIRE((*ref)[0] == "value1.1");
		REQUIRE((*ref)[1] == "value1.2");
		REQUIRE((*ref)[2] == "value1.3");
		ref = &parsec.dict.at("key2");
		REQUIRE(ref->empty());
		ref = &parsec.dict.at("key3");
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "value3");
		REQUIRE(parsec.body.empty());
	}

	SECTION("special characters inside quoted strings")
	{
		std::string file =
			"@Type\n"
			"%key1     \"value@1\"  \"value@2\"#new\n"
			"%key2     \"value%1\"  (\"value%2\")\n";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 2);
		REQUIRE((*ref)[0] == "value@1");
		REQUIRE((*ref)[1] == "value@2");
		ref = &parsec.dict.at("key2");
		REQUIRE(ref->size() == 4);
		REQUIRE((*ref)[0] == "value%1");
		REQUIRE((*ref)[1] == "(");
		REQUIRE((*ref)[2] == "value%2");
		REQUIRE((*ref)[3] == ")");
		REQUIRE(parsec.body.empty());
	}

	SECTION("file with no keys")
	{
		std::string file =
			"@Type\n"
			"a b c\n";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		REQUIRE(parsec.body.size() == 1);
		std::vector<BodyLine> body(parsec.body.begin(), parsec.body.end());
		REQUIRE(body[0].size() == 3);
		REQUIRE(body[0][0] == "a");
		REQUIRE(body[0][1] == "b");
		REQUIRE(body[0][2] == "c");
	}

	SECTION("correct handling of parentheses")
	{
		std::string file =
			"@Type\n"
			"%key1 (a b)\n"
			"a (b   c  d)(e\n";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 4);
		REQUIRE((*ref)[0] == "(");
		REQUIRE((*ref)[1] == "a");
		REQUIRE((*ref)[2] == "b");
		REQUIRE((*ref)[3] == ")");
		REQUIRE(parsec.body.size() == 1);
		std::vector<BodyLine> body(parsec.body.begin(), parsec.body.end());
		REQUIRE(body[0].size() == 8);
		REQUIRE(body[0][0] == "a");
		REQUIRE(body[0][1] == "(");
		REQUIRE(body[0][2] == "b");
		REQUIRE(body[0][3] == "c");
		REQUIRE(body[0][4] == "d");
		REQUIRE(body[0][5] == ")");
		REQUIRE(body[0][6] == "(");
		REQUIRE(body[0][7] == "e");
	}

	SECTION("correct handling of start of another section")
	{
		std::string file =
			"@Type1\n"
			"%key1\n"
			"@Type2\n"
			"%key2\n";

		std::istringstream stream(file);

		parsec = parse_mf_section(stream);

		REQUIRE("Type1" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 0);

		// check what remains
		std::string remains = stream.str().substr(stream.tellg());
		REQUIRE("@Type2\n%key2\n" == remains);
	}
} // parse_mf_section correct }}}


TEST_CASE("incorrect use of Mata::Parser::parse_mf_section()")
{ // {{{
	ParsedSection parsec;

	SECTION("no type")
	{
		std::string file =
			"@\n"
			"Type"
			"%key1\n"
			"%key2\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("expecting automaton type"));
	}

	SECTION("trailing characters behind @TYPE")
	{
		std::string file =
			"@Type another\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("invalid trailing characters"));
	}

	SECTION("missing type")
	{
		std::string file =
			"%key1\n"
			"%key2\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("expecting automaton type"));
	}

	SECTION("unterminated quote")
	{
		std::string file =
			"@Type\n"
			"%key1 \"value\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("missing ending quotes"));
	}

	SECTION("unterminated quote 2")
	{
		std::string file =
			"@Type\n"
			"%key1 \"\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("missing ending quotes"));
	}

	SECTION("newlines within names")
	{
		std::string file =
			"@Type\n"
			"%key1 \" value  \n"
			"   1\"\n"
			"\"value\n"
			"\n"
			"\"\n"
			"\n"
			"\"value    # comment\n"
			"3\"";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("missing ending quotes"));
	}

	SECTION("quoted strings starting in the middle of strings")
	{
		std::string file =
			"@Type\n"
			"%key1 val\"ue\"\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("misplaced quotes"));
	}

	SECTION("quoted strings ending in the middle of strings")
	{
		std::string file =
			"@Type\n"
			"%key1 \"val\"ue\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("misplaced quotes"));
	}

	SECTION("incorrect position of special characters")
	{
		std::string file =
			"@Type\n"
			"%key1 @here";

		CHECK_THROWS_WITH(parse_mf_section(file),
			Catch::Contains("invalid position of @TYPE") &&
			Catch::Contains("@here"));
	}

	SECTION("incorrect position of special characters 2")
	{
		std::string file =
			"@Type\n"
			"q1 @here q2";

		CHECK_THROWS_WITH(parse_mf_section(file),
			Catch::Contains("invalid position of @TYPE") &&
			Catch::Contains("@here"));
	}

	SECTION("incorrect position of special characters 3")
	{
		std::string file =
			"@Type\n"
			"q1 %here q2";

		CHECK_THROWS_WITH(parse_mf_section(file),
			Catch::Contains("invalid position of %KEY") &&
			Catch::Contains("%here"));
	}

	SECTION("incorrect position of special characters 4")
	{
		std::string file =
			"@Type\n"
			"%key1 %here";

		CHECK_THROWS_WITH(parse_mf_section(file),
			Catch::Contains("invalid position of %KEY") &&
			Catch::Contains("%here"));
	}

	SECTION("no key name")
	{
		std::string file =
			"@Type\n"
			"%\n"
			"%key2\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("%KEY name missing"));
	}

	SECTION("special characters inside strings 1")
	{
		std::string file =
			"@Type\n"
			"%key1     value@1\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("misplaced character \'@\'"));
	}

	SECTION("special characters inside strings 2")
	{
		std::string file =
			"@Type\n"
			"%key2     value%1\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("misplaced character \'%\'"));
	}

	SECTION("special characters inside strings 3")
	{
		std::string file =
			"@Type\n"
			"%key1     @value\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("invalid position of @TYPE"));
	}

	SECTION("special characters inside strings 4")
	{
		std::string file =
			"@Type\n"
			"%key2     %value\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("invalid position of %KEY"));
	}

	SECTION("invalid use of quotes")
	{
		std::string file =
			"\"@Type\"\n";

		CHECK_THROWS_WITH(parse_mf_section(file),
                          Catch::Contains("expecting automaton type"));
	}
} // parse_mf_section incorrect }}}


TEST_CASE("correct use of Mata::Parser::parse_mf()")
{ // {{{
	Parsed parsed;

	SECTION("empty file")
	{
		std::string file =
			"";

		parsed = parse_mf(file);

		REQUIRE(parsed.empty());
	}

	SECTION("one section")
	{
		std::string file =
			"@Type1\n"
			"%key1\n";

		parsed = parse_mf(file);
		REQUIRE(parsed.size() == 1);
		REQUIRE(parsed[0].type == "Type1");
		REQUIRE(haskey(parsed[0].dict, "key1"));
	}

	SECTION("two sections")
	{
		std::string file =
			"@Type1\n"
			"%key1\n"
			"@Type2\n"
			"%key2\n";

		parsed = parse_mf(file);
		REQUIRE(parsed.size() == 2);
		REQUIRE(parsed[0].type == "Type1");
		REQUIRE(haskey(parsed[0].dict, "key1"));
		REQUIRE(parsed[1].type == "Type2");
		REQUIRE(haskey(parsed[1].dict, "key2"));
	}
} // parse_mf }}}


TEST_CASE("parsing automata to intermediate representation")
{ // {{{
    Parsed parsed;

    SECTION("NFA")
    {
        std::string file =
           "@NFA-explicit\n"
           "%States-enum q r s t \"(r,s)\"\n"
           "%Alphabet-auto\n"
           "%Initial q & r\n"
           "%Final q | r\n"
           "q symbol & r\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        REQUIRE(auts.size() == 1);
        const Mata::IntermediateAut& aut = auts.back();
        REQUIRE(aut.transitions.size() == 1);
        REQUIRE(aut.transitions.front().first.name == "q");
        REQUIRE(aut.transitions.front().first.is_operand());
        REQUIRE(aut.transitions.front().second.node.is_operator());
        REQUIRE(aut.transitions.front().second.node.name == "&");
        REQUIRE(aut.transitions.front().second.children.size() == 2);
        REQUIRE(aut.transitions.front().second.children.front().node.is_operand());
        REQUIRE(aut.transitions.front().second.children.front().node.name == "symbol");
        REQUIRE(aut.transitions.front().second.children.front().children.empty());
        REQUIRE(aut.transitions.front().second.children[1].node.is_operand());
        REQUIRE(aut.transitions.front().second.children[1].node.name == "r");
        REQUIRE(aut.transitions.front().second.children[1].children.empty());
        REQUIRE(aut.initial_formula.node.name == "&");
        REQUIRE(aut.initial_formula.children.size() == 2);
        REQUIRE(aut.initial_formula.children[0].node.name == "q");
        REQUIRE(aut.initial_formula.children[1].node.name == "r");
        REQUIRE(aut.final_formula.node.name == "|");
        REQUIRE(aut.final_formula.children.size() == 2);
        REQUIRE(aut.final_formula.children[0].node.name == "q");
        REQUIRE(aut.final_formula.children[1].node.name == "r");
    }

    SECTION("NFA without &")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum q r s t \"(r,s)\"\n"
                "%Alphabet-auto\n"
                "q symbol r\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        REQUIRE(auts.size() == 1);
        const Mata::IntermediateAut& aut = auts.back();
        REQUIRE(aut.transitions.size() == 1);
        REQUIRE(aut.transitions.front().first.name == "q");
        REQUIRE(aut.transitions.front().first.is_operand());
        REQUIRE(aut.transitions.front().second.node.is_operator());
        REQUIRE(aut.transitions.front().second.node.name == "&");
        REQUIRE(aut.transitions.front().second.children.size() == 2);
        REQUIRE(aut.transitions.front().second.children.front().node.is_operand());
        REQUIRE(aut.transitions.front().second.children.front().node.name == "symbol");
        REQUIRE(aut.transitions.front().second.children.front().children.empty());
        REQUIRE(aut.transitions.front().second.children[1].node.is_operand());
        REQUIRE(aut.transitions.front().second.children[1].node.name == "r");
        REQUIRE(aut.transitions.front().second.children[1].children.empty());
    }

    SECTION("NFA explicit enumeration of initials and finals")
    {
        std::string file =
                "@NFA-explicit\n"
                "%States-enum q r s t \"(r,s)\"\n"
                "%Alphabet-auto\n"
                "%Initial r s\n"
                "%Final q t\n"
                "q symbol r\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        REQUIRE(auts.size() == 1);
        const Mata::IntermediateAut& aut = auts.back();
        REQUIRE(aut.transitions.size() == 1);
        REQUIRE(aut.initial_enumerated);
        REQUIRE(aut.final_enumerated);
        const auto initials = aut.get_enumerated_initials();
        REQUIRE(initials.count("r"));
        REQUIRE(initials.count("s"));
        REQUIRE(!initials.count("q"));
        const auto finals = aut.get_enumerated_finals();
        REQUIRE(finals.count("t"));
        REQUIRE(finals.count("q"));
        REQUIRE(!finals.count("r"));
    }

    SECTION("NFA - final states from multiple negations")
    {
        std::string file =
                "@NFA-bits\n"
                "%Alphabet-auto\n"
                "%Initial q1 q8\n"
                "%Final !q0 & !q1 & !q4 & !q5\n"
                "q0 (!a1 & !a2 & !a3 & (!a0 | a0)) q1\n"
                "q1 (!a2 & !a3 & !a4 & (!a0 | a0)) q2\n"
                "q2 (!a3 & !a4 & !a5 & (!a0 | a0)) q3\n"
                "q2 (!a3 & !a4 & !a5 & (!a0 | a0)) q4\n"
                "q3 (!a2 & !a3 & !a4 & (!a0 | a0)) q5\n"
                "q3 (!a2 & !a3 & !a4 & (!a0 | a0)) q6\n"
                "q5 (!a1 & !a2 & !a3 & (!a0 | a0)) q7\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        const Mata::IntermediateAut inter_aut = auts[0];

        auto final_states = inter_aut.get_positive_finals();
        REQUIRE(final_states.size() == 5);
        REQUIRE(final_states.count("2"));
        REQUIRE(final_states.count("3"));
        REQUIRE(final_states.count("6"));
        REQUIRE(final_states.count("7"));
        REQUIRE(final_states.count("8"));
    }
	
    SECTION("NFA - final states from one negation")
    {
        std::string file =
                "@NFA-bits\n"
                "%Alphabet-auto\n"
                "%Initial q1 q8\n"
                "%Final !q0\n"
                "q0 (!a1 & !a2 & !a3 & (!a0 | a0)) q1\n"
                "q1 (!a2 & !a3 & !a4 & (!a0 | a0)) q2\n"
                "q2 (!a3 & !a4 & !a5 & (!a0 | a0)) q3\n"
                "q2 (!a3 & !a4 & !a5 & (!a0 | a0)) q4\n"
                "q3 (!a2 & !a3 & !a4 & (!a0 | a0)) q5\n"
                "q3 (!a2 & !a3 & !a4 & (!a0 | a0)) q6\n"
                "q5 (!a1 & !a2 & !a3 & (!a0 | a0)) q7\n";

        const auto auts = Mata::IntermediateAut::parse_from_mf(parse_mf(file));
        const Mata::IntermediateAut inter_aut = auts[0];

        auto final_states = inter_aut.get_positive_finals();
        REQUIRE(final_states.size() == 8);
        REQUIRE(final_states.count("1"));
        REQUIRE(final_states.count("2"));
        REQUIRE(final_states.count("3"));
        REQUIRE(final_states.count("4"));
        REQUIRE(final_states.count("5"));
        REQUIRE(final_states.count("6"));
        REQUIRE(final_states.count("7"));
        REQUIRE(final_states.count("8"));
    }

    SECTION("AFA explicit")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-enum q r s t \"(r,s)\"\n"
                "%Alphabet-auto\n"
                "q symbol | other_symbol & (\"(r,s)\" | r | s)\n"
                "r !b & ! c & (\"(r,s)\")\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        REQUIRE(auts.size() == 1);
        const Mata::IntermediateAut& aut = auts.back();
        REQUIRE(aut.transitions.size() == 2);
        REQUIRE(aut.transitions.front().first.name == "q");
        REQUIRE(aut.transitions.front().first.is_operand());
        REQUIRE(aut.transitions.front().second.node.is_operator());
        REQUIRE(aut.transitions.front().second.node.name == "|");
        REQUIRE(aut.transitions.front().second.children.size() == 2);
        REQUIRE(aut.transitions.front().second.children.front().node.is_operand());
        REQUIRE(aut.transitions.front().second.children.front().node.name == "symbol");
        REQUIRE(aut.transitions.front().second.children.front().children.empty());
        REQUIRE(aut.transitions.front().second.children[1].node.is_operator());
        REQUIRE(aut.transitions.front().second.children[1].node.name == "&");
        REQUIRE(aut.transitions.front().second.children[1].children.size() == 2);
        REQUIRE(aut.transitions.front().second.children[1].children.front().node.is_operand());
        REQUIRE(aut.transitions.front().second.children[1].children.front().node.name == "other_symbol");
        REQUIRE(aut.transitions.front().second.children[1].children[1].node.is_operator());
        REQUIRE(aut.transitions.front().second.children[1].children[1].node.name == "|");
        REQUIRE(aut.transitions.front().second.children[1].children[1].children.front().node.name == "|");
        REQUIRE(aut.transitions.front().second.children[1].children[1].children[1].node.name == "s");
        REQUIRE(aut.transitions.front().second.children[1].children[1].children.front().children.front().node.name == "(r,s)");
        REQUIRE(aut.transitions.front().second.children[1].children[1].children.front().children[1].node.name == "r");

        REQUIRE(aut.transitions[1].first.name == "r");
        REQUIRE(aut.transitions[1].first.is_operand());
        REQUIRE(aut.transitions[1].second.node.is_operator());
        REQUIRE(aut.transitions[1].second.node.name == "&");
        REQUIRE(aut.transitions[1].second.children.size() == 2);
        REQUIRE(aut.transitions[1].second.children.front().node.is_operator());
        REQUIRE(aut.transitions[1].second.children.front().node.name == "!");
        REQUIRE(aut.transitions[1].second.children.front().children.front().node.name == "b");
        REQUIRE(aut.transitions[1].second.children[1].node.is_operator());
        REQUIRE(aut.transitions[1].second.children[1].node.name == "&");
        REQUIRE(aut.transitions[1].second.children[1].children.size() == 2);
    }

    SECTION("AFA explicit two automatic naming")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-auto\n"
                "%Alphabet-auto\n"
                "r !b & ! c & d\n";

        bool exception = false;
        try {
            parsed = parse_mf(file);
            std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        } catch (std::exception& e) {
            exception = true;

        }

        REQUIRE(exception);
    }

    SECTION("AFA explicit correct automatic naming")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-marked\n"
                "%Alphabet-enum a b\n"
                "q1 a & !q2 & b\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const Mata::IntermediateAut aut = auts[0];
        REQUIRE(aut.transitions.front().first.name == "1");
        REQUIRE(aut.transitions.front().first.raw == "q1");

    }

    SECTION("AFA explicit correct automatic naming - parentheses")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-marked\n"
                "%Alphabet-enum a b c\n"
                "q1 ((a & !q2) & b) | c\n";

        parsed = parse_mf(file);
        std::vector<Mata::IntermediateAut> auts = Mata::IntermediateAut::parse_from_mf(parsed);
        const Mata::IntermediateAut aut = auts[0];
        REQUIRE(aut.transitions.front().first.name == "1");
        REQUIRE(aut.transitions.front().first.raw == "q1");
    }

    SECTION("AFA explicit non existing symbol error")
    {
        std::string file =
                "@AFA-explicit\n"
                "%States-marked\n"
                "%Alphabet-enum a b\n"
                "q1 a & !q2 & c\n";

        bool exception = false;
        parsed = parse_mf(file);
        try {
            Mata::IntermediateAut::parse_from_mf(parsed);
        } catch (std::runtime_error e) {
            exception = true;
        }

        REQUIRE(exception);
    }
    } // parse_mf }}}


TEST_CASE("Mata::Parser::ParsedSection::operator<<(ostream&)")
{ // {{{
	SECTION("aux")
	{
		WARN_PRINT("Insufficient testing of Mata::Parser::ParsedSection::operator<<(ostream&)");
	}

} // }}}
