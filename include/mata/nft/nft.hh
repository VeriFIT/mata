/* nft.hh -- Nondeterministic finite automaton (over finite words).
 */

#ifndef MATA_NFT_HH_
#define MATA_NFT_HH_

// Static data structures, such as search stack, in algorithms. Might have some effect on some algorithms (like
//  fragile_revert).
//#define _STATIC_STRUCTURES_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/parser/parser.hh"
#include "mata/utils/utils.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/parser/inter-aut.hh"
#include "mata/utils/synchronized-iterator.hh"
#include "mata/utils/sparse-set.hh"
#include "types.hh"
#include "delta.hh"

#include "mata/nfa/nfa.hh"

/**
 * Nondeterministic Finite Automata including structures, transitions and algorithms.
 *
 * In particular, this includes:
 *   1. Structures (Automaton, Transitions, Results, Delta),
 *   2. Algorithms (operations, checks, tests),
 *   3. Constructions.
 *
 * Other algorithms are included in mata::nft::Plumbing (simplified API for, e.g., binding)
 * and mata::nft::algorithms (concrete implementations of algorithms, such as for complement).
 */
namespace mata::nft {

class Levels: public std::vector<Level> {
    using super = std::vector<Level>;
public:
    Levels& set(State state, Level level = DEFAULT_LEVEL);
    using super::vector;
    Levels(const std::vector<Level>& levels): super(levels) {}
    Levels(std::vector<Level>&& levels): super(std::move(levels)) {}
};

/**
 * A class representing an NFT.
 */
class Nft : public mata::nfa::Nfa {
public:
    /**
     * @brief Vector of levels giving each state a level in range from 0 to @c num_of_levels - 1.
     *
     * For state `q`, `levels[q]` gives the state `q` a level.
     */
    Levels levels{};
    /**
     * @brief Number of levels (tracks) the transducer recognizes. Each transducer transition will comprise
     *  @c num_of_levels of NFA transitions.
     */
    size_t num_of_levels{ DEFAULT_NUM_OF_LEVELS };

    /// Key value store for additional attributes for the NFT. Keys are attribute names as strings and the value types
    ///  are up to the user.
    /// For example, we can set up attributes such as "state_dict" for state dictionary attribute mapping states to their
    ///  respective names, or "transition_dict" for transition dictionary adding a human-readable meaning to each
    ///  transition.
    // TODO: When there is a need for state dictionary, consider creating default library implementation of state
    //  dictionary in the attributes.

public:
    explicit Nft(Delta delta = {}, utils::SparseSet<State> initial_states = {},
                 utils::SparseSet<State> final_states = {}, Levels levels = {}, const size_t num_of_levels = DEFAULT_NUM_OF_LEVELS,
                 Alphabet* alphabet = nullptr)
        : mata::nfa::Nfa(std::move(delta), std::move(initial_states), std::move(final_states), alphabet), num_of_levels(num_of_levels) {
        this->levels = levels.empty() ? Levels(num_of_states(), DEFAULT_LEVEL) : std::move(levels);
    }
    /**
     * @brief Construct a new explicit NFT with num_of_states states and optionally set initial and final states.
     *
     * @param[in] num_of_states Number of states for which to preallocate Delta.
     */
    explicit Nft(const size_t num_of_states, StateSet initial_states = {},
                 StateSet final_states = {}, Levels levels = {}, const size_t num_of_levels = DEFAULT_NUM_OF_LEVELS,
                 Alphabet* alphabet = nullptr)
        : mata::nfa::Nfa(num_of_states, std::move(initial_states), std::move(final_states), alphabet),
          num_of_levels(num_of_levels) {
        this->levels = levels.empty() ? Levels(num_of_states, DEFAULT_LEVEL) : std::move(levels);
    }

    explicit Nft(const mata::nfa::Nfa& other): mata::nfa::Nfa(other), levels(other.num_of_states(), DEFAULT_LEVEL) {}

    /**
     * @brief Construct a new explicit NFT from other NFT.
     */
    Nft(const Nft& other) = default;

    Nft(Nft&& other) noexcept
        : levels{ std::move(other.levels) }, num_of_levels{ other.num_of_levels } {
            delta = std::move(other.delta);
            initial = std::move(other.initial);
            final = std::move(other.final);
            attributes = std::move(other.attributes);
            alphabet = other.alphabet;
            other.alphabet = nullptr;
    }

    Nft& operator=(const Nft& other) = default;
    Nft& operator=(Nft&& other) noexcept;

    /**
     * Add a new (fresh) state to the automaton.
     * @return The newly created state.
     */
    State add_state();

    /**
     * Add state @p state to @c this if @p state is not in @c this yet.
     * @return The requested @p state.
     */
    State add_state(State state);

    /**
     * Add a new (fresh) state to the automaton with level @p level.
     * @return The newly created state.
     */
    State add_state_with_level(Level level);

    /**
     * Add state @p state to @c this with level @p level if @p state is not in @c this yet.
     * @return The requested @p state.
     */
    State add_state_with_level(State state, Level level);

    /**
     * Inserts a @p word into the NFT from a source state @p src to a target state @p tgt.
     * Creates new states along the path of the @p word.
     *
     * If the length of @p word is less than @c num_of_levels, then the last symbol of @p word
     * will form a transition going directly from the last inner state to @p tgt. The level
     * of the state @p tgt must be 0 or greater than the level of the last inner state.
     *
     * @param src The source state where the word begins. It must already be a part of the transducer.
     * @param word The nonempty word to be inserted into the NFA.
     * @param tgt The target state where the word ends. If set to Limits::max_state, the word ends in a new state.
     */
    State insert_word(const State src, const Word &word, const State tgt = Limits::max_state);

    /**
     * Inserts a word, which is created by interleaving parts from @p word_part_on_level, into the NFT
     * from a source state @p src to a target state @p tgt, creating new states along its path.
     *
     * The length of the inserted word equals @c num_of_levels * the maximum word length in the vector @p word_part_on_level.
     * At least one Word in @p word_part_on_level must be nonempty.
     * The vector @p word_part_on_level must have a size equal to @c num_of_levels.
     * Words shorter than the maximum word length are interpreted as words followed by a sequence of epsilons to match the maximum length.
     *
     * @param src The source state where the word begins. This state must already exist in the transducer and must be of a level 0.
     * @param word_part_on_level The vector of word parts, with each part corresponding to a different level.
     * @param tgt The target state where the word ends. If set to Limits::max_state, the word ends in a new state.
     */
    State insert_word_by_parts(const State src, const std::vector<Word> &word_part_on_level, const State tgt = Limits::max_state);

    /**
    * Inserts an identity transition into the NFT.
    *
    * @param state The state where the identity transition will be inserted. This serves as both the source and target state.
    * @param symbol The symbol used for the identity transition.
    */
    void insert_identity(const State state, const Symbol symbol);

    /**
     * @brief Clear the underlying NFT to a blank NFT.
     *
     * The whole NFT is cleared, each member is set to its zero value.
     */
    void clear();

    /**
     * @brief Check if @c this is exactly identical to @p aut.
     *
     * This is exact equality of automata, including state numbering (so even stronger than isomorphism),
     *  essentially only useful for testing purposes.
     * @return True if automata are exactly identical, false otherwise.
     */
    bool is_identical(const Nft& aut) const;

    /**
     * @brief Remove inaccessible (unreachable) and not co-accessible (non-terminating) states in-place.
     *
     * Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
     * starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
     * the starting point of a path ending in a final state).
     *
     * @param[out] state_renaming Mapping of trimmed states to new states.
     * @return @c this after trimming.
     */
    Nft& trim(StateRenaming* state_renaming = nullptr);

    /**
     * @brief In-place concatenation.
     */
    Nft& concatenate(const Nft& aut);

    /**
     * @brief In-place union
     */
    Nft& uni(const Nft &aut);

    /**
     * Unify transitions to create a directed graph with at most a single transition between two states.
     * @param[in] abstract_symbol Abstract symbol to use for transitions in digraph.
     * @return An automaton representing a directed graph.
     */
    Nft get_one_letter_aut(Symbol abstract_symbol = 'x') const;

    /**
     * Unify transitions to create a directed graph with at most a single transition between two states.
     *
     * @param[out] result An automaton representing a directed graph.
     */
    void get_one_letter_aut(Nft& result) const;

    void make_one_level_aut(const utils::OrdVector<Symbol> &dcare_replacements = { DONT_CARE });

    Nft get_one_level_aut(const utils::OrdVector<Symbol> &dont_care_symbol_replacements = { DONT_CARE }) const;

    void get_one_level_aut(Nft& result, const utils::OrdVector<Symbol> &dont_care_symbol_replacements = { DONT_CARE }) const;

    /**
     * @brief Prints the automaton in DOT format
     *
     * @param[in] ascii Whether to use ASCII characters for the output.
     * @return automaton in DOT format
     */
    std::string print_to_DOT(const bool ascii = false) const;
    /**
     * @brief Prints the automaton to the output stream in DOT format
     *
     * @param[in] ascii Whether to use ASCII characters for the output.
     */
    void print_to_DOT(std::ostream &output, const bool ascii = false) const;
    /**
     * @brief Prints the automaton in mata format
     *
     * If you need to parse the automaton again, use IntAlphabet in construct()
     *
     * @return automaton in mata format
     * TODO handle alphabet of the automaton, currently we print the exact value of the symbols
     */
    std::string print_to_mata() const;
    /**
     * @brief Prints the automaton to the output stream in mata format
     *
     * If you need to parse the automaton again, use IntAlphabet in construct()
     *
     * TODO handle alphabet of the automaton, currently we print the exact value of the symbols
     */
    void print_to_mata(std::ostream &output) const;

    /// Is the language of the automaton universal?
    bool is_universal(const Alphabet& alphabet, Run* cex = nullptr,
                      const ParameterMap& params = {{ "algorithm", "antichains" }}) const;
    /// Is the language of the automaton universal?
    bool is_universal(const Alphabet& alphabet, const ParameterMap& params) const;

    /// Checks whether a word is in the language of an automaton.
    bool is_in_lang(const Run& word) const;
    /// Checks whether a word is in the language of an automaton.
    bool is_in_lang(const Word& word) { return is_in_lang(Run{ word, {} }); }

    /// Checks whether the prefix of a string is in the language of an automaton
    bool is_prfx_in_lang(const Run& word) const;

    std::pair<Run, bool> get_word_for_path(const Run& run) const;

    /**
     * @brief Get the set of all words in the language of the automaton whose length is <= @p max_length
     *
     * If you have an automaton with finite language (can be checked using @ref is_acyclic),
     * you can get all words by calling
     *      get_words(aut.num_of_states())
     */
    std::set<Word> get_words(unsigned max_length);

}; // class Nft.

// Allow variadic number of arguments of the same type.
//
// Using parameter pack and variadic arguments.
// Adapted from: https://www.fluentcpp.com/2019/01/25/variadic-number-function-parameters-type/.
/// Pack of bools for reasoning about a sequence of parameters.
template<bool...> struct bool_pack{};
/// Check that for all values in a pack @p Ts are 'true'.
template<typename... Ts> using conjunction = std::is_same<bool_pack<true,Ts::value...>, bool_pack<Ts::value..., true>>;
/// Check that all types in a sequence of parameters @p Ts are of type @p T.
template<typename T, typename... Ts> using AreAllOfType = typename conjunction<std::is_same<Ts, T>...>::type;

Nft uni(const Nft &lhs, const Nft &rhs);

/**
 * @brief Compute intersection of two NFTs.
 *
 * Both automata can contain ε-transitions. Epsilons will be handled as alphabet symbols.
 *
 * Automata must share alphabets. //TODO: this is not implemented yet.
 * Transducers must have equal values of @c num_of_levels.
 *
 * @param[in] lhs First NFT to compute intersection for.
 * @param[in] rhs Second NFT to compute intersection for.
 * @param[out] prod_map Mapping of pairs of the original states (lhs_state, rhs_state) to new product states (not used internally, allocated only when !=nullptr, expensive).
 * @param[in] lhs_first_aux_state The first auxiliary state in @p lhs. Two auxiliary states does not form a product state.
 * @param[in] rhs_first_aux_state The first auxiliary state in @p rhs. Two auxiliary states does not form a product state.
 * @return NFT as a product of NFTs @p lhs and @p rhs.
 */
Nft intersection(const Nft& lhs, const Nft& rhs, std::unordered_map<std::pair<State, State>, State> *prod_map = nullptr,
                 const State lhs_first_aux_state = Limits::max_state, const State rhs_first_aux_state = Limits::max_state);


/**
 * @brief Composes two NFTs.
 *
 * Takes two NFTs and their corresponding synchronization levels as input,
 * and returns a new NFT that represents their composition.
 *
 * Vectors of synchronization levels have to be non-empty and of the the same size.
 *
 * @param[in] lhs First transducer to compose.
 * @param[in] rhs Second transducer to compose.
 * @param[in] lhs_sync_levels Ordered vector of synchronization levels of the @p lhs.
 * @param[in] rhs_sync_levels Ordered vector of synchronization levels of the @p rhs.
 * @return A new NFT after the composition.
 */
Nft compose(const Nft& lhs, const Nft& rhs, const utils::OrdVector<Level>& lhs_sync_levels, const utils::OrdVector<Level>& rhs_sync_levels);

/**
 * @brief Composes two NFTs.
 *
 * Takes two NFTs and their corresponding synchronization levels as input,
 * and returns a new NFT that represents their composition.
 *
 * @param[in] lhs First transducer to compose.
 * @param[in] rhs Second transducer to compose.
 * @param[in] lhs_sync_level The synchronization level of the @p lhs.
 * @param[in] rhs_sync_level The synchronization level of the @p rhs.
 * @return A new NFT after the composition.
 */
Nft compose(const Nft& lhs, const Nft& rhs, const Level& lhs_sync_level = 1, const Level rhs_sync_level = 0);

/**
 * @brief Concatenate two NFTs.
 *
 * Supports epsilon symbols when @p use_epsilon is set to true.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] use_epsilon Whether to concatenate over epsilon symbol.
 * @param[out] lhs_state_renaming Map mapping lhs states to result states.
 * @param[out] rhs_state_renaming Map mapping rhs states to result states.
 * @return Concatenated automaton.
 */
// TODO: check how fast is using just concatenate over epsilon and then call remove_epsilon().
Nft concatenate(const Nft& lhs, const Nft& rhs, bool use_epsilon = false,
                StateRenaming* lhs_state_renaming = nullptr, StateRenaming* rhs_state_renaming = nullptr);

/**
 * @brief Compute automaton accepting complement of @p aut.
 *
 * @param[in] aut Automaton whose complement to compute.
 * @param[in] alphabet Alphabet used for complementation.
 * @param[in] params Optional parameters to control the complementation algorithm:
 * - "algorithm": "classical" (classical algorithm determinizes the automaton, makes it complete and swaps final and non-final states);
 * - "minimize": "true"/"false" (whether to compute minimal deterministic automaton for classical algorithm);
 * @return Complemented automaton.
 */
Nft complement(const Nft& aut, const Alphabet& alphabet,
               const ParameterMap& params = {{ "algorithm", "classical" }, { "minimize", "false" }});

/**
 * @brief Compute automaton accepting complement of @p aut.
 *
 * This overloaded version complements over an already created ordered set of @p symbols instead of an alphabet.
 * This is a more efficient solution in case you already have @p symbols precomputed or want to complement multiple
 *  automata over the same set of @c symbols: the function does not need to compute the ordered set of symbols from
 *  the alphabet again (and for each automaton).
 *
 * @param[in] aut Automaton whose complement to compute.
 * @param[in] symbols Symbols to complement over.
 * @param[in] params Optional parameters to control the complementation algorithm:
 * - "algorithm": "classical" (classical algorithm determinizes the automaton, makes it complete and swaps final and non-final states);
 * - "minimize": "true"/"false" (whether to compute minimal deterministic automaton for classical algorithm);
 * @return Complemented automaton.
 */
Nft complement(const Nft& aut, const utils::OrdVector<Symbol>& symbols,
               const ParameterMap& params = {{ "algorithm", "classical" }, { "minimize", "false" }});

/**
 * @brief Compute minimal deterministic automaton.
 *
 * @param[in] aut Automaton whose minimal version to compute.
 * @param[in] params Optional parameters to control the minimization algorithm:
 * - "algorithm": "brzozowski"
 * @return Minimal deterministic automaton.
 */
Nft minimize(const Nft &aut, const ParameterMap& params = {{ "algorithm", "brzozowski" }});

/**
 * @brief Determinize automaton.
 *
 * @param[in] aut Automaton to determinize.
 * @param[out] subset_map Map that maps sets of states of input automaton to states of determinized automaton.
 * @return Determinized automaton.
 */
Nft determinize(const Nft& aut, std::unordered_map<StateSet, State> *subset_map = nullptr);

/**
 * @brief Reduce the size of the automaton.
 *
 * @param[in] aut Automaton to reduce.
 * @param[out] state_renaming Mapping of original states to reduced states.
 * @param[in] params Optional parameters to control the reduction algorithm:
 * - "algorithm": "simulation".
 * @return Reduced automaton.
 */
Nft reduce(const Nft &aut, StateRenaming *state_renaming = nullptr,
           const ParameterMap& params = {{ "algorithm", "simulation" } });

/**
 * @brief Checks inclusion of languages of two NFTs: @p smaller and @p bigger (smaller <= bigger).
 *
 * @param[in] smaller First automaton to concatenate.
 * @param[in] bigger Second automaton to concatenate.
 * @param[out] cex Counterexample for the inclusion.
 * @param[in] alphabet Alphabet of both NFTs to compute with.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
bool is_included(const Nft& smaller, const Nft& bigger, Run* cex, const Alphabet* alphabet = nullptr,
                 const ParameterMap& params = {{ "algorithm", "antichains" }});

/**
 * @brief Checks inclusion of languages of two NFTs: @p smaller and @p bigger (smaller <= bigger).
 *
 * @param[in] smaller First automaton to concatenate.
 * @param[in] bigger Second automaton to concatenate.
 * @param[in] alphabet Alphabet of both NFTs to compute with.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
inline bool is_included(const Nft& smaller, const Nft& bigger, const Alphabet* const alphabet = nullptr,
                        const ParameterMap& params = {{ "algorithm", "antichains" }}) {
    return is_included(smaller, bigger, nullptr, alphabet, params);
}

/**
 * @brief Perform equivalence check of two NFTs: @p lhs and @p rhs.
 *
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] alphabet Alphabet of both NFTs to compute with.
 * @param[in] params[ Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool are_equivalent(const Nft& lhs, const Nft& rhs, const Alphabet* alphabet,
                    const ParameterMap& params = {{ "algorithm", "antichains"}});

/**
 * @brief Perform equivalence check of two NFTs: @p lhs and @p rhs.
 *
 * The current implementation of @c Nft does not accept input alphabet. For this reason, an alphabet
 * has to be created from all transitions each time an operation on alphabet is called. When calling this function,
 * the alphabet has to be computed first.
 *
 * Hence, this function is less efficient than its alternative taking already defined alphabet as its parameter.
 * That way, alphabet has to be computed only once, as opposed to the current ad-hoc construction of the alphabet.
 * The use of the alternative with defined alphabet should be preferred.
 *
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool are_equivalent(const Nft& lhs, const Nft& rhs, const ParameterMap& params = {{ "algorithm", "antichains"}});

// Reverting the automaton by one of the three functions below,
// currently simple_revert seems best (however, not tested enough).
Nft revert(const Nft& aut);

// This revert algorithm is fragile, uses low level accesses to Nft and static data structures,
// and it is potentially dangerous when there are used symbols with large numbers (allocates an array indexed by symbols)
// It is faster asymptotically and for somewhat dense automata,
// the same or a little bit slower than simple_revert otherwise.
// Not affected by pre-reserving vectors.
Nft fragile_revert(const Nft& aut);

// Reverting the automaton by a simple algorithm, which does a lot of random access addition to Post and Move.
//  Much affected by pre-reserving vectors.
Nft simple_revert(const Nft& aut);

// Reverting the automaton by a modification of the simple algorithm.
// It replaces random access addition to SymbolPost by push_back and sorting later, so far seems the slowest of all, except on
//  dense automata, where it is almost as slow as simple_revert. Candidate for removal.
Nft somewhat_simple_revert(const Nft& aut);

// Removing epsilon transitions
Nft remove_epsilon(const Nft& aut, Symbol epsilon = EPSILON);

/**
 * @brief Projects out specified levels @p levels_to_project in the given transducer @p nft.
 *
 * @param[in] nft The transducer for projection.
 * @param[in] levels_to_project A non-empty ordered vector of levels to be projected out from the transducer. It must
 *  contain only values that are greater than or equal to 0 and smaller than @c num_of_levels.
 * @param[in] repeat_jump_symbol Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @p DONT_CARE symbols.
 * @return A new projected transducer.
 */
Nft project_out(const Nft& nft, const utils::OrdVector<Level>& levels_to_project, bool repeat_jump_symbol = true);

/**
 * @brief Projects out specified level @p level_to_project in the given transducer @p nft.
 *
 * @param[in] nft The transducer for projection.
 * @param[in] level_to_project A level that is going to be projected out from the transducer. It has to be greater than or
 *  equal to 0 and smaller than @c num_of_levels.
 * @param[in] repeat_jump_symbol Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE symbols.
 * @return A new projected transducer.
 */
Nft project_out(const Nft& nft, Level level_to_project, bool repeat_jump_symbol = true);

/**
 * @brief Projects to specified levels @p levels_to_project in the given transducer @p nft.
 *
 * @param[in] nft The transducer for projection.
 * @param[in] levels_to_project A non-empty ordered vector of levels the transducer is going to be projected to.
 *  It must contain only values greater than or equal to 0 and smaller than @c num_of_levels.
 * @param[in] repeat_jump_symbol Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE symbols.
 * @return A new projected transducer.
 */
Nft project_to(const Nft& nft, const utils::OrdVector<Level>& levels_to_project, bool repeat_jump_symbol = true);

/**
 * @brief Projects to a specified level @p level_to_project in the given transducer @p nft.
 *
 * @param[in] nft The transducer for projection.
 * @param[in] level_to_project A level the transducer is going to be projected to. It has to be greater than or equal to 0
 *  and smaller than @c num_of_levels.
 * @param[in] repeat_jump_symbol Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE symbols.
 * @return A new projected transducer.
 */
Nft project_to(const Nft& nft, Level level_to_project, bool repeat_jump_symbol = true);

/**
 * @brief Inserts new levels, as specified by the mask @p new_levels_mask, into the given transducer @p nft.
 *
 * @c num_of_levels must be greater than 0.
 * The vector @c new_levels_mask must be nonempty, its length must be greater than @c num_of_levels,
 * and it must contain exactly @c num_of_levels occurrences of false.
 *
 * @param[in] nft The original transducer.
 * @param[in] new_levels_mask A mask representing the old and new levels. The vector {1, 0, 1, 1, 0} indicates
 *  that one level is inserted before level 0 and two levels are inserted before level 1.
 * @param[in] default_symbol The default symbol to be used for transitions at the inserted levels.
 * @param[in] repeat_jump_symbol Specifies whether the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE symbols.
 */
Nft insert_levels(const Nft& nft, const BoolVector& new_levels_mask, const Symbol default_symbol = DONT_CARE, bool repeat_jump_symbol = true);

/**
 * @brief Inserts a new level @p new_level into the given transducer @p nft.
 *
 * @c num_of_levels must be greater than 0.
 *
 * @param[in] nft The original transducer.
 * @param[in] new_level Specifies the new level to be inserted into the transducer.
 *  If @p new_level is 0, then it is inserted before the 0-th level.
 *  If @p new_level is less than @c num_of_levels, then it is inserted before the level @c new_level-1.
 *  If @p new_level is greater than or equal to @c num_of_levels, then all levels from @c num_of_levels through @p new_level are appended after the last level.
 * @param[in] default_symbol The default symbol to be used for transitions at the inserted levels.
 * @param[in] repeat_jump_symbol Specifies whether the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE symbols.
 */
Nft insert_level(const Nft& nft, const Level new_level, const Symbol default_symbol = DONT_CARE, bool repeat_jump_symbol = true);

/** Encodes a vector of strings (each corresponding to one symbol) into a
 *  @c Word instance
 */
 // TODO: rename to something, but no idea to what.
 // Maybe we need some terminology - Symbols and Words are made of numbers.
 // What are the symbol names and their sequences?
Run encode_word(const Alphabet* alphabet, const std::vector<std::string>& input);

} // namespace mata::nft.

namespace std {
std::ostream& operator<<(std::ostream& os, const mata::nft::Nft& nft);
} // namespace std.

#endif /* MATA_NFT_HH_ */
