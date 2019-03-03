// TODO: add header

#include <vata2/vm-dispatch.hh>

#include "dispatch-aux.hh"
#include "str.hh"

using namespace Vata2::VM;
using Vata2::dispatch::test_and_call;

namespace
{
	VMValue str_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{
		DEBUG_PRINT("calling function \"" + func_name + "\" for str");

		if ("delete" == func_name) {
			assert(func_args.size() == 1);
			const VMValue& arg1 = func_args[0];
			assert("str" == arg1.type);
			const std::string* str = static_cast<const std::string*>(arg1.get_ptr());
			assert(nullptr != str);
			delete str;
			return VMValue("void", nullptr);
		}

		// we use throw to return result from test_and_call
		try {

			test_and_call("print", func_name, {"str"}, func_args, "void",
				*[](const std::string& str) -> auto {
					std::cout << str;
					return static_cast<VMPointer>(nullptr);
				});

			test_and_call("copy", func_name, {"str"}, func_args, "str",
				*[](const std::string& str) -> auto {
					std::string* ptr = new std::string(str);
					return static_cast<VMPointer>(ptr);
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
	reg_dispatcher("str", str_dispatch);
}
