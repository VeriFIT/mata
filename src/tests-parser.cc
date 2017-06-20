// TODO: some header

#include "../3rdparty/catch.hpp"

#include <vata2/parser.hh>
#include <vata2/util.hh>

using namespace Vata2::Parser;


TEST_CASE("correct use of Vata2::Parser::parse_vtf()")
{
	Parsed parsed;

	SECTION("empty file")
	{
		std::string file =
			"@NFA\n"
			"%Initial\n"
			"%Final\n"
			"%Transitions\n";

		 parsed = parse_vtf(file);
	}




	DEBUG_PRINT("Vata2::Parser::parse_vtf() not tested properly");
}

TEST_CASE("incorrect use of Vata2::Parser::parse_vtf()")
{
	Parsed parsed;

	SECTION("XXXXXXXXXXXXXXXX")
	{
	}

	DEBUG_PRINT("Vata2::Parser::parse_vtf() not tested properly");
}
