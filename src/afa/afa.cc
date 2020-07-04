/* afa.cc -- operations for AFA
 *
 * Copyright (c) TODO TODO
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

#include <algorithm>
#include <list>
#include <unordered_set>

// VATA headers
#include <vata2/afa.hh>
#include <vata2/util.hh>
#include <vata2/vm-dispatch.hh>

using std::tie;

using namespace Vata2::util;
using namespace Vata2::Afa;
using Vata2::Afa::Symbol;

const std::string Vata2::Afa::TYPE_AFA = "AFA";

std::ostream& std::operator<<(std::ostream& os, const Vata2::Afa::Trans& trans)
{ // {{{
	std::string result = "(" + std::to_string(trans.src) + ", " + trans.formula + ")";
	return os << result;
} // operator<<(ostream, Trans) }}}


void Afa::add_trans(const Trans& trans)
{ // {{{
	this->transitions.push_back(trans);
} // add_trans }}}

bool Afa::has_trans(const Trans& trans) const
{ // {{{
  return std::find(this->transitions.begin(),
                   this->transitions.end(),
                   trans) != this->transitions.end();
} // has_trans }}}


size_t Afa::trans_size() const
{ // {{{
	return this->transitions.size();
} // trans_size() }}}


std::ostream& Vata2::Afa::operator<<(std::ostream& os, const Afa& afa)
{ // {{{
	return os << std::to_string(serialize(afa));
} // Nfa::operator<<(ostream) }}}


bool Vata2::Afa::are_state_disjoint(const Afa& lhs, const Afa& rhs)
{ // {{{
  assert(&lhs);
  assert(&rhs);

  // TODO
  assert(false);
} // are_disjoint }}}


void Vata2::Afa::union_norename(
	Afa*        result,
	const Afa&  lhs,
	const Afa&  rhs)
{ // {{{
	assert(nullptr != result);

  assert(&lhs);
  assert(&rhs);

  // TODO
  assert(false);
} // union_norename }}}


Afa Vata2::Afa::union_rename(
	const Afa&  lhs,
	const Afa&  rhs)
{ // {{{
  assert(&lhs);
  assert(&rhs);

  // TODO
  assert(false);
} // union_rename }}}


bool Vata2::Afa::is_lang_empty(const Afa& aut, Path* cex)
{ // {{{
  assert(&aut);
  assert(&cex);

  // TODO
  assert(false);
} // is_lang_empty }}}


bool Vata2::Afa::is_lang_empty_cex(const Afa& aut, Word* cex)
{ // {{{
	assert(nullptr != cex);

  assert(&aut);
  assert(&cex);

  // TODO
  assert(false);
} // is_lang_empty_cex }}}


void Vata2::Afa::make_complete(
	Afa*             aut,
	const Alphabet&  alphabet,
	State            sink_state)
{ // {{{
	assert(nullptr != aut);

  assert(&alphabet);
  assert(&sink_state);

  // TODO
  assert(false);
} // make_complete }}}


Vata2::Parser::ParsedSection Vata2::Afa::serialize(
	const Afa&                aut,
	const SymbolToStringMap*  symbol_map,
	const StateToStringMap*   state_map)
{ // {{{
	Vata2::Parser::ParsedSection parsec;
	parsec.type = Vata2::Afa::TYPE_AFA;

	using bool_str_pair = std::pair<bool, std::string>;

	std::function<bool_str_pair(State)> state_namer = nullptr;
	if (nullptr == state_map)
	{
		state_namer = [](State st) -> auto {
			return bool_str_pair(true, "q" + std::to_string(st));
		};
	}
	else
	{
		state_namer = [&state_map](State st) -> auto {
			auto it = state_map->find(st);
			if (it != state_map->end()) { return bool_str_pair(true, it->second); }
			else { return bool_str_pair(false, ""); }
		};
	}

	std::function<bool_str_pair(Symbol)> symbol_namer = nullptr;
	if (nullptr == symbol_map) {
		symbol_namer = [](Symbol sym) -> auto {
			return bool_str_pair(true, "a" + std::to_string(sym));
		};
	} else {
		symbol_namer = [&symbol_map](Symbol sym) -> auto {
			auto it = symbol_map->find(sym);
			if (it != symbol_map->end()) { return bool_str_pair(true, it->second); }
			else { return bool_str_pair(false, ""); }
		};
	}

	// construct initial states
	std::vector<std::string> init_states;
	for (State s : aut.initialstates) {
		bool_str_pair bsp = state_namer(s);
		if (!bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(s)); }
		init_states.push_back(bsp.second);
	}
	parsec.dict["Initial"] = init_states;

	// construct final states
	std::vector<std::string> fin_states;
	for (State s : aut.finalstates) {
		bool_str_pair bsp = state_namer(s);
		if (!bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(s)); }
		fin_states.push_back(bsp.second);
	}
	parsec.dict["Final"] = fin_states;

	for (const auto& trans : aut.transitions) {
		bool_str_pair src_bsp = state_namer(trans.src);
		if (!src_bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(trans.src)); }

		parsec.body.push_back({ src_bsp.second, trans.formula });
	}

	return parsec;
} // serialize }}}


void Vata2::Afa::revert(Afa* result, const Afa& aut)
{ // {{{
	assert(nullptr != result);

  assert(&aut);

  // TODO
  assert(false);
} // revert }}}


void Vata2::Afa::remove_epsilon(Afa* result, const Afa& aut, Symbol epsilon)
{ // {{{
	assert(nullptr != result);

  assert(&aut);
  assert(&epsilon);

  // TODO
  assert(false);
} // remove_epsilon }}}


void Vata2::Afa::minimize(
	Afa*               result,
	const Afa&         aut,
	const StringDict&  params)
{ // {{{
	assert(nullptr != result);

  assert(&aut);
	assert(&params);

  // TODO
  assert(false);
} // minimize }}}


void Vata2::Afa::construct(
	Afa*                                 aut,
	const Vata2::Parser::ParsedSection&  parsec,
	Alphabet*                            alphabet,
	StringToStateMap*                    state_map)
{ // {{{
	assert(nullptr != aut);
	assert(nullptr != alphabet);

	if (parsec.type != Vata2::Afa::TYPE_AFA) {
		throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
			Vata2::Afa::TYPE_AFA + "\"");
	}

	bool remove_state_map = false;
	if (nullptr == state_map) {
		state_map = new StringToStateMap();
		remove_state_map = true;
	}

	State cnt_state = 0;

	// a lambda for translating state names to identifiers
	auto get_state_name = [state_map, &cnt_state](const std::string& str) {
		auto it_insert_pair = state_map->insert({str, cnt_state});
		if (it_insert_pair.second) { return cnt_state++; }
		else { return it_insert_pair.first->second; }
	};

	// a lambda for cleanup
	auto clean_up = [&]() {
		if (remove_state_map) { delete state_map; }
	};


	auto it = parsec.dict.find("Initial");
	if (parsec.dict.end() != it) {
		for (const auto& str : it->second) {
			State state = get_state_name(str);
			aut->initialstates.insert(state);
		}
	}


	it = parsec.dict.find("Final");
	if (parsec.dict.end() != it) {
		for (const auto& str : it->second) {
			State state = get_state_name(str);
			aut->finalstates.insert(state);
		}
	}

	for (const auto& body_line : parsec.body) {
		if (body_line.size() != 3) {
			// clean up
			clean_up();

			if (body_line.size() == 2) {
				throw std::runtime_error("Epsilon transitions not supported: " +
					std::to_string(body_line));
			} else {
				throw std::runtime_error("Invalid transition: " +
					std::to_string(body_line));
			}
		}

		State src_state = get_state_name(body_line[0]);
		Symbol symbol = alphabet->translate_symb(body_line[1]);
		State tgt_state = get_state_name(body_line[2]);

    assert(&src_state);
    assert(&symbol);
    assert(&tgt_state);

    // TODO
		// aut->add_trans(src_state, symbol, tgt_state);
    assert(false);
	}

	// do the dishes and take out garbage
	clean_up();
} // construct }}}


void Vata2::Afa::construct(
	Afa*                                 aut,
	const Vata2::Parser::ParsedSection&  parsec,
	StringToSymbolMap*                   symbol_map,
	StringToStateMap*                    state_map)
{ // {{{
	assert(nullptr != aut);

  assert(&parsec);
  assert(&symbol_map);
  assert(&state_map);

  // TODO
  assert(false);
} // construct(StringToSymbolMap) }}}


bool Vata2::Afa::is_in_lang(const Afa& aut, const Word& word)
{ // {{{
  assert(&aut);
  assert(&word);

  // TODO
  assert(false);
} // is_in_lang }}}


bool Vata2::Afa::is_prfx_in_lang(const Afa& aut, const Word& word)
{ // {{{
  assert(&aut);
  assert(&word);

  // TODO
  assert(false);
} // is_prfx_in_lang }}}


bool Vata2::Afa::is_deterministic(const Afa& aut)
{ // {{{
  assert(&aut);

  // TODO
  assert(false);
} // is_deterministic }}}


bool Vata2::Afa::is_complete(const Afa& aut, const Alphabet& alphabet)
{ // {{{
  assert(&aut);
  assert(&alphabet);

  // TODO
  assert(false);
} // is_complete }}}

bool Vata2::Afa::accepts_epsilon(const Afa& aut)
{ // {{{
	for (State st : aut.initialstates) {
		if (haskey(aut.finalstates, st)) return true;
	}

	return false;
} // accepts_epsilon }}}

std::ostream& std::operator<<(std::ostream& os, const Vata2::Afa::AfaWrapper& afa_wrap)
{ // {{{
	os << "{AFA wrapper|AFA: " << afa_wrap.afa << "|alphabet: " << afa_wrap.alphabet <<
		"|state_dict: " << std::to_string(afa_wrap.state_dict) << "}";
	return os;
} // operator<<(AfaWrapper) }}}

