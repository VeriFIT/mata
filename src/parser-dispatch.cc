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
		DEBUG_PRINT("calling function \"" + func_name + "\" for " + Vata2::TYPE_PARSEC);

		if ("info" == func_name) {
			assert(func_args.size() == 0);
			std::string* new_str = new std::string;
			*new_str = "parsed section (one section of .vtf format)";
			return VMValue(Vata2::TYPE_STR, new_str);
		}

		// we use throw to return result from test_and_call
		try {

			test_and_call("copy", func_name, {Vata2::TYPE_PARSEC}, func_args, Vata2::TYPE_PARSEC,
				*[](const ParsedSection& sec) -> auto {
					ParsedSection* new_parsec = new ParsedSection(sec);
					return static_cast<VMPointer>(new_parsec);
				});

		}
		catch (VMValue res)
		{
			return res;
		}

		return VMValue(Vata2::TYPE_NOT_A_VALUE, nullptr);
	}
}


void Vata2::Parser::init()
{
	reg_dispatcher(Vata2::TYPE_PARSEC, parsec_dispatch);
}
