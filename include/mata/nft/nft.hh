/** @file
 * @brief Nondeterministic Finite Transducers including structures, transitions and algorithms.
 *
 * In particular, this includes:
 *   1. Structures (Transducer, Transitions, Results, Delta),
 *   2. Algorithms (operations, checks, tests),
 *   3. Constructions.
 *
 * Other algorithms are included in @c mata::nft::plumbing (simplified API for, e.g., bindings)
 * and @c mata::nft::algorithms (concrete implementations of algorithms, such as for inclusion).
 */

/**
 * @page nft Nondeterministic Finite Transducers (NFTs)
 *
 * @warning Mata provides an experimental (unstable) support for (non-)deterministic finite transducers (NFTs).
 *  The provided interface may change.
 *
 * @section nft_design The design of NFTs in Mata
 *
 * In Mata, the classical transducers from textbook definitions (where a transition is a tuple of
 *  `(initial state, input symbol, final state, output string)`) are encoded using a simpler data structure for the
 *  transition relation, @c mata::nft::Delta The data structures can directly represent only transitions of the form
 *  `p -a-> s` where `a` is a single symbol or epsilon. A “high-level” transitions of the form `p -abc/def-> q` where
 * 'abc' is an input word and def is an output word are encoded as a sequence of these 'low level' transitions:
 *  `p -a- p’ -d-> q -b-> q' -e-> r -c-> r' -f-> s` (odd transitions read letters of the input track, even transitions read
 *  letters of the output track, a transition can have epsilon instead of a letter). The primed states are a sort of
 *  internal states, to where the automaton gets after reading a letter on the input tape. States are distinguished by
 *  their “level”. “Normal” states  p,q,r,s have level 0, the internal states p’,q’,r’ have in this case the levels 1
 *  (and you could have n-tape transducers where level can be larger than 1).
 * In Mata, the @c mata::nft::Delta interface is used only for the manual handling of the internal delta representation,
 *  taking only (initial state, input symbol, final state)
 * We find it useful to think about NFTs in Mata as normal NFAs with a single symbol on a transition (because Mata uses
 *  the same underlying data structure for NFTs as for NFAs) where states are annotated with levels. The 'NFT' states
 *  are the states with levels 0, the other levels are for the internal states. Each NFT transition is a sequence of NFA
 *  transitions, one per tape, between two states with levels 0. Each NFA transition performs a single symbol read on
 *  the tape of the source state.
 * A sequence of symbols on transitions is not a single word on any tape, but an interleaved description of a single NFT
 *  transition over multiple tapes, e.g., q1(l:0)-{a}->q2(l:1)-{b}->q3(l:2)-{c}->q4(l:0) (where (l:X) describes the level
 *  of the state) is a single 'NFT' transition (from a state with level 0 to another state with level 0) where the NFT
 *  symbol read is a 3-tape symbol ('a', 'b', 'c'), interpreted as 'read 'a' on tape 0, read 'b' on tape 1, and read 'c'
 *  on tape 2.' When you want to read 'af' on a single tape, you need to construct two such transitions where on the
 *  remaining tapes, you read an epsilon symbol.
 *
 * @section nft_usage Working with NFTs
 *
 * There are some utility functions to simplify creating this NFA-like Delta data structure for NFTs such as
 *  @c mata::nft::Nft::insert_word() (inserting the NFA-like sequence of transitions on different tapes: 'abcd' which means to
 *  read 'ac' on tape 0 and 'bd' on tape 1), and @c mata::nft::Nft::insert_word_by_levels() (inserting word for each tape
 *  separately: {'ac', 'bd'} to achieve the same as in the previous).
 * If you however want precise control over the created transitions, you can omit using the utility NFT functions and
 *  build your NFT like an NFA using the Nft::Delta::* operations directly, adding a single symbol on a single tape per
 *  add() call. You will have to correctly specify the levels for the states. When working with 2-tape NFTs modelling a
 *  replace operation, we use @c mata::applications::strings::replace namespace with utility functions such as
 *  @c mata::applications::strings::replace::replace_reluctant_regex() (accepting parameters: regex as the input for the tape 0,
 *  and a word as a replacement on tape 1), etc.
 * The notion of jumps (in the jump mode RepeatSymbol, a mode designed for NFTs) is just an optimization to reduce the
 *  size of the NFT. It is an approach to simplify the data structure for NFTs where you can say that you jump over
 *  several internal states and NFA-like transitions to up to the next state with level 0, each transition in the
 *  sequence reading the same single symbol of the jump. That is useful when, for example, having a NFT reading string
 *  'abc' on all tapes. You can encode it as a sequence of jumps between states with level 0 as
 *  q0(l:0)-{a}->q1(l:0)-{b}->q2(l:0)-{c}->q3(l:0). Notice that the internal states are not present, but they are implied
 *  by the jump.
 * Ideally, one should not even have to think about the levels and the intermediate states when properly using the
 *  utility functions (from the @c mata::nft::Nft class and the @c mata::nft namespace in general).
 *
 * @see @ref examples/nft.cc example.
 *
 * @note If you find some expected NFT operation or utility function missing, do not hesitate to let us know and we will
 *  implement it. The interface for NFTs is not stable yet, so we are open to any and all feedback.
 */

#ifndef MATA_NFT_HH_
#define MATA_NFT_HH_

// Static data structures, such as search stack, in algorithms. Might have some effect on some algorithms (like
//  fragile_revert).
//#define _STATIC_STRUCTURES_

#include <algorithm>
#include <cassert>
#include <limits>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "delta.hh"
#include "types.hh"
#include "mata/alphabet.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/utils/utils.hh"

#include "mata/nfa/nfa.hh"

namespace mata::nft {

/**
 * @brief A class representing an NFT.
 */
class Nft: public nfa::Nfa {
private:
    using super = nfa::Nfa;
public:
    /**
     * @brief Vector of levels giving each state a level in range from 0 to @c levels.num_of_levels - 1.
     *
     * For state `q`, `levels[q]` gives the state `q` a level.
     *
     * Also holds the number of levels in the NFT in @c levels.num_of_levels.
     */
    Levels levels{};

    /// Key value store for additional attributes for the NFT. Keys are attribute names as strings and the value types
    ///  are up to the user.
    /// For example, we can set up attributes such as "state_dict" for state dictionary attribute mapping states to their
    ///  respective names, or "transition_dict" for transition dictionary adding a human-readable meaning to each
    ///  transition.
    // TODO: When there is a need for state dictionary, consider creating default library implementation of state
    //  dictionary in the attributes.

public:
    explicit Nft(
            Delta delta = {}, utils::SparseSet<State> initial_states = {},
            utils::SparseSet<State> final_states = {}, Levels levels = {},
            Alphabet* alphabet = nullptr)
        : Nfa{ std::move(delta), std::move(initial_states), std::move(final_states), alphabet },
          levels{ levels.empty() ? Levels{ levels.num_of_levels, num_of_states(), DEFAULT_LEVEL } : std::move(levels) } {}

    /**
     * @brief Construct a new explicit NFT with num_of_states states and optionally set initial and final states.
     *
     * @param[in] num_of_states Number of states for which to preallocate Delta.
     * @param initial_states Initial states of the NFT.
     * @param final_states Final states of the NFT.
     * @param levels Levels of the states.
     * @param alphabet Alphabet of the NFT.
     */
    explicit Nft(const size_t num_of_states, utils::SparseSet<State> initial_states = {},
                 utils::SparseSet<State> final_states = {}, Levels levels = {},
                 Alphabet* alphabet = nullptr)
        : Nfa{ num_of_states, std::move(initial_states), std::move(final_states), alphabet },
          levels{ levels.empty() ? Levels{ levels.num_of_levels, num_of_states, DEFAULT_LEVEL } : std::move(levels) } {}

    static Nft with_levels(
            Levels levels, const size_t num_of_states = 0, utils::SparseSet<State> initial_states = {},
            utils::SparseSet<State> final_states = {}, Alphabet* alphabet = nullptr) {
        return Nft{ num_of_states, std::move(initial_states), std::move(final_states), std::move(levels), alphabet };
    }

    static Nft with_levels(
            Levels levels, Delta delta, utils::SparseSet<State> initial_states = {},
            utils::SparseSet<State> final_states = {}, Alphabet* alphabet = nullptr) {
        return Nft{ std::move(delta), std::move(initial_states), std::move(final_states), std::move(levels), alphabet };
    }

    static Nft with_levels(
            const size_t num_of_levels, const size_t num_of_states = 0, utils::SparseSet<State> initial_states = {},
            utils::SparseSet<State> final_states = {}, Alphabet* alphabet = nullptr) {
        return Nft{ num_of_states, std::move(initial_states), std::move(final_states), Levels{ num_of_levels }, alphabet };
    }

    static Nft with_levels(
            const size_t num_of_levels, Delta delta, utils::SparseSet<State> initial_states = {},
            utils::SparseSet<State> final_states = {}, Alphabet* alphabet = nullptr) {
        return Nft{ std::move(delta), std::move(initial_states), std::move(final_states), Levels{ num_of_levels }, alphabet };
    }

    /**
     * @brief Construct a new explicit NFT from other NFT.
     */
    Nft(const Nft& other) = default;

    Nft(Nft&& other) noexcept
        : levels{ std::move(other.levels) } {
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
     * @brief Construct a new NFT with @p num_of_levels levels from NFA.
     * All states levels are set to the @p default_level. The transition function
     * remains the same as in the NFA.
     *
     * Note: Constructor functions with more options are available in mata::nft::builder.
     *
     * @param other NFA to be converted to NFT.
     * @param num_of_levels Number of levels for the NFT. (default: 1)
     * @param default_level Default level for the states. (default: 0)
     */
    explicit Nft(const mata::nfa::Nfa& other, const size_t num_of_levels = 1, const Level default_level = DEFAULT_LEVEL)
        : mata::nfa::Nfa(other), levels{ num_of_levels, num_of_states(), default_level } {}

    /**
     * @brief Construct a new NFT with @p num_of_levels levels from NFA.
     * All states levels are set to the @p default_level. The transition function
     * remains the same as in the NFA.
     *
     * Note: Constructor functions with more options are available in mata::nft::builder.
     *
     * @param other NFA to be converted to NFT.
     * @param num_of_levels Number of levels for the NFT. (default: 1)
     * @param default_level Default level for the states. (default: 0)
     */
    explicit Nft(Nfa&& other, const size_t num_of_levels = 1, const Level default_level = DEFAULT_LEVEL)
        : Nfa(std::move(other)), levels{ num_of_levels, num_of_states(), default_level } {}

    /**
     * @brief Construct a new NFT with @p num_of_levels levels from NFA.
     * All states levels are set to the @p default_level. The transition function
     * remains the same as in the NFA.
     *
     * Note: Constructor functions with more options are available in mata::nft::builder.
     *
     * @param other NFA to be converted to NFT.
     * @param levels Levels for the states of the NFA @c other.
     */
    explicit Nft(const Nfa& other, Levels levels): Nfa(other), levels{ std::move(levels) } {}

    /**
     * @brief Construct a new NFT with @p num_of_levels levels from NFA.
     * All states levels are set to the @p default_level. The transition function
     * remains the same as in the NFA.
     *
     * Note: Constructor functions with more options are available in mata::nft::builder.
     *
     * @param other NFA to be converted to NFT.
     * @param levels Levels for the states of the NFA @c other.
     */
    explicit Nft(Nfa&& other, Levels levels): Nfa{ std::move(other) }, levels{ std::move(levels) } {}

    Nft& operator=(const Nfa& other) noexcept;
    Nft& operator=(Nfa&& other) noexcept;

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
     * Get the number of states with level @p level.
     * @return The number of states with level @p level.
     */
    size_t num_of_states_with_level(Level level) const;

    /**
     * Inserts a @p word into the NFT from a source state @p source to a target state @p target.
     * Creates new states along the path of the @p word.
     *
     * If the length of @p word is less than @c num_of_levels, then the last symbol of @p word will form a transition
     *  going directly from the last inner state to @p target.
     *
     * @param source The source state where the word begins. @p source must already exist.
     * @param word The nonempty word to be inserted into the NFA.
     * @param target The target state where the word ends. @p target must already exist.
     * @return The state @p target where the inserted @p word ends.
     */
    State insert_word(State source, const Word &word, State target);

    /**
     * @brief Inserts a @p word into the NFT from a source state @p source to a newly created target state, creating
     *  new states along the path of the @p word.
     *
     * If the length of @p word is less than @c num_of_levels, then the last symbol of @p word
     *  will form a transition going directly from the last inner state to the newly created target.
     *
     * @param source The source state where the word begins. @p source must already exist.
     * @param word The nonempty word to be inserted into the NFA.
     * @return The newly created target where the inserted word ends.
     */
    State insert_word(State source, const Word &word);

    /**
     * @brief Add a single NFT transition.
     *
     * The transition leads from a source state @p source to a target state @p target, creating new inner states for all
     *  tapes.
     *
     * If the length of @p symbols is less than @c num_of_levels, then the last symbol of @p symbols will form a jump
     *  transition going directly from the last inner state to @p target.
     *
     * @param source The source state where the NFT transition begins. @p source must already exist.
     * @param symbols The nonempty set of symbols, one for each tape to be inserted into the NFT.
     * @param target The target state where the NFT transition ends. @p target must already exist.
     * @return The target state @p target.
     */
    State add_transition(State source, const std::vector<Symbol>& symbols, State target);

    /**
     * @brief Add a single NFT transition to the NFT from a source state @p source to a newly created target state,
     *  creating new inner states for all tapes.
     *
     * If the length of @p symbols is less than @c num_of_levels, the last symbol of @p symbols
     *  will form a transition going directly from the last inner state to the newly created target.
     *
     * @param source The source state where the transition begins. @p source must already exist.
     * @param symbols The nonempty set of symbols, one for each tape to be inserted into the NFT.
     * @return The target state @p target.
     */
    State add_transition(State source, const std::vector<Symbol>& symbols);

    /**
     * @brief Inserts a word, which is created by interleaving parts from @p word_parts_on_levels, into the NFT
     *  from a source state @p source to a target state @p target, creating new states along the path of @p word.
     *
     * The length of the inserted word equals @c num_of_levels * the maximum word length in the vector @p word_parts_on_levels.
     * At least one Word in @p word_parts_on_levels must be nonempty.
     * The vector @p word_parts_on_levels must have a size equal to @c num_of_levels.
     * Words shorter than the maximum word length are interpreted as words followed by a sequence of epsilons to match the maximum length.
     *
     * @param source The source state where the word begins. @p source must already exist and be of a level 0.
     * @param word_parts_on_levels The vector of word parts, with each part corresponding to a different level.
     * @param target The target state where the word ends. @p target must already exist and be of a level 0.
     * @return The state @p target where the inserted @p word_parts_on_levels ends.
     */
    State insert_word_by_levels(State source, const std::vector<Word>& word_parts_on_levels, State target);

    /**
     * @brief Inserts a word, which is created by interleaving parts from @p word_parts_on_levels, into the NFT
     *  from a source state @p source to a target state @p target, creating new states along the path of @p word.
     *
     * The length of the inserted word equals @c num_of_levels * the maximum word length in the vector @p word_parts_on_levels.
     * At least one Word in @p word_parts_on_levels must be nonempty.
     * The vector @p word_parts_on_levels must have a size equal to @c num_of_levels.
     * Words shorter than the maximum word length are interpreted as words followed by a sequence of epsilons to match the maximum length.
     *
     * @param source The source state where the word begins. @p source must already exist be of a level 0.
     * @param word_parts_on_levels The vector of word parts, with each part corresponding to a different level.
     * @return The newly created target where the inserted @p word_parts_on_levels ends.
     */
    State insert_word_by_levels(State source, const std::vector<Word>& word_parts_on_levels);

    /**
     * Inserts identity transitions into the NFT.
     *
     * @param state The state where the identity transition will be inserted. @p state serves as both the source and
     *  target state.
     * @param symbols The vector of symbols used for the identity transition. Identity will be created for each symbol in
     *  the vector.
     * @param jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
     * is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
     * of @c DONT_CARE symbols.
     * @return Self with inserted identity.
     */
    Nft& insert_identity(State state, const std::vector<Symbol>& symbols, JumpMode jump_mode = JumpMode::RepeatSymbol);

    /**
     * Inserts identity transitions into the NFT.
     *
     * @param state The state where the identity transition will be inserted. @p state serves as both the source and
     *  target state.
     * @param alphabet The alphabet with symbols used for the identity transition. Identity will be created for each symbol in the @p alphabet.
     * @param jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
     * is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
     * of @c DONT_CARE symbols.
     * @return Self with inserted identity.
     */
    Nft& insert_identity(State state, const Alphabet* alphabet, JumpMode jump_mode = JumpMode::RepeatSymbol);

    /**
     * Inserts an identity transition into the NFT.
     *
     * @param state The state where the identity transition will be inserted. @p state serves as both the source and
     *  target state.
     * @param symbol The symbol used for the identity transition.
     * @param jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
     * is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
     * of @c DONT_CARE symbols.
     * @return Self with inserted identity.
     */
    Nft& insert_identity(State state, Symbol symbol, JumpMode jump_mode = JumpMode::RepeatSymbol);

    /**
     * @brief Checks if the transducer contains any jump transition
     */
    bool contains_jump_transitions() const;

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
     * Remove simple epsilon transitions from the automaton.
     *
     * @sa mata::nft::remove_epsilon()
     */
    void remove_epsilon(Symbol epsilon = EPSILON);

    /**
     * @brief In-place concatenation.
     */
    Nft& concatenate(const Nft& aut);

    /**
     * @brief In-place union
     */
    Nft& unite_nondet_with(const Nft &aut);

     /**
      * @brief Get NFT where transitions of @c this are replaced with transitions over one symbol @p abstract_symbol
      *
      * The transitions over EPSILON are not replaced, neither are the transitions coming from a state with a level
      * from @p levels_to_keep.
      *
      * @param[in] levels_to_keep Transitions coming from states with any of these levels are not replaced.
      * @param[in] abstract_symbol The symbol to replace with.
      * @return Nft
      */
    Nft get_one_letter_aut(const std::set<Level>& levels_to_keep = {}, Symbol abstract_symbol = 'x') const;

    /**
     * Unify transitions to create a directed graph with at most a single transition between two states.
     *
     * @param[out] result An automaton representing a directed graph.
     */
    void get_one_letter_aut(Nft& result) const;

    /**
     * @brief Unwinds jump transitions in the transducer.
     *
     * @param[in] dont_care_symbol_replacements Vector of symbols to replace @c DONT_CARE symbols with.
     * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
     * is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
     * of @c DONT_CARE symbols.
     */
    void unwind_jumps_inplace(const utils::OrdVector<Symbol> &dont_care_symbol_replacements = { DONT_CARE }, JumpMode jump_mode = JumpMode::RepeatSymbol);

    /**
     * @brief Creates a transducer with unwinded jump transitions from the current one.
     *
     * @param[in] dont_care_symbol_replacements Vector of symbols to replace @c DONT_CARE symbols with.
     * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
     * is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
     * of @c DONT_CARE symbols.
     */
    Nft unwind_jumps(const utils::OrdVector<Symbol> &dont_care_symbol_replacements = { DONT_CARE }, JumpMode jump_mode = JumpMode::RepeatSymbol) const;

    /**
     * @brief Unwinds jump transitions in the given transducer.
     *
     * @param[out] result A transducer with only one level.
     * @param[in] dont_care_symbol_replacements Vector of symbols to replace @c DONT_CARE symbols with.
     * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
     * is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
     * of @c DONT_CARE symbols.
     */
    void unwind_jumps(
        Nft& result, const utils::OrdVector<Symbol>& dont_care_symbol_replacements = { DONT_CARE },
        JumpMode jump_mode = JumpMode::RepeatSymbol) const;

    /**
     * @brief Prints the automaton in DOT format
     *
     * @param[in] decode_ascii_chars Whether to use ASCII characters for the output.
     * @param[in] use_intervals Whether to use intervals (e.g. [1-3] instead of 1,2,3) for labels.
     * @param[in] max_label_length Maximum label length for the output (-1 means no limit, 0 means no labels).
     *  If the label is longer than @p max_label_length, it will be truncated, with full label displayed on hover.
     * @param alphabet Alphabet to use for printing labels.
     *  If nullptr, the automaton's alphabet is used.
     *  If the automaton has no alphabet, symbols are printed as integers unless @p decode_ascii_chars is set.
     * @return automaton in DOT format
     */
    std::string print_to_dot(
        bool decode_ascii_chars = false, bool use_intervals = false, int max_label_length = -1,
        const Alphabet* alphabet = nullptr) const;

    /**
     * @brief Prints the automaton to the output stream in DOT format
     *
     * @param[out] output Output stream to print the automaton to.
     * @param[in] decode_ascii_chars Whether to use ASCII characters for the output.
     * @param[in] use_intervals Whether to use intervals (e.g. [1-3] instead of 1,2,3) for labels.
     * @param[in] max_label_length Maximum label length for the output (-1 means no limit, 0 means no labels).
     *  If the label is longer than @p max_label_length, it will be truncated, with full label displayed on hover.
     * @param alphabet Alphabet to use for printing labels.
     *  If nullptr, the automaton's alphabet is used.
     *  If the automaton has no alphabet, symbols are printed as integers unless @p decode_ascii_chars is set.
     */
    void print_to_dot(
        std::ostream& output, bool decode_ascii_chars = false, bool use_intervals = false, int max_label_length = -1,
        const Alphabet* alphabet = nullptr) const;

    /**
     * @brief Prints the automaton to the file in DOT format
     * @param filename Name of the file to print the automaton to
     * @param[in] decode_ascii_chars Whether to use ASCII characters for the output.
     * @param[in] use_intervals Whether to use intervals (e.g. [1-3] instead of 1,2,3) for labels.
     * @param[in] max_label_length Maximum label length for the output (-1 means no limit, 0 means no labels).
     *  If the label is longer than @p max_label_length, it will be truncated, with full label displayed on hover.
     * @param[in] alphabet Alphabet to use for printing labels.
     *  If nullptr, the automaton's alphabet is used.
     *  If the automaton has no alphabet, symbols are printed as integers unless @p decode_ascii_chars is set.
     */
    void print_to_dot(
        const std::string& filename, bool decode_ascii_chars = false, bool use_intervals = false,
        int max_label_length = -1, const Alphabet* alphabet = nullptr) const;

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

    /**
     * @brief Prints the automaton to the file in mata format
     * @param filename Name of the file to print the automaton to
     *
     * If you need to parse the automaton again, use IntAlphabet in construct()
     *
     * TODO handle alphabet of the automaton, currently we print the exact value of the symbols
     */
    void print_to_mata(const std::string& filename) const;

    /**
     * @brief Get the set of states reachable from the given set of states over the given symbol.
     * TODO: Relict from VATA. What to do with inclusion/ universality/ this post function? Revise all of them.
     *
     * @param states Set of states to compute the post set from.
     * @param symbol Symbol to compute the post set for.
     * @param epsilon_closure_opt Epsilon closure option. Perform epsilon closure before and/or after the post operation.
     * @return Set of states reachable from the given set of states over the given symbol.
     */
    StateSet post(const StateSet& states, Symbol symbol, EpsilonClosureOpt epsilon_closure_opt = EpsilonClosureOpt::None) const;

    /**
     * @brief Get the set of states reachable from the given state over the given symbol.
     *
     * @param state A state to compute the post set from.
     * @param symbol Symbol to compute the post set for.
     * @param epsilon_closure_opt Epsilon closure option. Perform epsilon closure before and/or after the post operation.
     * @return Set of states reachable from the given state over the given symbol.
     */
    StateSet post(const State state, const Symbol symbol, const EpsilonClosureOpt epsilon_closure_opt) const {
        return post(StateSet{ state }, symbol, epsilon_closure_opt);
    }

    /**
     * @brief Returns a reference to targets (states) reachable from the given state over the given symbol.
     *
     * This is an optimized shortcut for post(state, symbol, EpsilonClosureOpt::NONE).
     *
     * @param state A state to compute the post set from.
     * @param symbol Symbol to compute the post set for.
     * @return Set of states reachable from the given state over the given symbol.
     */
    const StateSet& post(const State state, const Symbol symbol) const {
        return delta.get_successors(state, symbol);
    }


    /// Is the language of the automaton universal?
    bool is_universal(const Alphabet& alphabet, Run* cex = nullptr,
                      const ParameterMap& params = {{ "algorithm", "antichains" }}) const;
    /// Is the language of the automaton universal?
    bool is_universal(const Alphabet& alphabet, const ParameterMap& params) const;

    /**
     * @brief Check whether a run over the word (or its prefix) is in the language of an automaton.
     *
     * @param word The run to check.
     * @param use_epsilon Whether the automaton uses epsilon transitions.
     * @param match_prefix Whether to also match the prefix of the word.
     *
     * @return True if the run (or its prefix) is in the language of the automaton, false otherwise.
     */
    bool is_in_lang(const Run& word, bool use_epsilon = false, bool match_prefix = false) const;

    /**
     * @brief Check whether a word (or its prefix) is in the language of an automaton.
     *
     * @param word The word to check.
     * @param use_epsilon Whether the automaton uses epsilon transitions.
     * @param match_prefix Whether to also match the prefix of the word.
     *
     * @return True if the word (or its prefix) is in the language of the automaton, false otherwise.
     */
    bool is_in_lang(const Word& word, const bool use_epsilon = false, const bool match_prefix = false) const {
         return is_in_lang(Run{ word, {} }, use_epsilon, match_prefix);
    }

    /**
     * @brief Check whether a prefix of a run is in the language of an automaton.
     *
     * @param word The run to check.
     * @param use_epsilon Whether the automaton uses epsilon transitions.
     *
     * @return True if the prefix of the run is in the language of the automaton, false otherwise.
     */
    bool is_prefix_in_lang(const Run& word, const bool use_epsilon = false) const { return is_in_lang(word, use_epsilon, true); }

    /**
     * @brief Check whether a prefix of a word is in the language of an automaton.
     *
     * @param word The word to check.
     * @param use_epsilon Whether the automaton uses epsilon transitions.
     *
     * @return True if the prefix of the word is in the language of the automaton, false otherwise.
     */
    bool is_prefix_in_lang(const Word& word, const bool use_epsilon = false) const { return is_prefix_in_lang(Run{ word, {} }, use_epsilon); }

    /**
     * @brief Checks whether track words are in the language of the transducer.
     *
     * That is, the function checks whether a tuple @p track_words (word1, word2, word3, ..., wordn) is in the regular
     *  relation accepted by the transducer with 'n' levels (tracks).
     */
    bool is_in_lang_by_levels(const std::vector<Word>& track_words);

    std::pair<Run, bool> get_word_for_path(const Run& run) const;

    /**
     * @brief Get the set of all words in the language of the automaton whose length is <= @p max_length
     *
     * If you have an automaton with finite language (can be checked using @ref is_acyclic),
     * you can get all words by calling
     *      aut.get_words(aut.num_of_states())
     * @param max_length Maximum length of words to be returned. Default: "no limit"; will infinitely loop if the language is infinite.
     * @return Set of all words in the language of the automaton whose length is <= @p max_length.
     */
    std::set<Word> get_words(size_t max_length = std::numeric_limits<size_t>::max()) const;

    /**
     * @brief Apply @p nfa to @c this.
     *
     * Intersects @p nfa with level @p level_to_apply_on of @c this. For 2-level NFT, the default values returns the image
     * of @p nfa, where you can use to_nfa_copy() or to_nfa_move() to get NFA representation of this language. If you need
     * pre-image of @p nfa for 2-level NFT, set @p level_to_apply_on to 1.
     * @param nfa NFA to apply.
     * @param level_to_apply_on Which level to apply the @p nfa on.
     * @param project_out_applied_level Whether the @p level_to_apply_on is projected out from final NFT.
     * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
     *  is interpreted as a sequence repeating the same symbol, or as a single instance of the symbol followed by a
     *  sequence of @c DONT_CARE symbols.
     * @return
     */
    Nft apply(
        const nfa::Nfa& nfa, Level level_to_apply_on = 0, bool project_out_applied_level = true,
        JumpMode jump_mode = JumpMode::RepeatSymbol) const;

    /**
     * @brief Apply @p word to @c this.
     *
     * Intersects { @p word } with level @p level_to_apply_on of @c this. For 2-level NFT, the default values returns the image
     * of @p word, where you can use to_nfa_copy() or to_nfa_move() to get NFA representation of this language. If you need
     * pre-image of @p word for 2-level NFT, set @p level_to_apply_on to 1.
     * @param word Word to apply.
     * @param level_to_apply_on Which level to apply the @p nfa on.
     * @param project_out_applied_level Whether the @p level_to_apply_on is projected out from final NFT.
     * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
     *  is interpreted as a sequence repeating the same symbol, or as a single instance of the symbol followed by a
     *  sequence of @c DONT_CARE symbols.
     * @return
     */
    Nft apply(
        const Word& word, Level level_to_apply_on = 0, bool project_out_applied_level = true,
        JumpMode jump_mode = JumpMode::RepeatSymbol) const;

    /**
     * @brief Copy NFT as NFA.
     *
     * Transitions are not updated to only have one level.
     * @return A newly created NFA with copied members from NFT.
     */
    Nfa to_nfa_copy() const { return Nfa{ delta, initial, final, alphabet }; }

    /**
     * @brief Move NFT as NFA.
     *
     * The NFT can no longer be used.
     * Transitions are not updated to only have one level.
     * @return A newly created NFA with moved members from NFT.
     */
    Nfa to_nfa_move() { return Nfa{ std::move(delta), std::move(initial), std::move(final), alphabet }; }

    /**
     * @brief Copy NFT as NFA updating the transitions to have one level only.
     *
     * @param[in] dont_care_symbol_replacements Vector of symbols to replace @c DONT_CARE symbols with.
     * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
     * is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
     * of @c DONT_CARE symbols.
     * @return A newly created NFA with copied members from NFT with updated transitions.
     */
    Nfa to_nfa_update_copy(
        const utils::OrdVector<Symbol>& dont_care_symbol_replacements = { DONT_CARE },
        JumpMode jump_mode = JumpMode::RepeatSymbol
    ) const;

    /**
     * @brief Move NFT as NFA updating the transitions to have one level only.
     *
     * The NFT can no longer be used.
     * @param[in] dont_care_symbol_replacements Vector of symbols to replace @c DONT_CARE symbols with.
     * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
     * is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
     * of @c DONT_CARE symbols.
     * @return A newly created NFA with moved members from NFT with updated transitions.
     */
    Nfa to_nfa_update_move(
        const utils::OrdVector<Symbol>& dont_care_symbol_replacements = { DONT_CARE },
        JumpMode jump_mode = JumpMode::RepeatSymbol
    );


    /**
     * @brief Make NFT complete in place.
     *
     * For each state `state`, add transitions with "missing" symbols from @p alphabet (symbols that do not occur on
     *  transitions from given `state`) to `sink_states[next_level(level)]` where `level == this->levels[state]`.
     * If NFT does not contain any states, this function does nothing.
     *
     * @param[in] alphabet Alphabet to use for computing "missing" symbols. If @c nullptr, use @c this->alphabet when
     *  defined, otherwise use @c this->delta.get_used_symbols().
     * @param epsilons Epsilon symbols to include when computing "missing" symbols. Epsilon symbols are handled as normal
     *  alphabet symbols.
     * @param[in] sink_states The level-indexed vector of sink states, one per level, already existing in the NFT, into
     *  which new transitions are added. If @c std::nullopt, add new sink states.
     * @return @c true if a new transition was added to the NFA, @c false otherwise.
     */
    bool make_complete(
        const Alphabet* alphabet = nullptr,
        const utils::OrdVector<Symbol>& epsilons = {},
        const std::optional<std::vector<State>>& sink_states = std::nullopt
    );

    /**
     * @brief Make NFT complete in place.
     *
     * For each state `state`, add transitions with "missing" symbols from @p alphabet (symbols that do not occur on
     *  transitions from given `state`) to `sink_states[next_level(level)]` where `level == this->levels[state]`.
     * If NFT does not contain any states, this function does nothing.
     *
     * @note This overloaded version is a more efficient version which does not need to compute the set of symbols to
     *  complete to from the alphabet. Prefer this version when you already have the set of symbols precomputed or plan
     *  to complete multiple automata over the same set of symbols.
     *
     * @param[in] symbols Symbols to compute "missing" symbols from.
     * @param epsilons Epsilon symbols to include when computing "missing" symbols. Epsilon symbols are handled as normal
     *  alphabet symbols.
     * @param[in] sink_states The level-indexed vector of sink states, one per level, already existing in the NFT, into
     *  which new transitions are added. If @c std::nullopt, add new sink states.
     * @return @c true if a new transition was added to the NFA, @c false otherwise.
     */
    bool make_complete(
        const utils::OrdVector<Symbol>& symbols,
        const utils::OrdVector<Symbol>& epsilons = {},
        const std::optional<std::vector<State>>& sink_states = std::nullopt
    );

    using super::is_complete;
    using super::is_deterministic;
}; // class Nft.

// Allow variadic number of arguments of the same type.
//
// Using parameter pack and variadic arguments.
// Adapted from: https://www.fluentcpp.com/2019/01/25/variadic-number-function-parameters-type/.
/// Pack of bools for reasoning about a sequence of parameters.
template<bool...> struct BoolPack{};
/// Check that for all values in a pack @p Ts are 'true'.
template<typename... Ts> using conjunction = std::is_same<BoolPack<true,Ts::value...>, BoolPack<Ts::value..., true>>;
/// Check that all types in a sequence of parameters @p Ts are of type @p T.
template<typename T, typename... Ts> using AreAllOfType = conjunction<std::is_same<Ts, T>...>::type;

/**
 * @brief Compute non-deterministic union.
 *
 * Does not add epsilon transitions, just unites initial and final states.
 * @return Non-deterministic union of @p lhs and @p rhs.
 */
Nft union_nondet(const Nft &lhs, const Nft &rhs);

Nft union_det_complete(const Nft &lhs, const Nft &rhs) = delete;
Nft product(const Nft &lhs, const Nft &rhs, ProductFinalStateCondition final_condition,
    Symbol first_epsilon, std::unordered_map<std::pair<State,State>,State> *prod_map) = delete;

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
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence of @c DONT_CARE.
 * @param[in] lhs_first_aux_state The first auxiliary state in @p lhs. Two auxiliary states does not form a product state.
 * @param[in] rhs_first_aux_state The first auxiliary state in @p rhs. Two auxiliary states does not form a product state.
 * @return NFT as a product of NFTs @p lhs and @p rhs.
 */
Nft intersection(const Nft& lhs, const Nft& rhs,
                 std::unordered_map<std::pair<State, State>, State> *prod_map = nullptr, JumpMode jump_mode = JumpMode::RepeatSymbol,
                 State lhs_first_aux_state = Limits::max_state, State rhs_first_aux_state = Limits::max_state);

/**
 * @brief Composes two NFTs (lhs || rhs; read as "rhs after lhs").
 *
 * This function computes the composition of two NFTs, `lhs` and `rhs`, by aligning their synchronization levels.
 * Transitions between two synchronization levels are ordered as follows: first the transitions of `lhs`, then
 * the transitions of `rhs` followed by next synchronization level (if exists). By default, synchronization
 * levels are projected out from the resulting NFT.
 *
 * Vectors of synchronization levels have to be non-empty and of the same size.
 *
 * @param[in] lhs First transducer to compose.
 * @param[in] rhs Second transducer to compose.
 * @param[in] lhs_sync_levels Ordered vector of synchronization levels of the @p lhs.
 * @param[in] rhs_sync_levels Ordered vector of synchronization levels of the @p rhs.
 * @param[in] project_out_sync_levels Whether we want to project out the synchronization levels.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence of @c DONT_CARE.
 * @return A new NFT after the composition.
 */
Nft compose(const Nft& lhs, const Nft& rhs,
            const utils::OrdVector<Level>& lhs_sync_levels, const utils::OrdVector<Level>& rhs_sync_levels,
            bool project_out_sync_levels = true,
            JumpMode jump_mode = JumpMode::RepeatSymbol);

/**
 * @brief Composes two NFTs (lhs || rhs; read as "rhs after lhs").
 *
 * This function computes the composition of two NFTs, `lhs` and `rhs`, by aligning their synchronization levels.
 * Transitions between two synchronization levels are ordered as follows: first the transitions of `lhs`, then
 * the transitions of `rhs` followed by next synchronization level (if exists). By default, synchronization
 * levels are projected out from the resulting NFT.
 *
 * @param[in] lhs First transducer to compose.
 * @param[in] rhs Second transducer to compose.
 * @param[in] lhs_sync_level The synchronization level of the @p lhs.
 * @param[in] rhs_sync_level The synchronization level of the @p rhs.
 * @param[in] project_out_sync_levels Whether we wont to project out the synchronization levels.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence of @c DONT_CARE.
 * @return A new NFT after the composition.
 */
Nft compose(const Nft& lhs, const Nft& rhs, Level lhs_sync_level = 1, Level rhs_sync_level = 0, bool project_out_sync_levels = true, JumpMode jump_mode = JumpMode::RepeatSymbol);

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


#ifdef MATA_NFT_NOT_IMPLEMENTED
// TODO(nft): Implement for NFTs.
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
#endif

/**
 * @brief Determinize automaton.
 *
 * @param[in] nft Automaton to determinize.
 * @param[out] subset_map Map that maps sets of states of input automaton to states of determinized automaton.
 * @return Determinized automaton.
 */
Nft determinize(const Nft& nft, std::unordered_map<StateSet, State> *subset_map = nullptr);

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
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @p DONT_CARE symbols.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
bool is_included(const Nft& smaller, const Nft& bigger, Run* cex, const Alphabet* alphabet = nullptr, JumpMode jump_mode = JumpMode::RepeatSymbol,
                 const ParameterMap& params = {{ "algorithm", "antichains" }});

/**
 * @brief Checks inclusion of languages of two NFTs: @p smaller and @p bigger (smaller <= bigger).
 *
 * @param[in] smaller First automaton to concatenate.
 * @param[in] bigger Second automaton to concatenate.
 * @param[in] alphabet Alphabet of both NFTs to compute with.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @p DONT_CARE symbols.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
inline bool is_included(
    const Nft& smaller, const Nft& bigger, const Alphabet* const alphabet = nullptr,
    const JumpMode jump_mode = JumpMode::RepeatSymbol,
    const ParameterMap& params = { { "algorithm", "antichains" } }) {
    return is_included(smaller, bigger, nullptr, alphabet, jump_mode, params);
}

/**
 * @brief Perform equivalence check of two NFTs: @p lhs and @p rhs.
 *
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] alphabet Alphabet of both NFTs to compute with.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @p DONT_CARE symbols.
 * @param[in] params[ Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool are_equivalent(const Nft& lhs, const Nft& rhs, const Alphabet* alphabet, JumpMode jump_mode = JumpMode::RepeatSymbol,
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
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @p DONT_CARE symbols.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool are_equivalent(const Nft& lhs, const Nft& rhs, JumpMode jump_mode = JumpMode::RepeatSymbol, const ParameterMap& params = {{ "algorithm", "antichains"}});

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

/**
 * @brief Inverts the levels of the given transducer @p aut.
 *
 * The function inverts the levels of the transducer, i.e., the level 0 becomes the last level, level 1 becomes the
 * second last level, and so on.
 *
 * @param[in] aut The transducer for inverting levels.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 * is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 * of @c DONT_CARE symbols.
 * @return A new transducer with inverted levels.
 */
Nft invert_levels(const Nft& aut, JumpMode jump_mode = JumpMode::RepeatSymbol);

/**
 * @brief Remove simple epsilon transitions.
 *
 * Simple epsilon transitions are the transitions of the form
 *      q0 -epsilon-> q1 -epsilon-> q2 -epsilon-> ... -epsilon-> qn
 * where q0 and qn are level 0 states, the states in-between are states
 * with level 1, 2, ..., num_of_levels and for each qi, for 0 < i < n,
 * there is only 1 transition going to qi (the transition qi-1 -epsilon-> qi)
 * and only 1 transition going from qi (the transition qi -epsilon -> qi+1).
 * This means that if there was some state p0 going with epsilon to q1,
 * these to epsilon transitions would not be removed.
 *
 * Furthermore, this assumes that the NFT @p aut does not have jump transitions.
 *
 * The resulting automaton has the same number of states as @p aut, just the
 * transitions can change. It is recommended to run trim() after this function.
 *
 * @param aut NFT without jump transitions
 * @param epsilon symbol representing epsilon
 * @return NFT whose language is same as @p aut but does not contain simple epsilon transitions
 */
Nft remove_epsilon(const Nft& aut, Symbol epsilon = EPSILON);

/**
 * @brief Projects out specified levels @p levels_to_project in the given transducer @p nft.
 *
 * @param[in] nft The transducer for projection.
 * @param[in] levels_to_project A non-empty ordered vector of levels to be projected out from the transducer. It must
 *  contain only values that are greater than or equal to 0 and smaller than @c num_of_levels.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @p DONT_CARE symbols.
 * @return A new projected transducer.
 */
Nft project_out(const Nft& nft, const utils::OrdVector<Level>& levels_to_project, JumpMode jump_mode = JumpMode::RepeatSymbol);

/**
 * @brief Projects out specified level @p level_to_project in the given transducer @p nft.
 *
 * @param[in] nft The transducer for projection.
 * @param[in] level_to_project A level that is going to be projected out from the transducer. It has to be greater than or
 *  equal to 0 and smaller than @c num_of_levels.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE symbols.
 * @return A new projected transducer.
 */
Nft project_out(const Nft& nft, Level level_to_project, JumpMode jump_mode = JumpMode::RepeatSymbol);

/**
 * @brief Projects to specified levels @p levels_to_project in the given transducer @p nft.
 *
 * @param[in] nft The transducer for projection.
 * @param[in] levels_to_project A non-empty ordered vector of levels the transducer is going to be projected to.
 *  It must contain only values greater than or equal to 0 and smaller than @c num_of_levels.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE symbols.
 * @return A new projected transducer.
 */
Nft project_to(const Nft& nft, const utils::OrdVector<Level>& levels_to_project, JumpMode jump_mode = JumpMode::RepeatSymbol);

/**
 * @brief Projects to a specified level @p level_to_project in the given transducer @p nft.
 *
 * @param[in] nft The transducer for projection.
 * @param[in] level_to_project A level the transducer is going to be projected to. It has to be greater than or equal to 0
 *  and smaller than @c num_of_levels.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE symbols.
 * @return A new projected transducer.
 */
Nft project_to(const Nft& nft, Level level_to_project, JumpMode jump_mode = JumpMode::RepeatSymbol);

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
 * @param[in] jump_mode Specifies whether the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE symbols.
 */
Nft insert_levels(const Nft& nft, const BoolVector& new_levels_mask, JumpMode jump_mode = JumpMode::RepeatSymbol);

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
 * @param[in] jump_mode Specifies whether the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE symbols.
 */
Nft insert_level(const Nft& nft, Level new_level, JumpMode jump_mode = JumpMode::RepeatSymbol);

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
