// TODO: Insert file header.

#ifndef LIBMATA_NFT_BUILDER_HH
#define LIBMATA_NFT_BUILDER_HH

#include "mata/nfa/builder.hh"
#include "nft.hh"

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
Nft create_single_word_nft(const std::vector<Symbol>& word);

/**
 * Create an automaton accepting only a single @p word.
 *
 * @param word Word to accept.
 * @param alphabet Alphabet to use in NFA for translating word into symbols. If specified, the alphabet has to contain
 *  translations for all of the word symbols. If left empty, a new alphabet with only the symbols of the word will be
 *  created.
 */
Nft create_single_word_nft(const std::vector<std::string>& word, Alphabet* alphabet = nullptr);

/**
 * Create automaton accepting only epsilon string.
 */
Nft create_empty_string_nft();

/**
 * Create automaton accepting sigma star over the passed alphabet using DONT_CARE symbol.
 */
Nft create_sigma_star_nft();

/**
 * Create automaton accepting sigma star over the passed alphabet.
 *
 * @param[in] alphabet Alphabet to construct sigma star automaton with. When alphabet is left empty, the default empty
 *  alphabet is used, creating an automaton accepting only the empty string.
 */
Nft create_sigma_star_nft(Alphabet* alphabet = new OnTheFlyAlphabet{});

/** Loads an automaton from Parsed object */
// TODO this function should the same thing as the one taking IntermediateAut or be deleted
Nft construct(const mata::parser::ParsedSection& parsec, Alphabet* alphabet, NameStateMap* state_map = nullptr);

/** Loads an automaton from Parsed object */
Nft construct(const mata::IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map = nullptr);
/** Loads an automaton from Parsed object; version for python binding */
void construct(
    Nft* result, const mata::IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map = nullptr
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
 * @param nft_stream String containing NFA in mata format.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nft parse_from_mata(const std::string& nft_in_mata);

/**
 * Parse NFA from the mata format in a file.
 *
 * @param nft_stream Path to the file containing NFA in mata format.
 * @throws std::runtime_error @p nft_file does not exist.
 * @throws std::runtime_error Parsing of NFA fails.
 */
Nft parse_from_mata(const std::filesystem::path& nft_file);

/**
 * Create NFT from NFA.
 * @param nfa_state NFA to create NFT from.
 * @param level_cnt Number of levels of NFT.
 * @param epsilons Which symbols handle as epsilons.
 * @return NFT representing @p nfa_state with @p level_cnt number of levels.
 */
Nft create_from_nfa(const mata::nfa::Nfa& nfa_state, Level level_cnt = 2, const std::set<Symbol>& epsilons = { EPSILON });

} // namespace mata::nft::builder.

#endif //LIBMATA_NFT_BUILDER_HH
