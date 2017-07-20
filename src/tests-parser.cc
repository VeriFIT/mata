// TODO: some header

#include "../3rdparty/catch.hpp"

#include <vata2/parser.hh>
#include <vata2/util.hh>

using namespace Vata2::Parser;


TEST_CASE("correct use of Vata2::Parser::parse_vtf_section()")
{
	ParsedSection parsec;

	SECTION("empty file")
	{
		std::string file =
			"@Type\n";

		parsec = parse_vtf_section(file);

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

		parsec = parse_vtf_section(file);

		REQUIRE("Type" == parsec.type);
		REQUIRE(parsec.dict.find("key1") != parsec.dict.end());
		REQUIRE(parsec.dict.at("key1").empty());
		REQUIRE(parsec.dict.find("key2") != parsec.dict.end());
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

		parsec = parse_vtf_section(file);

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

		parsec = parse_vtf_section(file);

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
			"%Transitions\n"
			"a\n"
			"b0 b1 b2 b3		b4    b5";

		parsec = parse_vtf_section(file);

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
			"%Transitions\t\n"
			"a\n"
			"   b0 b1 #b2";

		parsec = parse_vtf_section(file);

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
			"%key4 \"val 1   \" \n"
			"%key5\n"
			"\"%key6\"\n"
			"%key7 %key8\n"
			"%Transitions\n"
			"a \"\"\n"
			"b0 \"b 1\" c d\n"
			"c 0 \"\\\"he's so cool,\\\" he said\" c d\n"
			"\"a\"\n"
			"\"\"\n"
			"'\n"
			"q a q'";

		parsec = parse_vtf_section(file);

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
		REQUIRE(ref->size() == 1);
		REQUIRE((*ref)[0] == "%key6");
		ref = &parsec.dict.at("key7");
		REQUIRE(ref->size() == 0);
		ref = &parsec.dict.at("key8");
		REQUIRE(ref->size() == 0);
		REQUIRE(parsec.body.size() == 7);
		std::vector<BodyLine> body(parsec.body.begin(), parsec.body.end());
		REQUIRE(body[0].size() == 2);
		REQUIRE(body[0][0] == "a");
		REQUIRE(body[0][1] == "");
		REQUIRE(body[1].size() == 4);
		REQUIRE(body[1][0] == "b0");
		REQUIRE(body[1][1] == "b 1");
		REQUIRE(body[1][2] == "c");
		REQUIRE(body[1][3] == "d");
		REQUIRE(body[2].size() == 5);
		REQUIRE(body[2][0] == "c");
		REQUIRE(body[2][1] == "0");
		REQUIRE(body[2][2] == "\"he's so cool,\" he said");
		REQUIRE(body[2][3] == "c");
		REQUIRE(body[2][4] == "d");
		REQUIRE(body[3].size() == 1);
		REQUIRE(body[3][0] == "a");
		REQUIRE(body[4].size() == 1);
		REQUIRE(body[4][0] == "");
		REQUIRE(body[5].size() == 1);
		REQUIRE(body[5][0] == "'");
		REQUIRE(body[6].size() == 3);
		REQUIRE(body[6][0] == "q");
		REQUIRE(body[6][1] == "a");
		REQUIRE(body[6][2] == "q'");
	}

	SECTION("file with newlines among keys")
	{
		std::string file =
			"@Type\n"
			"%key1 value1.1\n"
			"value1.2   # comment\n"
			"   value1.3\n"
			"%key2\n"
			"%key3 \"value3\"";

		parsec = parse_vtf_section(file);

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

	SECTION("special characters inside strings")
	{
		std::string file =
			"@Type\n"
			"%key1     value@1  value@2\n"
			"%key2     value%1  value%2\n";

		parsec = parse_vtf_section(file);

		REQUIRE("Type" == parsec.type);
		const KeyListStore::mapped_type* ref = &parsec.dict.at("key1");
		REQUIRE(ref->size() == 2);
		REQUIRE((*ref)[0] == "value@1");
		REQUIRE((*ref)[1] == "value@2");
		ref = &parsec.dict.at("key2");
		REQUIRE(ref->size() == 2);
		REQUIRE((*ref)[0] == "value%1");
		REQUIRE((*ref)[1] == "value%2");
		REQUIRE(parsec.body.empty());
	}

	SECTION("file with no keys")
	{
		std::string file =
			"@Type\n"
			"%Transitions\n"
			"a b c\n";

		parsec = parse_vtf_section(file);

		REQUIRE("Type" == parsec.type);
		REQUIRE(parsec.body.size() == 1);
		std::vector<BodyLine> body(parsec.body.begin(), parsec.body.end());
		REQUIRE(body[0].size() == 3);
		REQUIRE(body[0][0] == "a");
		REQUIRE(body[0][1] == "b");
		REQUIRE(body[0][2] == "c");
	}
}

TEST_CASE("incorrect use of Vata2::Parser::parse_vtf_section()")
{
	ParsedSection parsec;

	SECTION("no type")
	{
		std::string file =
			"@\n"
			"Type"
			"%key1\n"
			"%key2\n"
			"%Transitions\n";

		CHECK_THROWS_WITH(parse_vtf_section(file),
			Catch::Contains("@TYPE name missing"));
	}

	SECTION("missing type")
	{
		std::string file =
			"%key1\n"
			"%key2\n"
			"%Transitions\n";

		CHECK_THROWS_WITH(parse_vtf_section(file),
			Catch::Contains("expecting automaton type"));
	}

	SECTION("stray characters")
	{
		std::string file =
			"@Type\n"
			"hello\n"
			"%key1\n"
			"%key2\n"
			"%Transitions\n";

		CHECK_THROWS_WITH(parse_vtf_section(file), Catch::Contains("expecting key"));
	}

	SECTION("unterminated quote")
	{
		std::string file =
			"@Type\n"
			"%key1 \"value\n"
			"%Transitions\n";

		CHECK_THROWS_WITH(parse_vtf_section(file),
			Catch::Contains("missing ending quotes"));
	}

	SECTION("unterminated quote 2")
	{
		std::string file =
			"@Type\n"
			"%key1 \"\n";

		CHECK_THROWS_WITH(parse_vtf_section(file),
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

		CHECK_THROWS_WITH(parse_vtf_section(file),
			Catch::Contains("missing ending quotes"));
	}

	SECTION("quoted strings starting in the middle of strings")
	{
		std::string file =
			"@Type\n"
			"%key1 val\"ue\"\n";

		CHECK_THROWS_WITH(parse_vtf_section(file),
			Catch::Contains("misplaced quotes"));
	}

	SECTION("quoted strings ending in the middle of strings")
	{
		std::string file =
			"@Type\n"
			"%key1 \"val\"ue\n";

		CHECK_THROWS_WITH(parse_vtf_section(file),
			Catch::Contains("misplaced quotes"));
	}

	SECTION("incorrect position of special characters")
	{
		std::string file =
			"@Type\n"
			"%key1 @here";

		CHECK_THROWS_WITH(parse_vtf_section(file),
			Catch::Contains("invalid position of @TYPE: @here"));
	}

	SECTION("incorrect position of special characters 2")
	{
		std::string file =
			"@Type\n"
			"%Transitions\n"
			"q1 @here q2";

		CHECK_THROWS_WITH(parse_vtf_section(file),
			Catch::Contains("invalid position of @TYPE: @here"));
	}

	SECTION("incorrect position of special characters 3")
	{
		std::string file =
			"@Type\n"
			"%Transitions\n"
			"q1 %here q2";

		CHECK_THROWS_WITH(parse_vtf_section(file),
			Catch::Contains("invalid position of %KEY: %here"));
	}

	SECTION("no key name")
	{
		std::string file =
			"@Type\n"
			"%\n"
			"%key2\n";

		CHECK_THROWS_WITH(parse_vtf_section(file),
			Catch::Contains("%KEY name missing"));
	}
}


TEST_CASE("correct use of Vata2::Parser::parse_vtf()")
{
	DEBUG_PRINT("Insufficent testing of Vata2::Parser::parse_vtf()");
}
