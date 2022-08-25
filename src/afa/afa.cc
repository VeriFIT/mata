/* afa.cc -- operations for AFA
 *
 * Copyright (c) TODO TODO
 *
 * This file is a part of libmata.
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

// MATA headers
#include <mata/afa.hh>
#include <mata/util.hh>

using std::tie;

using namespace Mata::util;
using namespace Mata::Afa;
using Mata::Afa::Symbol;

const std::string Mata::Afa::TYPE_AFA = "AFA";

std::ostream& std::operator<<(std::ostream& os, const Mata::Afa::Trans& trans)
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


std::ostream& Mata::Afa::operator<<(std::ostream& os, const Afa& afa)
{ // {{{
	return os << std::to_string(serialize(afa));
} // Nfa::operator<<(ostream) }}}


bool Mata::Afa::are_state_disjoint(const Afa& lhs, const Afa& rhs)
{ // {{{
  assert(&lhs);
  assert(&rhs);

  // TODO
  assert(false);
} // are_disjoint }}}


void Mata::Afa::union_norename(
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


Afa Mata::Afa::union_rename(
	const Afa&  lhs,
	const Afa&  rhs)
{ // {{{
  assert(&lhs);
  assert(&rhs);

  // TODO
  assert(false);
} // union_rename }}}


bool Mata::Afa::is_lang_empty(const Afa& aut, Path* cex)
{ // {{{
  assert(&aut);
  assert(&cex);

  // TODO
  assert(false);
} // is_lang_empty }}}


bool Mata::Afa::is_lang_empty_cex(const Afa& aut, Word* cex)
{ // {{{
	assert(nullptr != cex);

  assert(&aut);
  assert(&cex);

  // TODO
  assert(false);
} // is_lang_empty_cex }}}


void Mata::Afa::make_complete(
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


Mata::Parser::ParsedSection Mata::Afa::serialize(
	const Afa&                aut,
	const SymbolToStringMap*  symbol_map,
	const StateToStringMap*   state_map)
{ // {{{
	Mata::Parser::ParsedSection parsec;
	parsec.type = Mata::Afa::TYPE_AFA;

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


void Mata::Afa::revert(Afa* result, const Afa& aut)
{ // {{{
	assert(nullptr != result);

  assert(&aut);

  // TODO
  assert(false);
} // revert }}}


void Mata::Afa::remove_epsilon(Afa* result, const Afa& aut, Symbol epsilon)
{ // {{{
	assert(nullptr != result);

  assert(&aut);
  assert(&epsilon);

  // TODO
  assert(false);
} // remove_epsilon }}}


void Mata::Afa::minimize(
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


void Mata::Afa::construct(
	Afa*                                 aut,
	const Mata::Parser::ParsedSection&  parsec,
	Alphabet*                            alphabet,
	StringToStateMap*                    state_map)
{ // {{{
	assert(nullptr != aut);
	assert(nullptr != alphabet);

	if (parsec.type != Mata::Afa::TYPE_AFA) {
		throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
                                 Mata::Afa::TYPE_AFA + "\"");
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
		if (body_line.size() < 2) {
			// clean up
      clean_up();

      throw std::runtime_error("Invalid transition: " +
        std::to_string(body_line));
		}

		State src_state = get_state_name(body_line[0]);
    std::string formula;
    for (size_t i = 1; i < body_line.size(); ++i) {
      formula += body_line[i] + " ";
    }

    aut->add_trans(src_state, formula);
	}

	// do the dishes and take out garbage
	clean_up();
} // construct }}}


void Mata::Afa::construct(
	Afa*                                 aut,
	const Mata::Parser::ParsedSection&  parsec,
	StringToSymbolMap*                   symbol_map,
	StringToStateMap*                    state_map)
{ // {{{
	assert(nullptr != aut);

	bool remove_symbol_map = false;
	if (nullptr == symbol_map)
	{
		symbol_map = new StringToSymbolMap();
		remove_symbol_map = true;
	}

	auto release_res = [&](){ if (remove_symbol_map) delete symbol_map; };

  Mata::Nfa::OnTheFlyAlphabet alphabet(symbol_map);

	try
	{
		Mata::Afa::construct(aut, parsec, &alphabet, state_map);
	}
	catch (std::exception&)
	{
		release_res();
		throw;
	}

	release_res();
} // construct(StringToSymbolMap) }}}


bool Mata::Afa::is_in_lang(const Afa& aut, const Word& word)
{ // {{{
  assert(&aut);
  assert(&word);

  // TODO
  assert(false);
} // is_in_lang }}}


bool Mata::Afa::is_prfx_in_lang(const Afa& aut, const Word& word)
{ // {{{
  assert(&aut);
  assert(&word);

  // TODO
  assert(false);
} // is_prfx_in_lang }}}


bool Mata::Afa::is_deterministic(const Afa& aut)
{ // {{{
  assert(&aut);

  // TODO
  assert(false);
} // is_deterministic }}}


bool Mata::Afa::is_complete(const Afa& aut, const Alphabet& alphabet)
{ // {{{
  assert(&aut);
  assert(&alphabet);

  // TODO
  assert(false);
} // is_complete }}}

bool Mata::Afa::accepts_epsilon(const Afa& aut)
{ // {{{
	for (State st : aut.initialstates) {
		if (haskey(aut.finalstates, st)) return true;
	}

	return false;
} // accepts_epsilon }}}

std::ostream& std::operator<<(std::ostream& os, const Mata::Afa::AfaWrapper& afa_wrap)
{ // {{{
	os << "{AFA wrapper|AFA: " << afa_wrap.afa << "|alphabet: " << afa_wrap.alphabet <<
		"|state_dict: " << std::to_string(afa_wrap.state_dict) << "}";
	return os;
} // operator<<(AfaWrapper) }}}

