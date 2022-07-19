// TODO: add header

#include <mata/vm-dispatch.hh>

#include "dispatch-aux.hh"
#include <mata/parser.hh>

using namespace Mata::VM;
using Mata::Parser::ParsedSection;
using Mata::dispatch::test_and_call;

namespace
{
	VMValue parsec_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{
		DEBUG_PRINT("calling function \"" + func_name + "\" for " + Mata::TYPE_PARSEC);

		// we use throw to return result from test_and_call
		try {

			test_and_call("copy", func_name, {Mata::TYPE_PARSEC}, func_args, Mata::TYPE_PARSEC,
                          *[](const ParsedSection& sec) -> auto {
					ParsedSection* new_parsec = new ParsedSection(sec);
					return static_cast<VMPointer>(new_parsec);
				});

		}
		catch (VMValue res)
		{
			return res;
		}

		return VMValue(Mata::TYPE_NOT_A_VALUE, nullptr);
	}
}


void Mata::Parser::init()
{
	reg_dispatcher(Mata::TYPE_PARSEC, parsec_dispatch,
                   "parsed section (one section of .vtf format)");
}
