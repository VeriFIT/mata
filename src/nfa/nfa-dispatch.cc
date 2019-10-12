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
#include "../dispatch-aux.hh"

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
		DEBUG_PRINT("calling function \"" + func_name + "\" for " +
			Vata2::Nfa::TYPE_NFA + " with arguments " + std::to_string(func_args));

		const VMValue& arg0 = func_args[0];
		if (TYPE_NFA == arg0.type) {
			const NfaWrapper& wrap = *static_cast<const NfaWrapper*>(arg0.get_ptr());
			DEBUG_VM_LOW_PRINT("NFA: " + std::to_string(wrap.nfa));
			DEBUG_VM_LOW_PRINT("alphabet: " + std::to_string(wrap.alphabet));
		}

		// we use throw to return result from test_and_call
		try {

			test_and_call("construct", func_name, {Vata2::TYPE_PARSEC}, func_args,
				Vata2::Nfa::TYPE_NFA,
				*[](const ParsedSection& parsec) -> auto {
					NfaWrapper* nfa_wrap = new NfaWrapper;
					DEBUG_PRINT("constructing NFA " + (parsec.haskey("Name")?
							std::to_string(parsec["Name"]) :
							"[unnamed]"));

					// choosing the alphabet to use
					if (parsec.haskey("CharAlphabet")) {
						DEBUG_PRINT("using CharAlphabet");
						nfa_wrap->alphabet = new CharAlphabet();
					} else if (parsec.haskey("DirectAlphabet")) {
						DEBUG_PRINT("using DirectAlphabet");
						nfa_wrap->alphabet = new DirectAlphabet();
					} else if (parsec.haskey("EnumAlphabet")) {
						DEBUG_PRINT("using EnumAlphabet");
						// TODO: load the alphabet
						assert(false);
						nfa_wrap->alphabet = new EnumAlphabet();
					} else { // default
						DEBUG_PRINT("using OnTheFlyAlphabet");

						// TODO: fix resource leak
						StringToSymbolMap* sym_map = new StringToSymbolMap();
						nfa_wrap->alphabet = new OnTheFlyAlphabet(sym_map);
					}

					// TODO: do sth with state_dict!!!!
					construct(&nfa_wrap->nfa, parsec, nfa_wrap->alphabet, &nfa_wrap->state_dict);
					return static_cast<VMPointer>(nfa_wrap);
				});

			test_and_call("print", func_name, {TYPE_NFA}, func_args, Vata2::TYPE_VOID,
				*[](const NfaWrapper& nfa_wrap) -> auto {
					std::cout << nfa_wrap;
					return static_cast<VMPointer>(nullptr);
				});

			test_and_call("is_univ", func_name, {Vata2::Nfa::TYPE_NFA}, func_args,
				Vata2::TYPE_BOOL,
				*[](const NfaWrapper& nfa_wrap) -> auto {
					Word cex;
					// TODO: FIX
					StringDict params{{"algo", "naive"}};
					bool* result = new bool;
					*result = is_universal(nfa_wrap.nfa, *nfa_wrap.alphabet, &cex, params);
					return static_cast<VMPointer>(result);
				});
		}
		catch (VMValue res) {
			return res;
		}

		return VMValue(Vata2::TYPE_NOT_A_VALUE, nullptr);
	}
}


void Vata2::Nfa::init()
{
	reg_dispatcher(Vata2::Nfa::TYPE_NFA, nfa_dispatch,
		"basic nondeterministic finite automaton");
}
