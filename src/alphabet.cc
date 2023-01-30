/* alphabet.cc -- File containing alphabets for automata
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

#include <mata/alphabet.hh>

using Mata::Symbol;
using Mata::OnTheFlyAlphabet;

Mata::Util::OrdVector<Symbol> OnTheFlyAlphabet::get_alphabet_symbols() const {
    Util::OrdVector<Symbol> result;
    for (const auto& str_sym_pair : symbol_map) {
        result.insert(str_sym_pair.second);
    }
    return result;
} // OnTheFlyAlphabet::get_alphabet_symbols.

std::list<Symbol> OnTheFlyAlphabet::get_complement(const std::set<Symbol>& syms) const {
    std::list<Symbol> result;
    // TODO: Could be optimized.
    std::set<Symbol> symbols_alphabet;
    for (const auto& str_sym_pair : symbol_map) {
        symbols_alphabet.insert(str_sym_pair.second);
    }

    std::set_difference(
            symbols_alphabet.begin(), symbols_alphabet.end(),
            syms.begin(), syms.end(),
            std::inserter(result, result.end()));
    return result;
} // OnTheFlyAlphabet::get_complement.


void OnTheFlyAlphabet::add_symbols_from(const StringToSymbolMap& new_symbol_map) {
    for (const auto& symbol_binding: new_symbol_map) {
        update_next_symbol_value(symbol_binding.second);
        try_add_new_symbol(symbol_binding.first, symbol_binding.second);
    }
}

std::ostream &std::operator<<(std::ostream &os, const Mata::Alphabet& alphabet) {
    return os << std::to_string(alphabet);
}
