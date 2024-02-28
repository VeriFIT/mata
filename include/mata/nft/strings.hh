/* nfa-strings.hh -- Operations on NFAs for string solving.
 */

#ifndef MATA_NFT_STRING_SOLVING_HH_
#define MATA_NFT_STRING_SOLVING_HH_

#include "mata/nfa/strings.hh"
#include "nft.hh"

namespace mata::nft::strings {

constexpr Symbol BEGIN_MARKER{ EPSILON - 100 };
constexpr Symbol END_MARKER{ EPSILON - 99 };

/**
 * How many occurrences of the regex to replace, in order from left to right?
 */
enum class ReplaceMode {
    Single, ///< Replace only the first occurrence of the regex.
    All, ///< Replace all occurrences of the regex.
};

/**
 * Create identity transducer over the @p alphabet with @p level_cnt levels.
 */
Nft create_identity(mata::Alphabet* alphabet, Level level_cnt = 2);

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

Nft replace_reluctant(
    const Word& literal,
    const Word& replacement,
    Alphabet* alphabet,
    // TODO(nft): Change into constants?
    ReplaceMode replace_mode,
    Symbol begin_marker = BEGIN_MARKER
);

Nft replace_reluctant(
    const std::string& regex,
    const Word& replacement,
    Alphabet* alphabet,
    // TODO(nft): Change into constants?
    ReplaceMode replace_mode,
    Symbol begin_marker = BEGIN_MARKER
);

Nft replace_reluctant(
    nfa::Nfa regex,
    const Word& replacement,
    Alphabet* alphabet,
    ReplaceMode replace_mode,
    Symbol begin_marker = BEGIN_MARKER
);

Nft replace_reluctant_literal(const Word& literal, const Word& replacement, Alphabet* alphabet,
                              ReplaceMode replace_mode, Symbol end_marker = END_MARKER);

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

Nft replace_literal_nft(const Word& literal, const Word& replacement, const Alphabet* alphabet, const Symbol end_marker,
                        ReplaceMode replace_mode = ReplaceMode::All);
} // Namespace mata::nft::strings.

#endif // MATA_NFT_STRING_SOLVING_HH_.
