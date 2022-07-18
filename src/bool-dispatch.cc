// TODO: add header

#include <mata/vm-dispatch.hh>

#include "dispatch-aux.hh"
#include "bool.hh"

using namespace Mata::VM;
using Mata::dispatch::test_and_call;

namespace
{
	VMValue bool_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{ // {{{
		DEBUG_PRINT("calling function \"" + func_name + "\" for " + Mata::TYPE_BOOL);

		if ("delete" == func_name) {
			assert(func_args.size() == 1);
			const VMValue& arg1 = func_args[0];
			assert(Mata::TYPE_BOOL == arg1.type);
			const bool* bl = static_cast<const bool*>(arg1.get_ptr());
			assert(nullptr != bl);
			delete bl;
			return VMValue(Mata::TYPE_VOID, nullptr);
		}

		// we use throw to return result from test_and_call
		try {

			test_and_call("print", func_name, {Mata::TYPE_BOOL}, func_args, Mata::TYPE_VOID,
                          *[](bool b) -> auto {
					std::cout << b;
					return static_cast<VMPointer>(nullptr);
				});

			test_and_call("copy", func_name, {Mata::TYPE_BOOL}, func_args, Mata::TYPE_BOOL,
                          *[](bool b) -> auto {
					bool* ptr = new bool(b);
					return static_cast<VMPointer>(ptr);
				});

		}
		catch (VMValue res)
		{
			return res;
		}

		return VMValue(Mata::TYPE_NOT_A_VALUE, nullptr);
	}
} // bool_dispatch }}}


void Mata::Bool::init()
{
	reg_dispatcher(TYPE_BOOL, bool_dispatch, "a boolean data type");
}

