// Helpers shared by the NFA and NFT parts of the shim. Not part of the bridge.
#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/automaton.hh"
#include "mata/nft/types.hh"
#include "mata/parser/inter-aut.hh"
#include "rust/cxx.h"

namespace mata_sys::detail {

using mata::Symbol;
using mata::Word;
using mata::nfa::State;
using SymbolSet = mata::utils::OrdVector<Symbol>;

/// Views `size` elements at `data` as a Rust slice (whose pointer must not be null, even when empty).
template <class T> rust::Slice<const T> slice_of(const T* data, const std::size_t size) {
	static constexpr auto empty_slot = T();
	return rust::Slice<const T>(size == 0 ? &empty_slot : data, size);
}

template <class T> rust::Slice<const T> slice_of(const std::vector<T>& values) {
	return slice_of(values.data(), values.size());
}

inline Word to_word(const rust::Slice<const Symbol> word) { return Word(word.begin(), word.end()); }

/// Throws on EPSILON and DONT_CARE, which read as moves, not letters, in every Mata call that takes
/// a symbol set (complement, difference, make_complete, is_complete, sigma_star_nfa, to_nfa).
inline SymbolSet to_symbol_set(const rust::Slice<const Symbol> symbols) {
	if (std::ranges::any_of(symbols, [](const Symbol symbol) {
			return symbol == mata::nft::EPSILON || symbol == mata::nft::DONT_CARE;
		})) {
		throw std::invalid_argument("a symbol set cannot contain epsilon or the don't-care symbol");
	}
	return SymbolSet(std::vector<Symbol>(symbols.begin(), symbols.end()));
}

template <class T, class Range> void assign(rust::Vec<T>& out, const Range& values) {
	out.clear();
	out.reserve(std::size(values));
	for (const auto value : values) { out.push_back(value); }
}

/// Writes `words` as the bridge returns them: all symbols concatenated into `symbols`, and the length of each word
///  into `lengths`.
template <class Words>
void assign_words(rust::Vec<Symbol>& symbols, rust::Vec<std::size_t>& lengths, const Words& words) {
	symbols.clear();
	lengths.clear();
	lengths.reserve(std::size(words));
	for (const Word& word : words) {
		for (const Symbol symbol : word) { symbols.push_back(symbol); }
		lengths.push_back(word.size());
	}
}

/// Throws unless `state` is a state of `automaton` (Mata would grow the automaton instead).
inline void check_state(const mata::Automaton& automaton, const State state) {
	if (state >= automaton.num_of_states()) {
		throw std::out_of_range(
			"state " + std::to_string(state) + " does not exist (the automaton has " +
			std::to_string(automaton.num_of_states()) + " states)"
		);
	}
}

/// Throws unless the transitions are kept as Mata keeps them: sorted by symbol, with sorted, unique targets
/// that are states of `automaton`.
void check_delta(const mata::Automaton& automaton);

/// A Mata alphabet with the names of a Rust `Alphabet`, for printing.
class NameTable final : public mata::Alphabet {
  public:
	NameTable(rust::Slice<const rust::String> names, rust::Slice<const Symbol> symbols);

	Symbol translate_symb(const std::string& name) override;
	std::string reverse_translate_symbol(Symbol symbol) const override;
	SymbolSet get_alphabet_symbols() const override;
	bool empty() const override { return by_symbol_.empty(); }

  private:
	std::unordered_map<Symbol, std::string> by_symbol_ = {};
	std::unordered_map<std::string, Symbol> by_name_ = {};
};

/// Parses the automaton sections of a `.mata` text, mintermizing the bit-vector sections together.
std::vector<mata::IntermediateAut> parse_sections(rust::Str text);

/// The parser's alphabet: integers, or the names of a Rust `Alphabet`, extended on the fly.
class ParseAlphabet {
  public:
	ParseAlphabet(
		bool numeric_symbols,
		rust::Slice<const rust::String> names,
		rust::Slice<const Symbol> named_symbols,
		Symbol next_symbol
	);

	mata::Alphabet* get() { return numeric_ ? static_cast<mata::Alphabet*>(&integers_) : &on_the_fly_; }

	/// Writes the names that parsing added, ordered by symbol. Throws if no symbol below `DONT_CARE` was left.
	void report_new_names(rust::Vec<rust::String>& new_names, rust::Vec<Symbol>& new_symbols) const;

  private:
	bool numeric_;
	Symbol first_new_symbol_;
	mata::IntAlphabet integers_ = {};
	mata::OnTheFlyAlphabet on_the_fly_;
	std::unordered_set<std::string> known_names_ = {};
};

} // namespace mata_sys::detail
