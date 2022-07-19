// TODO: add header

#include <mata/vm-dispatch.hh>

#include "dispatch-aux.hh"
#include "void.hh"

using namespace Mata::VM;
using Mata::dispatch::test_and_call;

namespace
{
	VMValue void_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{
		DEBUG_PRINT("calling function \"" + func_name + "\" for " + Mata::TYPE_VOID);

		if ("delete" == func_name) {
			assert(func_args.size() == 1);
			return VMValue(Mata::TYPE_VOID, nullptr);
		}

		return VMValue(Mata::TYPE_NOT_A_VALUE, nullptr);
	}
}


void Mata::Void::init()
{
	reg_dispatcher(Mata::TYPE_VOID, void_dispatch,
                   "void; similar to void in C, used as a return type of procedures");
}
