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
		sec.body.push_back({"(", "print", "Hello World!", ")"});

		// we wish to catch output
		std::ostringstream cout_buf;
		cout_redirect cout_guard(cout_buf.rdbuf());

		mach.run_code(sec);

		REQUIRE(cout_buf.str() == "Hello World!");
	}

	SECTION("aux")
	{
		WARN_PRINT("Insufficient testing of Vata2::VM::VirtualMachine::run_code()");
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
			Catch::Contains("is not a valid function call"));
	}

	SECTION("incorrectly formed code 2")
	{
		sec.body.push_back({"(", "(", "return", "a", ")", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("is not a valid function call"));
	}

	SECTION("incorrectly formed code 3")
	{
		sec.body.push_back({"(", "load_aut", ")"});
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
		sec.body.push_back({"(", "return", "a", ")", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("mismatched parenthesis"));
	}

	SECTION("mismatched parenthesis 2")
	{
		sec.body.push_back({"(", "foo", "(", "return", "a",  ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("dangling code"));
	}

	SECTION("incorrect number of parameters 1")
	{
		sec.body.push_back({"(", "print", "Hello", "World", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("does not match arity of print"));
	}

	SECTION("incorrect number of parameters 2")
	{
		sec.body.push_back({"(", "foo", "(", "print", "Hello", "World", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("does not match arity of print"));
	}

	SECTION("aux")
	{
		WARN_PRINT("Insufficient testing of Vata2::VM::VirtualMachine::run_code()");
	}
}

TEST_CASE("Vata2::VM::VirtualMachine::default_dispatch() calls")
{
	// setting the environment
	VirtualMachine mach;
	ParsedSection sec;
	sec.type = "CODE";

	SECTION("return with more than 1 argument")
	{
		sec.body.push_back({"(", "return", "arg1", "args2", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("requires 1 argument"));
	}

	SECTION("invalid function name")
	{
		sec.body.push_back({"(", "invalid_func_name", "arg1", ")"});
		CHECK_THROWS_WITH(mach.run_code(sec),
			Catch::Contains("is not a defined function"));
	}

	SECTION("aux")
	{
		WARN_PRINT("Insufficient testing of Vata2::VM::VirtualMachine::run_code()");
	}
}
