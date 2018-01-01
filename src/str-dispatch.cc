// TODO: add header

#include <vata2/vm-dispatch.hh>

#include "dispatch-aux.hh"
#include "str.hh"

using namespace Vata2::VM;
using Vata2::dispatch::test_and_call;

namespace
{
	VMValue string_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{
		DEBUG_PRINT("calling function \"" + func_name + "\" for string");

		// we use throw to return result from test_and_call
		try {

			test_and_call("print", func_name, {"string"}, func_args, "void",
				*[](const std::string& str) -> auto {
					std::cout << str;
					return static_cast<VMPointer>(nullptr);
				});

		}
		catch (VMValue res)
		{
			return res;
		}

		return VMValue("NaV", nullptr);
	}
}


void Vata2::Str::init()
{
	reg_dispatcher("string", string_dispatch);
}
