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

		VMValue res;

		test_and_call(&res, "construct", func_name, {"Parsec"}, func_args, "NFA",
			*[](const ParsedSection& parsec) -> auto {
				DEBUG_PRINT("In <noname>()!");
				DEBUG_PRINT(std::to_string(parsec));
				return static_cast<VMPointer>(new Nfa(construct(parsec)));
			});

		assert(false);

		// TODO
	}
}


void Vata2::Nfa::init()
{
	reg_dispatcher("NFA", nfa_dispatch);
}
