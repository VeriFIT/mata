// TODO: some header

#include "../3rdparty/catch.hpp"

#include <vata2/vm-dispatch.hh>

using namespace Vata2::VM;

TEST_CASE("Vata2::VM::find_dispatcher()")
{
	SECTION("invalid type")
	{
		CHECK_THROWS_WITH(find_dispatcher("UNKNOWN"),
			Catch::Contains("cannot find the dispatcher"));
	}

	SECTION("valid type")
	{
		size_t n42 = 42;
		auto f = [&n42](const VMFuncName&, const VMFuncArgs&) -> VMValue {
			return VMValue("ANSWER", &n42); };

		reg_dispatcher("FOO", f);

		VMValue val = find_dispatcher("FOO")("BAR", { });
		REQUIRE(val.type == "ANSWER");
		REQUIRE(val.get_ptr() == &n42);
	}
}
