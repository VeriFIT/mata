// TODO: Insert file header.

#ifndef LIBMATA_BUILDER_HH
#define LIBMATA_BUILDER_HH

#include "nfa.hh"

using namespace Mata::Nfa;

/**
 * Namespace providing options to build NFAs.
 */
namespace Mata::Nfa::Builder {

/**
 * Create an automaton accepting only a single @p word.
 */
Nfa create_single_word_nfa(const std::vector<Symbol>& word);

/**
 * Create an automaton accepting only a single @p word.
 *
 * @param word Word to accept.
 * @param alphabet Alphabet to use in NFA for translating word into symbols. If specified, the alphabet has to contain
 *  translations for all of the word symbols. If left empty, a new alphabet with only the symbols of the word will be
 *  created.
 */
Nfa create_single_word_nfa(const std::vector<std::string>& word, Alphabet* alphabet = nullptr);

/**
 * Create automaton accepting only epsilon string.
 */
Nfa create_empty_string_nfa();

/**
 * Create automaton accepting sigma star over the passed alphabet.
 *
 * @param[in] alphabet Alphabet to construct sigma star automaton with. When alphabet is left empty, the default empty
 *  alphabet is used, creating an automaton accepting only the empty string.
 */
Nfa create_sigma_star_nfa(Alphabet* alphabet = new OnTheFlyAlphabet{});

/** Loads an automaton from Parsed object */
// TODO this function should the same thing as the one taking IntermediateAut or be deleted
Nfa construct(const Mata::Parser::ParsedSection& parsec, Alphabet* alphabet, StringToStateMap* state_map = nullptr);

/** Loads an automaton from Parsed object */
Nfa construct(const Mata::IntermediateAut& inter_aut, Alphabet* alphabet, StringToStateMap* state_map = nullptr);

template<class ParsedObject>
Nfa construct(const ParsedObject& parsed, Mata::StringToSymbolMap* symbol_map = nullptr,
              StringToStateMap* state_map = nullptr) {
    Mata::StringToSymbolMap tmp_symbol_map;
    if (symbol_map) {
        tmp_symbol_map = *symbol_map;
    }
    Mata::OnTheFlyAlphabet alphabet(tmp_symbol_map);

    Nfa aut = construct(parsed, &alphabet, state_map);

    if (symbol_map) {
        *symbol_map = alphabet.get_symbol_map();
    }
    return aut;
} // construct().


} // namespace Mata::Nfa::Builder.

#endif //LIBMATA_BUILDER_HH
