// TODO: Insert file header.

#ifndef LIBMATA_NFT_BUILDER_HH
#define LIBMATA_NFT_BUILDER_HH

#include "mata/nfa/builder.hh"
#include "nft.hh"

#include <optional>
#include <filesystem>

/**
 * Namespace providing options to build NFAs.
 */
namespace mata::nft::builder {

using namespace mata::nft;

using NameStateMap = std::unordered_map<std::string, State>;

/**
 * Create an automaton accepting only a single @p word.
 */
Nft create_single_word_nft(const Word& word);

/**
 * Create an automaton accepting only a single @p word.
 *
 * @param word Word to accept.
 * @param alphabet Alphabet to use in NFA for translating word into symbols. If specified, the alphabet has to contain
 *  translations for all the word symbols. If left empty, a new alphabet with only the symbols of the word will be
 *  created.
 */
Nft create_single_word_nft(const WordName& word, Alphabet* alphabet = nullptr);

/**
 * Create automaton accepting only epsilon string.
 */
Nft create_empty_string_nft(size_t num_of_levels = DEFAULT_NUM_OF_LEVELS);

/**
 * Create automaton accepting sigma star over the passed alphabet using DONT_CARE symbol.
 *
 * @param[in] num_of_levels Number of levels in the created NFT.
 */
Nft create_sigma_star_nft(size_t num_of_levels = DEFAULT_NUM_OF_LEVELS);

/**
 * Create automaton accepting sigma star over the passed alphabet.
 *
 * @param[in] alphabet Alphabet to construct sigma star automaton with. When alphabet is left empty, the default empty
 *  alphabet is used, creating an automaton accepting only the empty string.
 * @param[in] num_of_levels Number of levels in the created NFT.
 */
Nft create_sigma_star_nft(const Alphabet* alphabet = new OnTheFlyAlphabet{}, size_t num_of_levels = DEFAULT_NUM_OF_LEVELS);

/** Loads an automaton from Parsed object */
// TODO this function should the same thing as the one taking IntermediateAut or be deleted
Nft construct(const parser::ParsedSection& parsec, Alphabet* alphabet, NameStateMap* state_map = nullptr);

/** Loads an automaton from Parsed object */
Nft construct(const IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map = nullptr);
/** Loads an automaton from Parsed object; version for python binding */
void construct(
    Nft* result, const IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map = nullptr
);

template<class ParsedObject>
Nft construct(const ParsedObject& parsed, Alphabet* alphabet = nullptr,
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
 * @param nft_stream Input stream containing NFA in mata format.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nft parse_from_mata(std::istream& nft_stream);

/**
 * Parse NFA from the mata format in a string.
 *
 * @param nft_in_mata String containing NFA in mata format.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nft parse_from_mata(const std::string& nft_in_mata);

/**
 * Parse NFA from the mata format in a file.
 *
 * @param nft_file Path to the file containing NFA in mata format.
 * @throws std::runtime_error @p nft_file does not exist.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nft parse_from_mata(const std::filesystem::path& nft_file);

/**
 * Create NFT from NFA.
 * @param nfa_state NFA to create NFT from.
 * @param num_of_levels Number of levels of NFT.
 * @param next_levels_symbol Symbol to use on non-NFA levels. std::nullopt means using the same symbol on all levels.
 * @param epsilons Which symbols handle as epsilons.
 * @return NFT representing @p nfa_state with @p num_of_levels number of levels.
 */
Nft from_nfa_with_zero_levels(const nfa::Nfa& nfa_state, size_t num_of_levels = DEFAULT_NUM_OF_LEVELS, bool explicit_transitions = true, std::optional<Symbol> next_levels_symbol = {});

} // namespace mata::nft::builder.

#endif //LIBMATA_NFT_BUILDER_HH
