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
	{ // {{{
		DEBUG_PRINT("calling function \"" + func_name + "\" for " + Vata2::TYPE_STR);

		if ("delete" == func_name) {
			assert(func_args.size() == 1);
			const VMValue& arg1 = func_args[0];
			assert(Vata2::TYPE_STR == arg1.type);
			const std::string* str = static_cast<const std::string*>(arg1.get_ptr());
			assert(nullptr != str);
			delete str;
			return VMValue(Vata2::TYPE_VOID, nullptr);
		}

		if ("info" == func_name) {
			assert(func_args.size() == 0);
			std::string* new_str = new std::string;
			*new_str = "a string data type";
			return VMValue(Vata2::TYPE_STR, new_str);
		}

		// we use throw to return result from test_and_call
		try {

			test_and_call("print", func_name, {Vata2::TYPE_STR}, func_args, Vata2::TYPE_VOID,
				*[](const std::string& str) -> auto {
					std::cout << str;
					return static_cast<VMPointer>(nullptr);
				});

			test_and_call("copy", func_name, {Vata2::TYPE_STR}, func_args, Vata2::TYPE_STR,
				*[](const std::string& str) -> auto {
					std::string* ptr = new std::string(str);
					return static_cast<VMPointer>(ptr);
				});

		}
		catch (VMValue res)
		{
			return res;
		}

		return VMValue(Vata2::TYPE_NOT_A_VALUE, nullptr);
	}
} // str_dispatch }}}


void Vata2::Str::init()
{
	reg_dispatcher(TYPE_STR, str_dispatch);
}
