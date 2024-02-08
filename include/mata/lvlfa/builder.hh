// TODO: Insert file header.

#ifndef LIBMATA_LVLFA_BUILDER_HH
#define LIBMATA_LVLFA_BUILDER_HH

#include "lvlfa.hh"
#include <bits/stdc++.h>

#include "mata/nfa/builder.hh"

#include <filesystem>


/**
 * Namespace providing options to build NFAs.
 */
namespace mata::lvlfa::builder {

using namespace mata::lvlfa;

using NameStateMap = std::unordered_map<std::string, State>;

/**
 * Create an automaton accepting only a single @p word.
 */
Lvlfa create_single_word_lvlfa(const std::vector<Symbol>& word);

/**
 * Create an automaton accepting only a single @p word.
 *
 * @param word Word to accept.
 * @param alphabet Alphabet to use in NFA for translating word into symbols. If specified, the alphabet has to contain
 *  translations for all of the word symbols. If left empty, a new alphabet with only the symbols of the word will be
 *  created.
 */
Lvlfa create_single_word_lvlfa(const std::vector<std::string>& word, Alphabet* alphabet = nullptr);

/**
 * Create automaton accepting only epsilon string.
 */
Lvlfa create_empty_string_lvlfa();

/**
 * Create automaton accepting sigma star over the passed alphabet using DONT_CARE symbol.
 */
Lvlfa create_sigma_star_lvlfa();

/**
 * Create automaton accepting sigma star over the passed alphabet.
 *
 * @param[in] alphabet Alphabet to construct sigma star automaton with. When alphabet is left empty, the default empty
 *  alphabet is used, creating an automaton accepting only the empty string.
 */
Lvlfa create_sigma_star_lvlfa(Alphabet* alphabet = new OnTheFlyAlphabet{});

/** Loads an automaton from Parsed object */
// TODO this function should the same thing as the one taking IntermediateAut or be deleted
Lvlfa construct(const mata::parser::ParsedSection& parsec, Alphabet* alphabet, NameStateMap* state_map = nullptr);

/** Loads an automaton from Parsed object */
Lvlfa construct(const mata::IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map = nullptr);
/** Loads an automaton from Parsed object; version for python binding */
void construct(
    Lvlfa* result, const mata::IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map = nullptr
);

template<class ParsedObject>
Lvlfa construct(const ParsedObject& parsed, Alphabet* alphabet = nullptr,
              NameStateMap* state_map = nullptr) {
    OnTheFlyAlphabet tmp_alphabet{};
    if (!alphabet) {
        alphabet = &tmp_alphabet;
    }
    return construct(parsed, alphabet, state_map);
} // construct().

/**
 * Parse NFA from the mata format in an input stream.
 *
 * @param lvlfa_stream Input stream containing NFA in mata format.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Lvlfa parse_from_mata(std::istream& lvlfa_stream);

/**
 * Parse NFA from the mata format in a string.
 *
 * @param lvlfa_stream String containing NFA in mata format.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Lvlfa parse_from_mata(const std::string& lvlfa_in_mata);

/**
 * Parse NFA from the mata format in a file.
 *
 * @param lvlfa_stream Path to the file containing NFA in mata format.
 * @throws std::runtime_error @p lvlfa_file does not exist.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Lvlfa parse_from_mata(const std::filesystem::path& lvlfa_file);

} // namespace mata::lvlfa::builder.

#endif //LIBMATA_LVLFA_BUILDER_HH
