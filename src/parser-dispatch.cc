// TODO: add header

#include <vata2/vm-dispatch.hh>

#include "dispatch-aux.hh"
#include <vata2/parser.hh>

using namespace Vata2::VM;
using Vata2::Parser::ParsedSection;
using Vata2::dispatch::test_and_call;

namespace
{
	VMValue parsec_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{
		DEBUG_PRINT("calling function \"" + func_name + "\" for Parsec");

		// we use throw to return result from test_and_call
		try {

			test_and_call("copy", func_name, {"Parsec"}, func_args, "Parsec",
				*[](const ParsedSection& sec) -> auto {
					ParsedSection* new_parsec = new ParsedSection(sec);
					return static_cast<VMPointer>(new_parsec);
				});

		}
		catch (VMValue res)
		{
			return res;
		}

		return VMValue("NaV", nullptr);
	}
}


void Vata2::Parser::init()
{
	reg_dispatcher("Parsec", parsec_dispatch);
}
