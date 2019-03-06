/* nfa-dispatch.cc --dispatcher for NFA-related functions
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libvata2.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <tuple>

// VATA headers
#include <vata2/nfa.hh>
#include <vata2/vm-dispatch.hh>

// local headers
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
		DEBUG_PRINT("calling function \"" + func_name + "\" for " + Vata2::Nfa::TYPE_NFA);

		if ("info" == func_name) {
			assert(func_args.size() == 0);
			std::string* new_str = new std::string;
			*new_str = "basic nondeterministic finite automaton";
			return VMValue(Vata2::TYPE_STR, new_str);
		}

		// we use throw to return result from test_and_call
		try {

			test_and_call("construct", func_name, {Vata2::TYPE_PARSEC}, func_args,
				Vata2::Nfa::TYPE_NFA,
				*[](const ParsedSection& parsec) -> auto {
					return static_cast<VMPointer>(new Nfa(construct(parsec)));
				});

		}
		catch (VMValue res)
		{
			return res;
		}

		return VMValue(Vata2::TYPE_NOT_A_VALUE, nullptr);
	}
}


void Vata2::Nfa::init()
{
	reg_dispatcher(Vata2::Nfa::TYPE_NFA, nfa_dispatch);
}
