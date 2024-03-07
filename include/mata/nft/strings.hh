/* nfa-strings.hh -- Operations on NFAs for string solving.
 */

#ifndef MATA_NFT_STRING_SOLVING_HH_
#define MATA_NFT_STRING_SOLVING_HH_

#include "mata/nfa/strings.hh"
#include "nft.hh"

namespace mata::nft::strings {

constexpr Symbol BEGIN_MARKER{ EPSILON - 100 }; ///< Marker marking the beginning of the regex to be replaced.
constexpr Symbol END_MARKER{ EPSILON - 99 }; ///< Marker marking the end of the regex to be replaced.

/**
 * How many occurrences of the regex to replace, in order from left to right?
 */
enum class ReplaceMode {
    Single, ///< Replace only the first occurrence of the regex.
    All, ///< Replace all occurrences of the regex.
};

/**
 * Create identity transducer over the @p alphabet with @p num_of_levels levels.
 */
Nft create_identity(mata::Alphabet* alphabet, size_t num_of_levels = 2);

/**
 * Create identity input/output transducer with 2 levels over the @p alphabet with @p level_cnt levels with single
 *  symbol @p from_symbol replaced with @to_symbol.
 */
Nft create_identity_with_single_symbol_replace(mata::Alphabet* alphabet, Symbol from_symbol, Symbol replacement,
                                               ReplaceMode replace_mode = ReplaceMode::All);

/**
 * Create identity input/output transducer with 2 levels over the @p alphabet with @p level_cnt levels with single
 *  symbol @p from_symbol replaced with word @p replacement.
 */
Nft create_identity_with_single_symbol_replace(mata::Alphabet* alphabet, Symbol from_symbol, const Word& replacement,
                                               ReplaceMode replace_mode = ReplaceMode::All);

/**
 * @brief Create NFT modelling a reluctant leftmost replace of regex @p regex to @p replacement.
 *
 * The most general replace operation, handling any regex as the part to be replaced.
 * @param regex A string containing regex to be replaced.
 * @param replacement Literal to be replaced with.
 * @param alphabet Alphabet over which to create the NFT.
 * @param replace_mode Whether to replace all or just the single (the leftmost) occurrence of @p regex.
 * @param begin_marker Symbol to be used internally as a begin marker of replaced @p regex.
 * @return The reluctant leftmost replace NFT.
 */
Nft replace_reluctant_regex(const std::string& regex, const Word& replacement, Alphabet* alphabet,
                            ReplaceMode replace_mode = ReplaceMode::All, Symbol begin_marker = BEGIN_MARKER);

/**
 * @brief Create NFT modelling a reluctant leftmost replace of regex @p regex to @p replacement.
 *
 * The most general replace operation, handling any regex as the part to be replaced.
 * @param regex NFA representing regex to be replaced.
 * @param replacement Literal to replace with.
 * @param alphabet Alphabet over which to create the NFT.
 * @param replace_mode Whether to replace all or just the single (the leftmost) occurrence of @p regex.
 * @param begin_marker Symbol to be used internally as a begin marker of replaced @p regex.
 * @return The reluctant leftmost replace NFT.
 */
Nft replace_reluctant_regex(nfa::Nfa regex, const Word& replacement, Alphabet* alphabet,
                            ReplaceMode replace_mode = ReplaceMode::All, Symbol begin_marker = BEGIN_MARKER);

/**
 * Create NFT modelling a reluctant leftmost replace of literal @p literal to @p replacement.
 * @param literal Literal to replace.
 * @param replacement Literal to replace with.
 * @param alphabet Alphabet over which to create the NFT.
 * @param replace_mode Whether to replace all or just the single (the leftmost) occurrence of @p literal.
 * @param end_marker Symbol to be used internally as an end marker marking the end of the replaced literal.
 * @return The reluctant leftmost replace NFT.
 */
Nft replace_reluctant_literal(const Word& literal, const Word& replacement, Alphabet* alphabet,
                              ReplaceMode replace_mode = ReplaceMode::All, Symbol end_marker = END_MARKER);

/**
 * Create NFT modelling a reluctant leftmost replace of symbol @p from_symbol to @p replacement.
 * @param from_symbol Symbol to replace.
 * @param replacement Symbol to replace with.
 * @param alphabet Alphabet over which to create the NFT.
 * @param replace_mode Whether to replace all or just the single (the leftmost) occurrence of @p from_symbol.
 * @return The reluctant leftmost replace NFT.
 */
Nft replace_reluctant_single_symbol(Symbol from_symbol, Symbol replacement, mata::Alphabet* alphabet,
                                    ReplaceMode replace_mode = ReplaceMode::All);

/**
 * Create NFT modelling a reluctant leftmost replace of symbol @p from_symbol to @p replacement.
 * @param from_symbol Symbol to replace.
 * @param replacement Literal to replace with.
 * @param alphabet Alphabet over which to create the NFT.
 * @param replace_mode Whether to replace all or just the single (the leftmost) occurrence of @p from_symbol.
 * @return The reluctant leftmost replace NFT.
 */
Nft replace_reluctant_single_symbol(Symbol from_symbol, const Word& replacement, mata::Alphabet* alphabet,
                                    ReplaceMode replace_mode = ReplaceMode::All);

/**
 * @brief Implementation of all reluctant replace versions.
 */
class ReluctantReplace {
public:
    /**
     * @brief Create NFT modelling a reluctant leftmost replace of regex @p regex to @p replacement.
     *
     * The most general replace operation, handling any regex as the part to be replaced.
     * @param regex NFA representing regex to be replaced.
     * @param replacement Literal to replace with.
     * @param alphabet Alphabet over which to create the NFT.
     * @param replace_mode Whether to replace all or just the single (the leftmost) occurrence of @p regex.
     * @param begin_marker Symbol to be used internally as a begin marker of replaced @p regex.
     * @return The reluctant leftmost replace NFT.
     */
    static Nft replace_regex(nfa::Nfa regex, const Word& replacement, Alphabet* alphabet,
                             ReplaceMode replace_mode = ReplaceMode::All, Symbol begin_marker = BEGIN_MARKER);
    /**
     * Create NFT modelling a reluctant leftmost replace of literal @p literal to @p replacement.
     * @param literal Literal to replace.
     * @param replacement Literal to replace with.
     * @param alphabet Alphabet over which to create the NFT.
     * @param replace_mode Whether to replace all or just the single (the leftmost) occurrence of @p literal.
     * @param end_marker Symbol to be used internally as an end marker marking the end of the replaced literal.
     * @return The reluctant leftmost replace NFT.
     */
    static Nft replace_literal(const Word& literal, const Word& replacement, Alphabet* alphabet,
                                            ReplaceMode replace_mode = ReplaceMode::All, Symbol end_marker = END_MARKER);
    /**
     * Create NFT modelling a reluctant leftmost replace of symbol @p from_symbol to @p replacement.
     * @param from_symbol Symbol to replace.
     * @param replacement Symbol to replace with.
     * @param alphabet Alphabet over which to create the NFT.
     * @param replace_mode Whether to replace all or just the single (the leftmost) occurrence of @p from_symbol.
     * @return The reluctant leftmost replace NFT.
     */
    static Nft replace_symbol(Symbol from_symbol, Symbol replacement, mata::Alphabet* alphabet,
                                           ReplaceMode replace_mode = ReplaceMode::All);
    /**
     * Create NFT modelling a reluctant leftmost replace of symbol @p from_symbol to @p replacement.
     * @param from_symbol Symbol to replace.
     * @param replacement Literal to replace with.
     * @param alphabet Alphabet over which to create the NFT.
     * @param replace_mode Whether to replace all or just the single (the leftmost) occurrence of @p from_symbol.
     * @return The reluctant leftmost replace NFT.
     */
    static Nft replace_symbol(Symbol from_symbol, const Word& replacement, mata::Alphabet* alphabet,
                                           ReplaceMode replace_mode = ReplaceMode::All);
protected:
    nfa::Nfa end_marker_dfa(nfa::Nfa regex);
    Nft marker_nft(const nfa::Nfa& marker_dfa, Symbol marker);

    nfa::Nfa generic_marker_dfa(const std::string& regex, Alphabet* alphabet);
    nfa::Nfa generic_marker_dfa(nfa::Nfa regex, Alphabet* alphabet);

    nfa::Nfa begin_marker_nfa(const std::string& regex, Alphabet* alphabet);
    nfa::Nfa begin_marker_nfa(nfa::Nfa regex, Alphabet* alphabet);

    Nft begin_marker_nft(const nfa::Nfa& marker_nfa, Symbol begin_marker);
    Nft end_marker_dft(const nfa::Nfa& end_marker_dfa, Symbol end_marker);
    nfa::Nfa reluctant_nfa_with_marker(nfa::Nfa nfa, Symbol marker, Alphabet* alphabet);

    Nft reluctant_leftmost_nft(const std::string& regex, Alphabet* alphabet, Symbol begin_marker, const Word& replacement, ReplaceMode replace_mode);
    Nft reluctant_leftmost_nft(nfa::Nfa nfa, Alphabet* alphabet, Symbol begin_marker, const Word& replacement, ReplaceMode replace_mode);

    Nft replace_literal_nft(const Word& literal, const Word& replacement, const Alphabet* alphabet, Symbol end_marker,
                            ReplaceMode replace_mode = ReplaceMode::All);
};

} // Namespace mata::nft::strings.

#endif // MATA_NFT_STRING_SOLVING_HH_.
