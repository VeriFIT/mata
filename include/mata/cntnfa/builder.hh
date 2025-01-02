// TODO: Insert file header.

#ifndef LIBMATA_BUILDER_HH
#define LIBMATA_BUILDER_HH

#include "nfa.hh"

#include <filesystem>


/**
 * Namespace providing options to build NFAs.
 */
namespace mata::nfa::builder {

using namespace mata::nfa;

using NameStateMap = std::unordered_map<std::string, State>;

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
Nfa construct(const mata::parser::ParsedSection& parsec, Alphabet* alphabet, NameStateMap* state_map = nullptr);

/** Loads an automaton from Parsed object */
Nfa construct(const mata::IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map = nullptr);
/** Loads an automaton from Parsed object; version for python binding */
void construct(
    Nfa* result, const mata::IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map = nullptr
);

template<class ParsedObject>
Nfa construct(const ParsedObject& parsed, Alphabet* alphabet = nullptr,
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
 * @param nfa_stream Input stream containing NFA in mata format.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nfa parse_from_mata(std::istream& nfa_stream);

/**
 * Parse NFA from the mata format in a string.
 *
 * @param nfa_stream String containing NFA in mata format.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nfa parse_from_mata(const std::string& nfa_in_mata);

/**
 * Parse NFA from the mata format in a file.
 *
 * @param nfa_stream Path to the file containing NFA in mata format.
 * @throws std::runtime_error @p nfa_file does not exist.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nfa parse_from_mata(const std::filesystem::path& nfa_file);

} // namespace mata::nfa::builder.

#endif //LIBMATA_BUILDER_HH
