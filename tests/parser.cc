/* tests-parser.cc -- tests of Parser
 */

#include "mata/parser/parser.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"
#include "mata/parser/inter-aut.hh"
#include "mata/utils/utils.hh"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <optional>
#include <sstream>

using namespace mata::parser;
using namespace mata::utils;

TEST_CASE("correct use of mata::Parser::parse_mf_section()") { // {{{
	ParsedSection parsec;

	SECTION("empty file") {
		std::string file = "";

		parsec = parse_mf_section(file);
		REQUIRE(parsec.empty());
	}

	SECTION("empty section") {
		std::string file = "@Type\n";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		REQUIRE(parsec.dict.empty());
		REQUIRE(parsec.body.empty());
	}

	SECTION("type line ending in CRLF") {
		parsec = parse_mf_section("@Type\r\n%key1 value1\r\n");

		REQUIRE("Type" == parsec.type);
		REQUIRE(haskey(parsec.dict, "key1"));
	}

	SECTION("file with some keys") {
		std::string file = "@Type\n"
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

	SECTION("file with some keys and values") {
		std::string file = "@Type\n"
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

	SECTION("file with multiple values for some keys") {
		std::string file = "@Type\n"
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

	SECTION("file with some transitions") {
		std::string file = "@Type\n"
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
		REQUIRE(body[0][0].token == "a");
		REQUIRE(body[1].size() == 6);
		REQUIRE(body[1][0].token == "b0");
		REQUIRE(body[1][1].token == "b1");
		REQUIRE(body[1][2].token == "b2");
		REQUIRE(body[1][3].token == "b3");
		REQUIRE(body[1][4].token == "b4");
		REQUIRE(body[1][5].token == "b5");
	}

	SECTION("file with transitions and line break") {
		std::string file = "@Type\n"
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
		REQUIRE(body[0][0].token == "a");
		REQUIRE(body[0][1].token == "b");
	}

	SECTION("lone backslash as the type line (#761)") {
		// Used to read line.back() out of bounds and abort on a Debug assert.
		CHECK_THROWS_WITH(parse_mf_section("\\"), Catch::Matchers::ContainsSubstring("expecting automaton type"));
		CHECK_THROWS_WITH(parse_mf_section("\\\n"), Catch::Matchers::ContainsSubstring("expecting automaton type"));
	}

	SECTION("type line ending with a backslash is not a continuation line") {
		CHECK_THROWS_WITH(
			parse_mf_section("@NFA-explicit\\\n%Initial q0\n"),
			Catch::Matchers::ContainsSubstring("invalid trailing characters")
		);
	}

	SECTION("file with transitions and line break") {
		std::string file = "@Type\n"
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
		REQUIRE(body[0][0].token == "a");
		REQUIRE(body[0][1].token == "x");
		REQUIRE(body[0][2].token == "&");
		REQUIRE(body[0][3].token == "!");
		REQUIRE(body[0][4].token == "b");
		REQUIRE(body[0][5].token == "&");
		REQUIRE(body[0][6].token == "c");
		REQUIRE(body[0][7].token == "|");
		REQUIRE(body[0][8].token == "(");
		REQUIRE(body[0][9].token == "a");
		REQUIRE(body[0][10].token == "&");
		REQUIRE(body[0][11].token == "!");
		REQUIRE(body[0][12].token == "b");
		REQUIRE(body[0][13].token == ")");
	}

	SECTION("file with comments and whitespaces") {
		std::string file = "     \n"
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
		REQUIRE(body[0][0].token == "a");
		REQUIRE(body[1].size() == 2);
		REQUIRE(body[1][0].token == "b0");
		REQUIRE(body[1][1].token == "b1");
	}

	SECTION("using double quotes and escaping for names") {
		std::string file = "@Type\n"
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
		REQUIRE(body[0][0].token == "a");
		REQUIRE(body[0][1].token == "");
		REQUIRE(body[1].size() == 4);
		REQUIRE(body[1][0].token == "b0");
		REQUIRE(body[1][1].token == "b 1");
		REQUIRE(body[1][2].token == "c");
		REQUIRE(body[1][3].token == "d");
		REQUIRE(body[2].size() == 1);
		REQUIRE(body[2][0].token == "%key6");
		REQUIRE(body[3].size() == 5);
		REQUIRE(body[3][0].token == "c");
		REQUIRE(body[3][1].token == "0");
		REQUIRE(body[3][2].token == "\"he's so cool,\" he said \\/");
		REQUIRE(body[3][3].token == "c");
		REQUIRE(body[3][4].token == "d");
		REQUIRE(body[4].size() == 1);
		REQUIRE(body[4][0].token == "a");
		REQUIRE(body[5].size() == 1);
		REQUIRE(body[5][0].token == "");
		REQUIRE(body[6].size() == 1);
		REQUIRE(body[6][0].token == "'");
		REQUIRE(body[7].size() == 3);
		REQUIRE(body[7][0].token == "q");
		REQUIRE(body[7][1].token == "a");
		REQUIRE(body[7][2].token == "q'");
	}

	SECTION("file with newlines among keys") {
		std::string file = "@Type\n"
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

	SECTION("special characters inside quoted strings") {
		std::string file = "@Type\n"
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

	SECTION("quoted operators in the body") {
		parsec = parse_mf_section("@Type\nq0 \"&\" & q1\n");
		CHECK(parsec.body == std::vector<BodyLine>{{"q0", BodyToken{"&", true}, "&", "q1"}});
	}

	SECTION("file with no keys") {
		std::string file = "@Type\n"
						   "a b c\n";

		parsec = parse_mf_section(file);

		REQUIRE("Type" == parsec.type);
		REQUIRE(parsec.body.size() == 1);
		std::vector<BodyLine> body(parsec.body.begin(), parsec.body.end());
		REQUIRE(body[0].size() == 3);
		REQUIRE(body[0][0].token == "a");
		REQUIRE(body[0][1].token == "b");
		REQUIRE(body[0][2].token == "c");
	}

	SECTION("correct handling of parentheses") {
		std::string file = "@Type\n"
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
		REQUIRE(body[0][0].token == "a");
		REQUIRE(body[0][1].token == "(");
		REQUIRE(body[0][2].token == "b");
		REQUIRE(body[0][3].token == "c");
		REQUIRE(body[0][4].token == "d");
		REQUIRE(body[0][5].token == ")");
		REQUIRE(body[0][6].token == "(");
		REQUIRE(body[0][7].token == "e");
	}

	SECTION("correct handling of start of another section") {
		std::string file = "@Type1\n"
						   "%key1\n"
						   "@Type2\n"
						   "%key2\n";

		std::istringstream stream(file);

		parsec = parse_mf_section(stream);

		REQUIRE("Type1" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 0);

		// check what remains
		std::string remains = stream.str().substr(static_cast<size_t>(stream.tellg()));
		REQUIRE("@Type2\n%key2\n" == remains);
	}

	SECTION("Correctly parse with missing newline at the end of the file") {
		std::string file = "@NFA-explicit\n"
						   "%Alphabet-auto\n"
						   "%Initial q0\n"
						   "%Final q0\n"
						   "q0 0 q0";
		std::istringstream stream(file);
		mata::nfa::Nfa aut = mata::nfa::builder::construct(mata::IntermediateAut::parse_from_mf(parse_mf(file))[0]);
		CHECK(aut.initial.size() == 1);
		CHECK(aut.initial.contains(0));
		CHECK(aut.final.size() == 1);
		CHECK(aut.initial.contains(0));
		CHECK(aut.delta.contains(0, 0, 0) == 1);
		CHECK(aut.delta.num_of_transitions() == 1);
	}
} // parse_mf_section correct }}}

TEST_CASE("incorrect use of mata::Parser::parse_mf_section()") { // {{{
	ParsedSection parsec;

	SECTION("no type") {
		std::string file = "@\n"
						   "Type"
						   "%key1\n"
						   "%key2\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("expecting automaton type"));
	}

	SECTION("trailing characters behind @TYPE") {
		std::string file = "@Type another\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("invalid trailing characters"));
	}

	SECTION("missing type") {
		std::string file = "%key1\n"
						   "%key2\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("expecting automaton type"));
	}

	SECTION("unterminated quote") {
		std::string file = "@Type\n"
						   "%key1 \"value\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("missing ending quotes"));
	}

	SECTION("unterminated quote 2") {
		std::string file = "@Type\n"
						   "%key1 \"\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("missing ending quotes"));
	}

	SECTION("newlines within names") {
		std::string file = "@Type\n"
						   "%key1 \" value  \n"
						   "   1\"\n"
						   "\"value\n"
						   "\n"
						   "\"\n"
						   "\n"
						   "\"value    # comment\n"
						   "3\"";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("missing ending quotes"));
	}

	SECTION("quoted strings starting in the middle of strings") {
		std::string file = "@Type\n"
						   "%key1 val\"ue\"\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("misplaced quotes"));
	}

	SECTION("quoted strings ending in the middle of strings") {
		std::string file = "@Type\n"
						   "%key1 \"val\"ue\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("misplaced quotes"));
	}

	SECTION("incorrect position of special characters") {
		std::string file = "@Type\n"
						   "%key1 @here";

		CHECK_THROWS_WITH(
			parse_mf_section(file), Catch::Matchers::ContainsSubstring("invalid position of @TYPE") &&
										Catch::Matchers::ContainsSubstring("@here")
		);
	}

	SECTION("incorrect position of special characters 2") {
		std::string file = "@Type\n"
						   "q1 @here q2";

		CHECK_THROWS_WITH(
			parse_mf_section(file), Catch::Matchers::ContainsSubstring("invalid position of @TYPE") &&
										Catch::Matchers::ContainsSubstring("@here")
		);
	}

	SECTION("incorrect position of special characters 3") {
		std::string file = "@Type\n"
						   "q1 %here q2";

		CHECK_THROWS_WITH(
			parse_mf_section(file), Catch::Matchers::ContainsSubstring("invalid position of %KEY") &&
										Catch::Matchers::ContainsSubstring("%here")
		);
	}

	SECTION("incorrect position of special characters 4") {
		std::string file = "@Type\n"
						   "%key1 %here";

		CHECK_THROWS_WITH(
			parse_mf_section(file), Catch::Matchers::ContainsSubstring("invalid position of %KEY") &&
										Catch::Matchers::ContainsSubstring("%here")
		);
	}

	SECTION("no key name") {
		std::string file = "@Type\n"
						   "%\n"
						   "%key2\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("%KEY name missing"));
	}

	SECTION("special characters inside strings 1") {
		std::string file = "@Type\n"
						   "%key1     value@1\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("misplaced character \'@\'"));
	}

	SECTION("special characters inside strings 2") {
		std::string file = "@Type\n"
						   "%key2     value%1\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("misplaced character \'%\'"));
	}

	SECTION("special characters inside strings 3") {
		std::string file = "@Type\n"
						   "%key1     @value\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("invalid position of @TYPE"));
	}

	SECTION("special characters inside strings 4") {
		std::string file = "@Type\n"
						   "%key2     %value\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("invalid position of %KEY"));
	}

	SECTION("invalid use of quotes") {
		std::string file = "\"@Type\"\n";

		CHECK_THROWS_WITH(parse_mf_section(file), Catch::Matchers::ContainsSubstring("expecting automaton type"));
	}
} // parse_mf_section incorrect }}}

TEST_CASE("correct use of mata::Parser::parse_mf()") { // {{{
	Parsed parsed;

	SECTION("empty file") {
		std::string file = "";

		parsed = parse_mf(file);

		REQUIRE(parsed.empty());
	}

	SECTION("one section") {
		std::string file = "@Type1\n"
						   "%key1\n";

		parsed = parse_mf(file);
		REQUIRE(parsed.size() == 1);
		REQUIRE(parsed[0].type == "Type1");
		REQUIRE(haskey(parsed[0].dict, "key1"));
	}

	SECTION("two sections") {
		std::string file = "@Type1\n"
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

TEST_CASE("parsing automata to intermediate representation") { // {{{
	Parsed parsed;

	SECTION("NFA") {
		std::string file = "@NFA-explicit\n"
						   "%States-enum q r s t \"(r,s)\"\n"
						   "%Alphabet-auto\n"
						   "%Initial q & r\n"
						   "%Final q | r\n"
						   "q symbol & r\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		REQUIRE(auts.size() == 1);
		const mata::IntermediateAut& aut = auts.back();
		REQUIRE(aut.transitions.size() == 1);
		REQUIRE(aut.transitions.front().first.name() == "q");
		REQUIRE(aut.transitions.front().first.is_operand());
		REQUIRE(aut.transitions.front().second.node().is_operator());
		REQUIRE(aut.transitions.front().second.node().name() == "&");
		REQUIRE(aut.transitions.front().second.children().size() == 2);
		REQUIRE(aut.transitions.front().second.children().front().node().is_operand());
		REQUIRE(aut.transitions.front().second.children().front().node().name() == "symbol");
		REQUIRE(aut.transitions.front().second.children().front().children().empty());
		REQUIRE(aut.transitions.front().second.children()[1].node().is_operand());
		REQUIRE(aut.transitions.front().second.children()[1].node().name() == "r");
		REQUIRE(aut.transitions.front().second.children()[1].children().empty());
		REQUIRE(aut.initial_formula.node().name() == "&");
		REQUIRE(aut.initial_formula.children().size() == 2);
		REQUIRE(aut.initial_formula.children()[0].node().name() == "q");
		REQUIRE(aut.initial_formula.children()[1].node().name() == "r");
		REQUIRE(aut.final_formula.node().name() == "|");
		REQUIRE(aut.final_formula.children().size() == 2);
		REQUIRE(aut.final_formula.children()[0].node().name() == "q");
		REQUIRE(aut.final_formula.children()[1].node().name() == "r");
	}

	SECTION("NFA without &") {
		std::string file = "@NFA-explicit\n"
						   "%States-enum q r s t \"(r,s)\"\n"
						   "%Alphabet-auto\n"
						   "q symbol r\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		REQUIRE(auts.size() == 1);
		const mata::IntermediateAut& aut = auts.back();
		REQUIRE(aut.transitions.size() == 1);
		REQUIRE(aut.transitions.front().first.name() == "q");
		REQUIRE(aut.transitions.front().first.is_operand());
		REQUIRE(aut.transitions.front().second.node().is_operator());
		REQUIRE(aut.transitions.front().second.node().name() == "&");
		REQUIRE(aut.transitions.front().second.children().size() == 2);
		REQUIRE(aut.transitions.front().second.children().front().node().is_operand());
		REQUIRE(aut.transitions.front().second.children().front().node().name() == "symbol");
		REQUIRE(aut.transitions.front().second.children().front().children().empty());
		REQUIRE(aut.transitions.front().second.children()[1].node().is_operand());
		REQUIRE(aut.transitions.front().second.children()[1].node().name() == "r");
		REQUIRE(aut.transitions.front().second.children()[1].children().empty());
	}

	SECTION("NFA explicit enumeration of initials and finals") {
		std::string file = "@NFA-explicit\n"
						   "%States-enum q r s t \"(r,s)\"\n"
						   "%Alphabet-auto\n"
						   "%Initial r s\n"
						   "%Final q t\n"
						   "q symbol r\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		REQUIRE(auts.size() == 1);
		const mata::IntermediateAut& aut = auts.back();
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

	SECTION("NFA - final states from multiple negations") {
		std::string file = "@NFA-bits\n"
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

		const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
		const mata::IntermediateAut inter_aut = auts[0];

		auto final_states = inter_aut.get_positive_finals();
		REQUIRE(final_states.size() == 5);
		REQUIRE(final_states.count("2"));
		REQUIRE(final_states.count("3"));
		REQUIRE(final_states.count("6"));
		REQUIRE(final_states.count("7"));
		REQUIRE(final_states.count("8"));
	}

	SECTION("NFA - final states from one negation") {
		std::string file = "@NFA-bits\n"
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

		const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
		const mata::IntermediateAut inter_aut = auts[0];

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

	SECTION("NFA - true/false constants") {
		std::string file =
			R"(@NFA-bits
%Alphabet-auto
%Initial q1
%Final \true
q0 \true q1
q1 \false q2
)";

		const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
		const mata::IntermediateAut inter_aut = auts[0];

		CHECK(inter_aut.final_formula.node().is_true());
		CHECK(inter_aut.transitions.at(0).second.children().at(0).node().is_true());
		CHECK(inter_aut.transitions.at(1).second.children().at(0).node().is_false());
	}

	SECTION("alternating automata are not supported") {
		const std::string file{"@AFA-explicit\n%States-enum q r\n%Alphabet-auto\n%Initial q\n%Final r\nq a & r\n"};
		parsed = parse_mf(file);
		CHECK_THROWS_WITH(
			mata::IntermediateAut::parse_from_mf(parsed),
			Catch::Matchers::ContainsSubstring("Unknown automaton type 'AFA-explicit'")
		);
		CHECK_THROWS_WITH(
			mata::nfa::builder::parse_from_mata(file),
			Catch::Matchers::ContainsSubstring("The type of input automaton is 'AFA-explicit'")
		);
	}

	// TODO(afa): Re-add the alternating automata support. The AFA code is kept, but not compiled: `MATA_AFA_SUPPORT`
	//  is never defined.
#ifdef MATA_AFA_SUPPORT
	SECTION("AFA explicit") {
		std::string file = "@AFA-explicit\n"
						   "%States-enum q r s t \"(r,s)\"\n"
						   "%Alphabet-auto\n"
						   "q symbol | other_symbol & (\"(r,s)\" | r | s)\n"
						   "r !b & ! c & (\"(r,s)\")\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		REQUIRE(auts.size() == 1);
		const mata::IntermediateAut& aut = auts.back();
		REQUIRE(aut.transitions.size() == 2);
		REQUIRE(aut.transitions.front().first.name() == "q");
		REQUIRE(aut.transitions.front().first.is_operand());
		REQUIRE(aut.transitions.front().second.node().is_operator());
		REQUIRE(aut.transitions.front().second.node().name() == "|");
		REQUIRE(aut.transitions.front().second.children().size() == 2);
		REQUIRE(aut.transitions.front().second.children().front().node().is_operand());
		REQUIRE(aut.transitions.front().second.children().front().node().name() == "symbol");
		REQUIRE(aut.transitions.front().second.children().front().children().empty());
		REQUIRE(aut.transitions.front().second.children()[1].node().is_operator());
		REQUIRE(aut.transitions.front().second.children()[1].node().name() == "&");
		REQUIRE(aut.transitions.front().second.children()[1].children().size() == 2);
		REQUIRE(aut.transitions.front().second.children()[1].children().front().node().is_operand());
		REQUIRE(aut.transitions.front().second.children()[1].children().front().node().name() == "other_symbol");
		REQUIRE(aut.transitions.front().second.children()[1].children()[1].node().is_operator());
		REQUIRE(aut.transitions.front().second.children()[1].children()[1].node().name() == "|");
		REQUIRE(aut.transitions.front().second.children()[1].children()[1].children().front().node().name() == "|");
		REQUIRE(aut.transitions.front().second.children()[1].children()[1].children()[1].node().name() == "s");
		REQUIRE(
			aut.transitions.front()
				.second.children()[1]
				.children()[1]
				.children()
				.front()
				.children()
				.front()
				.node()
				.name() == "(r,s)"
		);
		REQUIRE(
			aut.transitions.front().second.children()[1].children()[1].children().front().children()[1].node().name() ==
			"r"
		);

		REQUIRE(aut.transitions[1].first.name() == "r");
		REQUIRE(aut.transitions[1].first.is_operand());
		REQUIRE(aut.transitions[1].second.node().is_operator());
		REQUIRE(aut.transitions[1].second.node().name() == "&");
		REQUIRE(aut.transitions[1].second.children().size() == 2);
		REQUIRE(aut.transitions[1].second.children().front().node().is_operator());
		REQUIRE(aut.transitions[1].second.children().front().node().name() == "!");
		REQUIRE(aut.transitions[1].second.children().front().children().front().node().name() == "b");
		REQUIRE(aut.transitions[1].second.children()[1].node().is_operator());
		REQUIRE(aut.transitions[1].second.children()[1].node().name() == "&");
		REQUIRE(aut.transitions[1].second.children()[1].children().size() == 2);
	}

	SECTION("AFA explicit two automatic naming") {
		std::string file = "@AFA-explicit\n"
						   "%States-auto\n"
						   "%Alphabet-auto\n"
						   "r !b & ! c & d\n";

		bool exception = false;
		try {
			parsed = parse_mf(file);
			std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		} catch (std::exception& e) { exception = true; }

		REQUIRE(exception);
	}

	SECTION("AFA explicit correct automatic naming") {
		std::string file = "@AFA-explicit\n"
						   "%States-marked\n"
						   "%Alphabet-enum a b\n"
						   "q1 a & !q2 & b\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const mata::IntermediateAut aut = auts[0];
		REQUIRE(aut.transitions.front().first.name() == "1");
		REQUIRE(aut.transitions.front().first.raw == "q1");
	}

	SECTION("AFA explicit correct automatic naming - parentheses") {
		std::string file = "@AFA-explicit\n"
						   "%States-marked\n"
						   "%Alphabet-enum a b c\n"
						   "q1 ((a & !q2) & b) | c\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const mata::IntermediateAut aut = auts[0];
		REQUIRE(aut.transitions.front().first.name() == "1");
		REQUIRE(aut.transitions.front().first.raw == "q1");
	}

	SECTION("AFA explicit non existing symbol error") {
		std::string file = "@AFA-explicit\n"
						   "%States-marked\n"
						   "%Alphabet-enum a b\n"
						   "q1 a & !q2 & c\n";

		bool exception = false;
		parsed = parse_mf(file);
		try {
			mata::IntermediateAut::parse_from_mf(parsed);
		} catch (const std::runtime_error& e) { exception = true; }

		REQUIRE(exception);
	}
#endif

	SECTION("malformed transition with one token (#761)") {
		std::string file = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0\n";
		parsed = parse_mf(file);
		CHECK_THROWS_WITH(
			mata::IntermediateAut::parse_from_mf(parsed), Catch::Matchers::ContainsSubstring("transition [q0]")
		);
	}

	SECTION("malformed transition with two tokens (#761)") {
		std::string file = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 1\n";
		parsed = parse_mf(file);
		CHECK_THROWS_AS(mata::IntermediateAut::parse_from_mf(parsed), std::runtime_error);
	}

	SECTION("malformed transition with four tokens (#761)") {
		std::string file = "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q2\nq0 1 q1 q2\n";
		parsed = parse_mf(file);
		CHECK_THROWS_AS(mata::IntermediateAut::parse_from_mf(parsed), std::runtime_error);
	}

	SECTION("bad alphabet type suffix (#761)") {
		std::string file = "@NFA-bitz\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 1 q1\n";
		parsed = parse_mf(file);
		CHECK_THROWS_AS(mata::IntermediateAut::parse_from_mf(parsed), std::runtime_error);
	}

	SECTION("bad naming suffix (#761)") {
		std::string file = "@NFA-explicit\n%Alphabet-auto\n%States-numbers\n%Initial q0\n%Final q1\nq0 1 q1\n";
		parsed = parse_mf(file);
		CHECK_THROWS_AS(mata::IntermediateAut::parse_from_mf(parsed), std::runtime_error);
	}

	SECTION("ParsedSection operator[] on missing key (#761)") {
		std::string file = "@NFA-explicit\n%Initial q0\n";
		const auto sec = mata::parser::parse_mf_section(file);
		CHECK_THROWS_AS((void) sec["Final"], std::runtime_error);
	}

	SECTION("non-ASCII first character of the type line (#761)") {
		CHECK_THROWS_WITH(
			mata::parser::parse_mf_section("\xc3\xa9@NFA-explicit\n%Initial q0\n"),
			Catch::Matchers::ContainsSubstring("expecting automaton type")
		);
	}

	SECTION("operator as the source state or the symbol of an explicit transition (#761)") {
		for (const std::string type : {"NFA", "NFT"}) {
			for (const std::string line : {"q0 ) q1", "q0 ( q1", "q0 ! q1", "q0 | q1", "( a q1", "! a q1"}) {
				parsed = parse_mf("@" + type + "-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\n" + line + "\n");
				CHECK_THROWS_WITH(
					mata::IntermediateAut::parse_from_mf(parsed), Catch::Matchers::ContainsSubstring("is an operator")
				);
			}
		}
	}

	SECTION("unknown automaton type (#761)") {
		parsed = parse_mf("@DFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\n");
		CHECK_THROWS_WITH(
			mata::IntermediateAut::parse_from_mf(parsed), Catch::Matchers::ContainsSubstring("Unknown automaton type")
		);
	}

	SECTION("constructing an automaton without an alphabet (#761)") {
		parsed = parse_mf("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\n");
		const std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		CHECK_THROWS_AS(mata::nfa::builder::construct(auts[0], nullptr), std::runtime_error);
	}

	SECTION("bare @NFA and %Alphabet without a suffix stay accepted") {
		parsed = parse_mf("@NFA\n%Alphabet\n%Initial q0\n%Final q1\nq0 a q1\n");
		const std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		REQUIRE(auts.size() == 1);
		CHECK(auts[0].automaton_type == mata::IntermediateAut::AutomatonType::Nfa);
		CHECK(auts[0].alphabet_type == mata::IntermediateAut::AlphabetType::Explicit);
		CHECK(auts[0].symbol_naming == mata::IntermediateAut::Naming::Auto);
		REQUIRE(auts[0].transitions.size() == 1);
		CHECK(auts[0].transitions[0].first.raw == "q0");
		CHECK(auts[0].transitions[0].second.children()[0].node().name() == "a");
		CHECK(auts[0].transitions[0].second.children()[1].node().raw == "q1");
	}
} // parse_mf }}}

TEST_CASE("malformed formulas in the intermediate representation") { // {{{
	// Each of these used to read past the end of an operand or operator stack, which crashed Release builds.
	const auto check_rejected = [](const std::string& file, const std::string& message) {
		const Parsed parsed = parse_mf(file);
		CHECK_THROWS_WITH(mata::IntermediateAut::parse_from_mf(parsed), Catch::Matchers::ContainsSubstring(message));
	};

	SECTION("'(' without a matching ')'") {
		check_rejected("@NFA-explicit\n%Alphabet-auto\n%Initial (q0\n%Final q1\nq0 a q1\n", "Unbalanced parentheses");
	}

	SECTION("')' without a matching '('") {
		check_rejected("@NFA-explicit\n%Alphabet-auto\n%Initial q0 )\n%Final q1\nq0 a q1\n", "Unbalanced parentheses");
	}

	SECTION("binary operator without two operands") {
		check_rejected("@NFA-explicit\n%Alphabet-auto\n%Initial & q0\n%Final q1\nq0 a q1\n", "needs two operands");
		check_rejected("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 & & q1\n", "needs two operands");
	}

	SECTION("negation without an operand") {
		check_rejected("@NFA-explicit\n%Alphabet-auto\n%Initial !\n%Final q1\nq0 a q1\n", "without an operand");
	}

	SECTION("operands without an operator between them") {
		check_rejected(
			"@NFA-explicit\n%Alphabet-auto\n%Initial q0 & q1 q2\n%Final q1\nq0 a q1\n", "without an operator"
		);
		check_rejected("@NFA-bits\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a0 a1 q1\n", "without an operator");
		check_rejected("@NFA-bits\n%Alphabet-auto\n& \\true true a0\n", "without an operator");
	}

	SECTION("parenthesis as the target state") {
		check_rejected("@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a (\n", "unexpected '('");
	}

	SECTION("transition without a target state") {
		mata::IntermediateAut aut;
		aut.automaton_type = mata::IntermediateAut::AutomatonType::Nfa;
		const mata::FormulaNode source{mata::FormulaNode::Type::Operand, "q0", mata::FormulaNode::OperandType::State};
		aut.transitions.emplace_back(source, mata::FormulaGraph{});
		CHECK_THROWS_WITH(
			aut.get_symbol_part_of_transition(aut.transitions.front()),
			Catch::Matchers::ContainsSubstring("expected a symbol formula and a target state")
		);
	}
} // }}}

TEST_CASE("mata::Parser::ParsedSection::operator<<(ostream&)") { // {{{
	SECTION("aux") { WARN_PRINT("Insufficient testing of mata::Parser::ParsedSection::operator<<(ostream&)"); }

} // }}}

TEST_CASE("mata::IntermediateAut streaming a negated compound formula - #760") { // {{{
	// Before the fix, the unary branch of serialize_graph() recursed on the child node alone, dropped its children
	//  and printed the formula from reserved but unconstructed memory, with the operands missing.
	const auto print_transition = [](const std::string& formula) {
		const std::string file{"@NFA-bits\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 " + formula + " q1\n"};
		const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
		std::ostringstream out;
		out << auts[0];
		const std::string printed{out.str()};
		const size_t begin{printed.find("q0 -> ")};
		REQUIRE(begin != std::string::npos);
		return printed.substr(begin, printed.find('\n', begin) - begin);
	};

	SECTION("negated binary formula") { CHECK(print_transition("!(a0 & a1)") == "q0 -> !(a0 & a1) & q1"); }

	SECTION("negated nested binary formula") {
		CHECK(print_transition("!((a0 & a1) | a2)") == "q0 -> !((a0 & a1) | a2) & q1");
	}

	SECTION("negation under a binary operator") {
		CHECK(print_transition("a2 | !(a0 & !a1)") == "q0 -> (a2 | !(a0 & !a1)) & q1");
	}

	SECTION("double negation") { CHECK(print_transition("!!a0") == "q0 -> !(!a0) & q1"); }

	SECTION("operator node without children cannot be built") {
		const mata::FormulaNode conjunction{
			mata::FormulaNode::Type::Operator, "&", mata::FormulaNode::OperatorType::And
		};
		CHECK_THROWS_AS(mata::FormulaGraph{conjunction}, std::invalid_argument);
	}

	SECTION("empty formula throws instead of being printed") {
		mata::IntermediateAut aut{};
		aut.automaton_type = mata::IntermediateAut::AutomatonType::Nfa;
		aut.transitions.emplace_back(
			mata::FormulaNode{mata::FormulaNode::Type::Operand, "q0", mata::FormulaNode::OperandType::State},
			mata::FormulaGraph{}
		);
		std::ostringstream out;
		CHECK_THROWS_WITH(out << aut, Catch::Matchers::ContainsSubstring("neither an operand nor an operator"));
	}
} // }}}

TEST_CASE("mata::IntermediateAut keeps the names of its formula nodes") { // {{{
	// A node keeps a view of its name into the name storage of its automaton instead of a string of its own, so a copy
	//  and a move have to keep that storage alive.
	const std::string file{"@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a0 q1\n"};

	SECTION("a copy and a move outliving the parse result still read their names") {
		std::optional<mata::IntermediateAut> copied{};
		std::optional<mata::IntermediateAut> moved{};
		{
			const auto auts = mata::IntermediateAut::parse_from_mf(parse_mf(file));
			REQUIRE(auts.size() == 1);
			copied = auts[0];
			moved = std::move(auts[0]);
		}

		for (const mata::IntermediateAut* aut : {&*copied, &*moved}) {
			REQUIRE(aut->transitions.size() == 1);
			CHECK(aut->transitions[0].first.raw == "q0");
			CHECK(aut->transitions[0].first.name() == "0");
			CHECK(aut->transitions[0].second.children()[0].node().raw == "a0");
			CHECK(aut->transitions[0].second.children()[1].node().raw == "q1");
			CHECK(aut->transitions[0].second.children()[1].node().name() == "1");
		}
	}

	SECTION("tokens pointing outside the storage of their section are copied") {
		std::optional<mata::IntermediateAut> aut{};
		{
			mata::parser::ParsedSection section{parse_mf(file).front()};
			const std::string source{"q0"};
			const std::string symbol{"a1"};
			const std::string target{"q2"};
			section.body.push_back({{source, false}, {symbol, false}, {target, false}});
			aut = mata::IntermediateAut::parse_from_mf({section}).front();
		}
		REQUIRE(aut->transitions.size() == 2);
		CHECK(aut->transitions[0].second.children()[0].node().raw == "a0");
		CHECK(aut->transitions[1].first.raw == "q0");
		CHECK(aut->transitions[1].second.children()[0].node().raw == "a1");
		CHECK(aut->transitions[1].second.children()[1].node().raw == "q2");
	}

	SECTION("add_transition stores the names it is given") {
		mata::IntermediateAut aut{};
		const std::string source_name{"q0"};
		const std::string symbol_name{"a0"};
		const std::string target_name{"q1"};
		aut.add_transition(
			mata::FormulaNode{
				mata::FormulaNode::Type::Operand, source_name, mata::FormulaNode::OperandType::State, true
			},
			mata::FormulaNode{mata::FormulaNode::Type::Operand, symbol_name, mata::FormulaNode::OperandType::Symbol},
			mata::FormulaGraph{mata::FormulaNode{
				mata::FormulaNode::Type::Operand, target_name, mata::FormulaNode::OperandType::State, true
			}}
		);

		REQUIRE(aut.transitions.size() == 1);
		CHECK(aut.transitions[0].first.raw == "q0");
		CHECK(aut.transitions[0].first.name() == "0");
		CHECK(aut.transitions[0].second.children()[0].node().raw == "a0");
		CHECK(aut.transitions[0].second.children()[1].node().raw == "q1");
		CHECK(aut.transitions[0].second.children()[1].node().name() == "1");
		// The automaton keeps copies, not views of the strings the caller owns.
		CHECK(aut.transitions[0].first.raw.data() != source_name.data());
		CHECK(aut.transitions[0].second.children()[0].node().raw.data() != symbol_name.data());
		CHECK(aut.transitions[0].second.children()[1].node().raw.data() != target_name.data());
	}
} // }}}
