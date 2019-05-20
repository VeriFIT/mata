// TODO: add header

#include <vata2/vm-dispatch.hh>

#include "dispatch-aux.hh"
#include "bool.hh"

using namespace Vata2::VM;
using Vata2::dispatch::test_and_call;

namespace
{
	VMValue bool_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{ // {{{
		DEBUG_PRINT("calling function \"" + func_name + "\" for " + Vata2::TYPE_BOOL);

		if ("delete" == func_name) {
			assert(func_args.size() == 1);
			const VMValue& arg1 = func_args[0];
			assert(Vata2::TYPE_BOOL == arg1.type);
			const bool* bl = static_cast<const bool*>(arg1.get_ptr());
			assert(nullptr != bl);
			delete bl;
			return VMValue(Vata2::TYPE_VOID, nullptr);
		}

		// we use throw to return result from test_and_call
		try {

			test_and_call("print", func_name, {Vata2::TYPE_BOOL}, func_args, Vata2::TYPE_VOID,
				*[](bool b) -> auto {
					// TODO: remove the EOL
					std::cout << b << "\n";
					return static_cast<VMPointer>(nullptr);
				});

			test_and_call("copy", func_name, {Vata2::TYPE_BOOL}, func_args, Vata2::TYPE_BOOL,
				*[](bool b) -> auto {
					bool* ptr = new bool(b);
					return static_cast<VMPointer>(ptr);
				});

		}
		catch (VMValue res)
		{
			return res;
		}

		return VMValue(Vata2::TYPE_NOT_A_VALUE, nullptr);
	}
} // bool_dispatch }}}


void Vata2::Bool::init()
{
	reg_dispatcher(TYPE_BOOL, bool_dispatch, "a boolean data type");
}

