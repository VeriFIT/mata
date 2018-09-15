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
		DEBUG_PRINT("calling function \"" + func_name + "\" for void");

		if ("delete" == func_name) {
			assert(func_args.size() == 1);
			return VMValue("void", nullptr);
		}

		return VMValue("NaV", nullptr);
	}
}


void Vata2::Void::init()
{
	reg_dispatcher("void", void_dispatch);
}
