// TODO: some header

#include "../3rdparty/catch.hpp"

#include <vata2/vm.hh>

using namespace Vata2::Parser;
using namespace Vata2::VM;

namespace {

/// A helper class that guards rebinding of std::cout.rdbuf()
class cout_redirect
{
private:
	std::streambuf* old;

public:
	cout_redirect(std::streambuf* new_buf) : old(std::cout.rdbuf(new_buf)) { }
	cout_redirect(const cout_redirect& rhs) = delete;
	cout_redirect& operator=(const cout_redirect& rhs) = delete;
	~cout_redirect() { this->release(); }

	void release()
	{
		if (nullptr != this->old) { std::cout.rdbuf(this->old); }
		this->old = nullptr;
	}
};

} /* anonymous namespace */

/*
TEST_CASE("Vata2::VM::VirtualMachine::run_code() correct calls")
{
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
		sec.body.push_back({"(", "print", "\"Hello World!\"", ")"});

		// we wish to catch output
		std::ostringstream cout_buf;
		cout_redirect cout_guard(cout_buf.rdbuf());

		mach.run_code(sec);

		REQUIRE(cout_buf.str() == "Hello World!");
	}

	SECTION("load_file")
	{
		sec.body.push_back({"a1", "=", "(", "load_file", "\"unit-test-data/nfa-a.vtf\"", ")"});

		// we wish to catch output
		std::ostringstream cout_buf;
		cout_redirect cout_guard(cout_buf.rdbuf());

		mach.run_code(sec);
	}

	SECTION("aux")
	{
		WARN_PRINT("Insufficient testing of Vata2::VM::VirtualMachine::run_code()");
	}

}
*/

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
			Catch::Contains("is not a valid function call"));
	}

	SECTION("incorrectly formed code 2")
	{
		sec.body.push_back({"(", "(", "return", "\"a\"", ")", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("is not a valid function call"));
	}

	SECTION("incorrectly formed code 3")
	{
		sec.body.push_back({"(", "load_file", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("is not a valid function call"));
	}

	SECTION("incorrectly formed code 4")
	{
		sec.body.push_back({"foo", "(", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("is not a valid function call"));
	}

	SECTION("mismatched parenthesis 1")
	{
		sec.body.push_back({"(", "return", "\"a\"", ")", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("mismatched parenthesis"));
	}

	SECTION("mismatched parenthesis 2")
	{
		sec.body.push_back({"(", "foo", "(", "return", "\"a\"",  ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("dangling code"));
	}

	SECTION("incorrect number of parameters 1")
	{
		sec.body.push_back({"(", "print", "\"Hello\"", "\" World\"", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("does not match arity of print"));
	}

	SECTION("incorrect number of parameters 2")
	{
		sec.body.push_back({"(", "foo", "(", "print", "\"Hello World\"", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("dangling code"));
	}

	SECTION("aux")
	{
		WARN_PRINT("Insufficient testing of Vata2::VM::VirtualMachine::run_code()");
	}
}

/*
TEST_CASE("Vata2::VM::VirtualMachine::run() calls")
{
	VirtualMachine mach;
	ParsedSection sec;

	SECTION("call for an empty CODE section")
	{
		sec.type = "CODE";
		mach.run(sec);
	}

	SECTION("call for an unnamed empty automaton")
	{
		sec.type = "NFA";
		mach.run(sec);
	}

	SECTION("call for a named empty automaton")
	{
		sec.type = "NFA";
		sec.dict.insert({"Name", {"a1"}});
		mach.run(sec);

		VMValue val_a1 = mach.load_from_storage("a1");
		REQUIRE(val_a1.type == "NFA");
	}

	SECTION("aux")
	{
		WARN_PRINT("Insufficient testing of Vata2::VM::VirtualMachine::run()");
	}
}

TEST_CASE("Vata2::VM::VirtualMachine::load_from_storage() calls")
{
	VirtualMachine mach;
	SECTION("accessing a missing element 1")
	{
		CHECK_THROWS_WITH(mach.load_from_storage("foo"),
			Catch::Contains("is not in the memory"));
	}

	SECTION("accessing a missing element 1")
	{
		ParsedSection sec;
		sec.type = "NFA";
		sec.dict.insert({"Name", {"foo"}});
		mach.run(sec);

		CHECK_THROWS_WITH(mach.load_from_storage("bar"),
			Catch::Contains("is not in the memory"));
	}
}
 */

TEST_CASE("Vata2::VM::VirtualMachine::default_dispatch() calls")
{
	// setting the environment
	VirtualMachine mach;
	ParsedSection sec;
	sec.type = "CODE";

	SECTION("return with more than 1 argument")
	{
		sec.body.push_back({"(", "return", "\"arg1\"", "\"arg2\"", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("requires 1 argument"));
	}

	SECTION("invalid function name")
	{
		sec.body.push_back({"(", "invalid_func_name", "\"arg1\"", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("is not a defined function"));
	}

	SECTION("aux")
	{
		WARN_PRINT("Insufficient testing of Vata2::VM::VirtualMachine::run_code()");
	}
}
