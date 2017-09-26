// TODO: add header

#include <tuple>

#include <vata2/nfa.hh>
#include <vata2/vm-dispatch.hh>

#include "dispatch-aux.hh"

using namespace Vata2::Nfa;
using namespace Vata2::VM;

using Vata2::Parser::ParsedSection;
using Vata2::dispatch::test_and_call;


namespace
{
	VMValue nfa_dispatch(
		const VMFuncName&  func_name,
		const VMFuncArgs&  func_args)
	{
		DEBUG_PRINT("calling function \"" + func_name + "\" for NFA");

		// we use throw to return result from test_and_call
		try {

			test_and_call("construct", func_name, {"Parsec"}, func_args, "NFA",
				*[](const ParsedSection& parsec) -> auto {
					return static_cast<VMPointer>(new Nfa(construct(parsec)));
				});

		}
		catch (VMValue res)
		{
			return res;
		}

		throw std::runtime_error("invalid function name in nfa_dispatch: " +
			std::to_string(func_name));
	}
}


void Vata2::Nfa::init()
{
	reg_dispatcher("NFA", nfa_dispatch);
}
