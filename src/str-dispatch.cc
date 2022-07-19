// TODO: add header

#include <mata/vm-dispatch.hh>

#include "dispatch-aux.hh"
#include "str.hh"

using namespace Mata::VM;
using Mata::dispatch::test_and_call;

namespace
{
	VMValue str_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{ // {{{
		DEBUG_VM_HIGH_PRINT("calling function \"" + func_name + "\" for " + Mata::TYPE_STR);

		if ("delete" == func_name) {
			assert(func_args.size() == 1);
			const VMValue& arg1 = func_args[0];
			assert(Mata::TYPE_STR == arg1.type || Mata::TYPE_TOKEN == arg1.type);
			const std::string* str = static_cast<const std::string*>(arg1.get_ptr());
			assert(nullptr != str);
			delete str;
			return VMValue(Mata::TYPE_VOID, nullptr);
		}

		// we use throw to return result from test_and_call
		try {

			test_and_call("print", func_name, {Mata::TYPE_STR}, func_args, Mata::TYPE_VOID,
                          *[](const std::string& str) -> auto {
					std::cout << str;
					return static_cast<VMPointer>(nullptr);
				});

			test_and_call("copy", func_name, {Mata::TYPE_STR}, func_args, Mata::TYPE_STR,
                          *[](const std::string& str) -> auto {
					std::string* ptr = new std::string(str);
					return static_cast<VMPointer>(ptr);
				});

		}
		catch (VMValue res)
		{
			return res;
		}

		return VMValue(Mata::TYPE_NOT_A_VALUE, nullptr);
	}
} // str_dispatch }}}


void Mata::Str::init()
{
	reg_dispatcher(TYPE_STR, str_dispatch, "a string data type");
	reg_dispatcher(TYPE_TOKEN, str_dispatch, "a token data type");
}
