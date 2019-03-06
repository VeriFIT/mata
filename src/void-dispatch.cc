// TODO: add header

#include <vata2/vm-dispatch.hh>

#include "dispatch-aux.hh"
#include "void.hh"

using namespace Vata2::VM;
using Vata2::dispatch::test_and_call;

namespace
{
	VMValue void_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{
		DEBUG_PRINT("calling function \"" + func_name + "\" for " + Vata2::TYPE_VOID);

		if ("delete" == func_name) {
			assert(func_args.size() == 1);
			return VMValue(Vata2::TYPE_VOID, nullptr);
		}

		if ("info" == func_name) {
			assert(func_args.size() == 0);
			std::string* new_str = new std::string;
			*new_str = "void; similar to void in C, used as a return type of procedures";
			return VMValue(Vata2::TYPE_STR, new_str);
		}

		return VMValue(Vata2::TYPE_NOT_A_VALUE, nullptr);
	}
}


void Vata2::Void::init()
{
	reg_dispatcher(Vata2::TYPE_VOID, void_dispatch);
}
