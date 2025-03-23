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

/**
 * Creates Tabakov-Vardi random NFA.
 * The implementation is based on the paper "Experimental Evaluation of Classical Automata Constructions" by Tabakov and Vardi.
 *
 * @param num_of_states Number of states in the automaton.
 * @param alphabet_size Size of the alphabet.
 * @param states_transitions_ratio_per_symbol Ratio between number of transitions and number of states for each symbol.
 *  The value must be in range [0, num_of_states]. A value of 1 means that there will be num_of_states transitions for each symbol.
 *  A value of num_of_states means that there will be a transition between every pair of states for each symbol.
 * @param final_state_density Density of final states in the automaton. The value must be in range [0, 1]. The state 0 is always final.
 *  If the density is 1, every state will be final.
 */
Nfa create_random_nfa_tabakov_vardi(const size_t num_of_states, const size_t alphabet_size, const double states_trans_ratio_per_symbol, const double final_state_density);

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

/**
 * @brief Create NFA from @p regex
 * 
 * The created NFA does not contain epsilons, is trimmed and reduced.
 * It uses the parser from RE2, see mata/parser/re2praser.hh for more
 * details and options.
 * 
 * At https://github.com/google/re2/wiki/Syntax, you can find the syntax
 * of @p regex with following futher limitations:
 *  1) The allowed characters are the first 256 characters of Unicode,
 *     i.e., Latin1 encoding (ASCII + 128 more characters). For the 
 *     full Unicode, check mata/parser/re2praser.hh.
 *  2) The created automaton represents the language of the regex and
 *     is not expected to be used in regex matching. Therefore, stuff
 *     like ^, $, \b, etc. are ignored in the regex. For example, the
 *     regular expressions a*b and ^a*b will both result in the same
 *     NFA accepting the language of multiple 'a's followed by one 'b'.
 *     See also issue #494.
 * 
 * @sa mata::parser::create_nfa()
 * 
 * @param regex regular expression
 */
Nfa create_from_regex(const std::string& regex);

} // namespace mata::nfa::builder.

#endif //LIBMATA_BUILDER_HH
