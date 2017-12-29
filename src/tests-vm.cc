// TODO: some header

#include "../3rdparty/catch.hpp"

#include <vata2/vm.hh>

using namespace Vata2::Parser;
using namespace Vata2::VM;

TEST_CASE("Vata2::VM::VirtualMachine::run_code() correct calls")
{
	// we wish to catch output
	std::stringstream cout_buf;
	std::streambuf* old_cout = std::cout.rdbuf(cout_buf.rdbuf());

	// setting the environment
	VirtualMachine mach;
	ParsedSection sec;
	sec.type = "CODE";

	SECTION("empty program")
	{
		mach.run_code(sec);
	}

	SECTION("Hello World")
	{
		sec.body.push_back({"(", "print", "Hello World!", ")"});
		mach.run_code(sec);

		REQUIRE(cout_buf.str() == "Hello World!");
	}

	std::cout.rdbuf(old_cout);

	SECTION("aux")
	{
		DEBUG_PRINT("Insufficient testing of Vata2::VM::VirtualMachine::run_code()");
	}

}

TEST_CASE("Vata2::VM::VirtualMachine::run_code() invalid calls")
{
	// setting the environment
	VirtualMachine mach;
	ParsedSection sec;
	sec.type = "CODE";

	SECTION("incorrectly formed code 1")
	{
		sec.body.push_back({"(", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("XXXXXXXXXXXXXXXXXXXXXXXXXX"));
	}

	SECTION("incorrectly formed code 2")
	{
		sec.body.push_back({"(", "(", "load_aut", "a", ")", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("XXXXXXXXXXXXXXXXXXXXXXXXXX"));
	}

	SECTION("incorrectly formed code 3")
	{
		sec.body.push_back({"(", "load_aut", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("XXXXXXXXXXXXXXXXXXXXXXXXXX"));
	}

	SECTION("aux")
	{
		DEBUG_PRINT("Insufficient testing of Vata2::VM::VirtualMachine::run_code()");
	}
}
