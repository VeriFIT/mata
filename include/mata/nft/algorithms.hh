/** @file
 * @brief Functions to operate on NFTs, such as inclusion and universality checking, etc.
 */

#ifndef MATA_NFT_INTERNALS_HH_
#define MATA_NFT_INTERNALS_HH_

#include <optional>

#include "mata/simlib/util/binary_relation.hh"
#include "nft.hh"

/**
 * Concrete NFT implementations of algorithms, such as complement, inclusion, or universality checking.
 *
 * This is a separation of the implementation from the interface defined in mata::nft.
 * Note, that in mata::nft interface, there are particular dispatch functions calling
 * these function according to parameters provided by a user.
 * E.g. we can call the following function: `is_universal(aut, alph, {{'algorithm', 'antichains'}})`
 * to check for universality based on antichain-based algorithm.
 *
 * In particular, this includes algorithms for:
 *   1. Complementation,
 *   2. Inclusion,
 *   3. Universality checking,
 *   4. Intersection/concatenation with epsilon transitions, or,
 *   5. Computing relation.
 */
namespace mata::nft::algorithms {

/**
 * Brzozowski minimization of automata (revert -> determinize -> revert -> determinize).
 * @param[in] aut Automaton to be minimized.
 * @return Minimized automaton.
 */
Nft minimize_brzozowski(const Nft& aut);

/**
 * Complement implemented by determinization, adding sink state and making automaton complete. Then it adds final states
 *  which were non-final in the original automaton.
 * @param[in] aut Automaton to be complemented.
 * @param[in] symbols Symbols needed to make the automaton complete.
 * @param[in] minimize_during_determinization Whether the determinized automaton is computed by (Brzozowski)
 * minimization.
 * @return Complemented automaton.
 */
Nft complement_classical(
	const Nft& aut, const utils::OrdVector<Symbol>& symbols, bool minimize_during_determinization = false
);

/**
 * Inclusion implemented by complementation of bigger automaton, intersecting it with smaller, and then it checks
 *  emptiness of intersection.
 * @param[in] smaller Automaton which language should be included in the bigger one.
 * @param[in] bigger Automaton which language should include the smaller one.
 * @param[in] alphabet Alphabet of both automata (it is computed automatically, but it is more efficient to set it if
 *  you have it).
 * @param[out] cex A potential counterexample word which breaks inclusion
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE.
 * @return True if smaller language is included, i.e., if the final intersection of smaller complement of bigger is
 * empty.
 * @throw std::invalid_argument when the level vector of an operand does not cover all its states or holds a level
 *  that is out of range.
 */
bool is_included_naive(
	const Nft& smaller,
	const Nft& bigger,
	const Alphabet* alphabet = nullptr,
	Run* cex = nullptr,
	JumpMode jump_mode = JumpMode::RepeatSymbol
);

/**
 * Inclusion implemented by antichain algorithms.
 *
 * The operands are read as the flat automata that @c Nft::unwind_jumps() with the DONT_CARE replacements below would
 *  produce, EPSILON being an ordinary letter. When unwinding would spell DONT_CARE out over the replacements (on a
 *  DONT_CARE transition, or on the links padding a jump under @c JumpMode::AppendDontCares), the jumps and DONT_CAREs
 *  are explored lazily during the search instead: no operand is unwound and DONT_CARE is not expanded over the
 *  alphabet. The answer is that of @c is_included_antichains_unwound().
 * @param[in] smaller Automaton which language should be included in the bigger one
 * @param[in] bigger Automaton which language should include the smaller one
 * @param[in] alphabet Alphabet whose symbols replace @c DONT_CARE; @c nullptr for the symbols of both automata
 *  without @c DONT_CARE.
 * @param[out] cex A potential counterexample word which breaks inclusion, one symbol per level. When the jumps are
 *  explored lazily, the positions of @c cex->path inside a jump hold @c Limits::max_state.
 * @param[out] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE.
 * @return True if smaller language is included, i.e., if the final intersection of smaller complement of bigger is
 * empty.
 * @throw std::invalid_argument when the level vector of an operand does not cover all its states or holds a level
 *  that is out of range.
 */
bool is_included_antichains(
	const Nft& smaller,
	const Nft& bigger,
	const Alphabet* alphabet = nullptr,
	Run* cex = nullptr,
	JumpMode jump_mode = JumpMode::RepeatSymbol
);

/**
 * Inclusion implemented by unwinding both automata with @c Nft::to_nfa_update_copy() and running the NFA antichain
 *  inclusion on the results; the reference for @c is_included_antichains().
 * @param[in] smaller Automaton which language should be included in the bigger one
 * @param[in] bigger Automaton which language should include the smaller one
 * @param[in] alphabet Alphabet whose symbols replace @c DONT_CARE; @c nullptr for the symbols of both automata
 *  without @c DONT_CARE.
 * @param[out] cex A potential counterexample word which breaks inclusion, and a path through the unwound @p smaller.
 * @param[out] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 *  of @c DONT_CARE.
 * @return True if smaller language is included.
 * @throw std::invalid_argument when the level vector of an operand does not cover all its states or holds a level
 *  that is out of range.
 */
bool is_included_antichains_unwound(
	const Nft& smaller,
	const Nft& bigger,
	const Alphabet* alphabet = nullptr,
	Run* cex = nullptr,
	JumpMode jump_mode = JumpMode::RepeatSymbol
);

/**
 * Universality check implemented by checking emptiness of complemented automaton
 * @param[in] aut Automaton which universality is checked
 * @param[in] alphabet Alphabet of the automaton
 * @param[out] cex Counterexample word which eventually breaks the universality
 * @return True if the complemented automaton has non-empty language, i.e., the original one is not universal
 */
bool is_universal_naive(const Nft& aut, const Alphabet& alphabet, Run* cex);

/**
 * Universality checking based on subset construction with antichain.
 * @param[in] aut Automaton which universality is checked
 * @param[in] alphabet Alphabet of the automaton
 * @param[out] cex Counterexample word which eventually breaks the universality
 * @return True if the automaton is universal, otherwise false.
 */
bool is_universal_antichains(const Nft& aut, const Alphabet& alphabet, Run* cex);

Simlib::Util::BinaryRelation compute_relation(
	const Nft& aut, const ParameterMap& params = {{"relation", "simulation"}, {"direction", "forward"}}
);

/**
 * @brief Compute product of two NFTs, final condition is to be specified.
 *
 * @param[in] lhs First NFT to compute intersection for.
 * @param[in] rhs Second NFT to compute intersection for.
 * @param[in] final_condition The predicate that tells whether a pair of states is final (conjunction for intersection).
 * @param[out] product_map Can be used to get the mapping of the pairs of the original states to product states.
 *   Mostly useless, it is only filled in and returned if !=nullptr, but the algorithm internally uses another data
 * structures, because this one is too slow.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 * of @c DONT_CARE.
 * @param[in] lhs_first_aux_state The first auxiliary state in @p lhs. Two auxiliary states can not form a product
 * state.
 * @param[in] rhs_first_aux_state The first auxiliary state in @p rhs. Two auxiliary states con not form a product
 * state.
 * @return NFT as a product of NFTs @p lhs and @p rhs with ε handled as regular symbols.
 */
Nft product(
	const Nft& lhs,
	const Nft& rhs,
	const std::function<bool(State, State)>&& final_condition,
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>>* product_map = nullptr,
	JumpMode jump_mode = JumpMode::RepeatSymbol,
	State lhs_first_aux_state = Limits::max_state,
	State rhs_first_aux_state = Limits::max_state
);

/**
 * @brief Concatenate two NFTs.
 *
 * Connects @p lhs and @p rhs by adding a run of ε-transitions, one per level, from each final state of @p lhs to
 * each initial state of @p rhs. The ε is set to @p epsilon. If @p use_epsilon is false, @c remove_epsilon(@p
 * epsilon) is run on the result, so every transition over @p epsilon disappears, including the operands' own
 * transitions labelled with that symbol. Operand transitions over any other symbol are preserved; in particular,
 * with @p epsilon != @c EPSILON the operands' @c EPSILON transitions survive, which is not the case when
 * @p epsilon == @c EPSILON.
 *
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] epsilon Symbol used for the connecting transitions between @p lhs and @p rhs.
 * @param[in] use_epsilon Whether to keep the connecting epsilon transitions in the result.
 * @param[out] lhs_state_renaming Map mapping lhs states to result states.
 * @param[out] rhs_state_renaming Map mapping rhs states to result states.
 * @return Concatenated automaton.
 */
Nft concatenate_eps(
	const Nft& lhs,
	const Nft& rhs,
	const Symbol& epsilon,
	bool use_epsilon = false,
	StateRenaming* lhs_state_renaming = nullptr,
	StateRenaming* rhs_state_renaming = nullptr
);

/**
 * @brief Concatenate two NFTs with epsilon transitions, consuming the left operand.
 *
 * Same result as the copying overload. The left operand's state posts are moved into the result; the right
 * operand is copied normally. If @p lhs and @p rhs are aliased, a controlled snapshot of @p rhs is taken before
 * @p lhs is consumed. @p lhs is left valid but empty after the operation.
 */
Nft concatenate_eps(
	Nft&& lhs,
	const Nft& rhs,
	const Symbol& epsilon,
	bool use_epsilon = false,
	StateRenaming* lhs_state_renaming = nullptr,
	StateRenaming* rhs_state_renaming = nullptr
);

/**
 * @brief Concatenate two NFTs with epsilon transitions, consuming both operands.
 *
 * Same result as the copying overload. The state posts of both operands are moved into the result. If @p lhs and
 * @p rhs are aliased, a controlled snapshot of @p rhs is taken before @p lhs is consumed, so the result holds both
 * copies. Both @p lhs and @p rhs are left valid but empty after the operation.
 */
Nft concatenate_eps(
	Nft&& lhs,
	Nft&& rhs,
	const Symbol& epsilon,
	bool use_epsilon = false,
	StateRenaming* lhs_state_renaming = nullptr,
	StateRenaming* rhs_state_renaming = nullptr
);

/**
 * @brief The common level layout of the general composition of two NFTs.
 *
 * The aligned levels come in blocks, one per synchronization level and one after the last: the levels only @c lhs
 *  reads, then the levels only @c rhs reads, then the synchronization level, which the last block lacks. The result
 *  of the composition has the aligned levels in this order, without the synchronization levels when they are
 *  projected out. Example:
 *
 *                 LHS                         |                    RHS
 * --------------------------------------------+---------------------------------------------
 *            num_of_levels = 5                |             num_of_levels = 4
 *          sync_levels = { 2, 4 }             |            sync_levels = { 1, 2 }
 * --------------------------------------------+---------------------------------------------
 *  positions: lhs 0, lhs 1, rhs 0, sync (2, 1), lhs 3, sync (4, 2), rhs 3
 *  lhs_new_levels_mask() = { 0, 0, 1, 0, 0, 0, 1 }, rhs_new_levels_mask() = { 1, 1, 0, 0, 1, 0, 0 }
 */
struct CompositionSchedule {
	/// Which operand reads an aligned level.
	enum class Owner : uint8_t {
		Lhs, ///< Only @c lhs reads it; @c rhs is not inserted into it.
		Rhs, ///< Only @c rhs reads it.
		Sync, ///< Both read it: a synchronization level.
	};
	/// An aligned level.
	struct Position {
		Owner owner;
		/// The level of the @c lhs state when this aligned level is read: the level @c lhs reads here, or, where only
		///  @c rhs reads, the next level @c lhs reads (0 once it has read its last level of the round).
		Level lhs_level;
		/// The level of the @c rhs state when this aligned level is read, as @c lhs_level.
		Level rhs_level;
		bool operator==(const Position&) const = default;
	};

	/// The aligned levels, in the order of the result levels.
	std::vector<Position> positions{};

	/**
	 * @brief Lay out the composition of an NFT with @p lhs_num_of_levels levels and one with @p rhs_num_of_levels
	 *  levels on the given synchronization levels, which must be valid (see @c compose()).
	 */
	CompositionSchedule(
		size_t lhs_num_of_levels,
		size_t rhs_num_of_levels,
		const utils::OrdVector<Level>& lhs_sync_levels,
		const utils::OrdVector<Level>& rhs_sync_levels
	);

	/// The mask of the aligned levels @c insert_levels() inserts into @c lhs: those only @c rhs reads.
	BoolVector lhs_new_levels_mask() const;
	/// The mask of the aligned levels @c insert_levels() inserts into @c rhs: those only @c lhs reads.
	BoolVector rhs_new_levels_mask() const;
	/// The aligned synchronization levels.
	utils::OrdVector<Level> sync_positions() const;
};

/**
 * @brief Composes two NFTs.
 *
 * This function computes the composition of two NFTs, `lhs` and `rhs`, by aligning their synchronization levels.
 * Vectors of synchronization levels must be non-empty and of the same size. The levels of the resulting NFT
 * are formed, iteratively, from the non-synchronizing levels of `lhs`, followed by the non-synchronizing levels of
 * `rhs`, followed by the next synchronizing level (if not projected out by setting project_out_sync_levels to true).
 * For example, given `lhs` with 7 levels, `rhs` with 8 levels, lhs_sync_levels=<0,2,5>, rhs_sync_levels=<1,4,5>, the
 * resulting NFT levels will correspond to (assuming that we are not projecting out the synchronized levels)
 *   - `rhs` level 0
 *   - synchronized level from level 0 of `lhs` and level 1 of `rhs`
 *   - `lhs` level 1
 *   - `rhs` levels 2 and 3
 *   - synchronized level from level 2 of `lhs` and level 4 of `rhs`
 *   - `lhs` levels 3 and 4
 *   - synchronized level from level 5 of `lhs` and level 5 of `rhs`
 *   - `lhs` level 6
 *   - `rhs` levels 6 and 7
 *
 * NOTE: If you have only a single synchronization level per NFT, consider @c compose_fast() for better performance; it
 *       reads jump transitions as well.
 *
 * @param[in] lhs First transducer to compose.
 * @param[in] rhs Second transducer to compose.
 * @param[in] lhs_sync_levels Ordered vector of synchronization levels of the @p lhs.
 * @param[in] rhs_sync_levels Ordered vector of synchronization levels of the @p rhs.
 * @param[in] project_out_sync_levels Whether we want to project out the synchronization levels.
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence
 * of @c DONT_CARE.
 * @param[in] normalization Only @c CompositionNormalization::Full changes the result: the general composition
 *  generates no states of its own for @c GeneratedOnly to rewrite.
 *
 * Operands whose transitions all go to a later level of their round or end it are composed by
 *  @c compose_general_direct() (with more than one aligned level), all others by the pipeline of
 *  @c compose_general_reference(); both accept the same relation.
 *
 * @return A new NFT after the composition.
 * @throws std::invalid_argument When the request is invalid; the same checks as @c nft::compose() runs, so calling
 *  this entry point directly does not skip them.
 */
Nft compose_general(
	const Nft& lhs,
	const Nft& rhs,
	const utils::OrdVector<Level>& lhs_sync_levels,
	const utils::OrdVector<Level>& rhs_sync_levels,
	bool project_out_sync_levels = true,
	JumpMode jump_mode = JumpMode::RepeatSymbol,
	CompositionNormalization normalization = CompositionNormalization::Auto
);

/**
 * @brief The general composition by a pipeline: copies of the operands padded to the common levels by
 *  @c insert_levels(), a whole-round self-loop at each of their zero-level states, their @c product(), and
 *  @c project_out() of the synchronization levels.
 *
 * The reference and the fallback of @c compose_general_direct(); takes the arguments of @c compose_general().
 */
Nft compose_general_reference(
	const Nft& lhs,
	const Nft& rhs,
	const utils::OrdVector<Level>& lhs_sync_levels,
	const utils::OrdVector<Level>& rhs_sync_levels,
	bool project_out_sync_levels = true,
	JumpMode jump_mode = JumpMode::RepeatSymbol,
	CompositionNormalization normalization = CompositionNormalization::Auto
);

/**
 * @brief The general composition by a direct traversal of the aligned configurations of the two NFTs.
 *
 * Builds a result with the relation of @c compose_general_reference(), EPSILON included, without padding copies of the
 * operands: an operand reads its own levels and waits on the others, and a jump transition is read level by level.
 *  It covers operands whose transitions all go to a later level of the round or end it, under every @p jump_mode,
 *  with more than one aligned level. DONT_CARE matches every symbol of the other operand, EPSILON included, as in
 *  @c product() (#561); an operand the pipeline inserts no level into may choose its jump again inside it under
 *  @c JumpMode::RepeatSymbol and @c JumpMode::NoJump, as @c product() lets it.
 *
 * @return The composition, or @c std::nullopt for operands outside that domain.
 * @throws std::invalid_argument When the request is invalid, as @c compose_general().
 */
std::optional<Nft> compose_general_direct(
	const Nft& lhs,
	const Nft& rhs,
	const utils::OrdVector<Level>& lhs_sync_levels,
	const utils::OrdVector<Level>& rhs_sync_levels,
	bool project_out_sync_levels = true,
	JumpMode jump_mode = JumpMode::RepeatSymbol,
	CompositionNormalization normalization = CompositionNormalization::Auto
);

/**
 * @brief Composes two NFTs with a single synchronization level, reading their jump transitions in @p jump_mode.
 *
 * The levels of the resulting NFT are in the following order:
 *  - levels of `lhs` before its synchronization level
 *  - levels of `rhs` before its synchronization level
 *  - the synchronizing level (possibly missing if project_out_sync_levels is true)
 *  - levels of `lhs` after its synchronization level
 *  - levels of `rhs` after its synchronization level
 *
 * A jump transition is read lazily, one level at a time: the jumps of a symbol post share a cursor per level they cover
 *  after their first one, which reads the symbol of the jump under @c JumpMode::RepeatSymbol and DONT_CARE under
 *  @c JumpMode::AppendDontCares, and a configuration of the composition pairs such cursors as it pairs states. A level
 *  inside a jump thus synchronizes on the symbol the jump reads there. The operands are not copied, and DONT_CARE is
 *  not spelled out. Under @c JumpMode::RepeatSymbol, an EPSILON transition between two zero-level states is a whole
 *  round of EPSILON, as under @c JumpMode::NoJump.
 *
 * The result has no jump transitions: a jump is unwound into one transition per level, through the result states of
 *  the configurations with its cursors, so it reads the same in every jump mode. Its relation is the one of
 *  @c compose_fast_no_jump() on the operands with their jumps unwound in @p jump_mode (DONT_CARE kept), which is the
 *  relation of @c compose_general() whenever the operands have no EPSILON or DONT_CARE, no jump of @p jump_mode
 *  @c JumpMode::AppendDontCares reads DONT_CARE on a synchronization level, and, under @c JumpMode::RepeatSymbol,
 *  neither operand with a jump is composed with a one-level operand. The two compositions place EPSILON differently,
 *  and @c compose_general() matches DONT_CARE with EPSILON (#561) and lets an operand composed with a one-level
 *  operand choose its jump again inside it.
 *
 * @param[in] lhs First transducer to compose.
 * @param[in] rhs Second transducer to compose.
 * @param[in] lhs_sync_level The synchronization level of the @p lhs.
 * @param[in] rhs_sync_level The synchronization level of the @p rhs.
 * @param[in] project_out_sync_levels Whether we want to project out the synchronization levels.
 * @param[in] jump_mode How a jump transition reads the levels after its first one. With @c JumpMode::NoJump, the
 *  operands must not have jump transitions other than fast EPSILON transitions, as in @c compose_fast_no_jump().
 * @param[in] normalization How to normalize the result after the construction; see @c CompositionNormalization.
 *
 * @return A new NFT after the composition.
 * @throws std::invalid_argument When a synchronization level is out of range for its operand, when the composition
 *  would leave no levels, or, under a jump mode other than @c JumpMode::NoJump, when a transition goes to an earlier
 *  level of its round. The no-jump shape of the operands under @c JumpMode::NoJump is only checked by
 *  @c MATA_ASSERT.
 */
Nft compose_fast(
	const Nft& lhs,
	const Nft& rhs,
	Level lhs_sync_level,
	Level rhs_sync_level,
	bool project_out_sync_levels,
	JumpMode jump_mode,
	CompositionNormalization normalization = CompositionNormalization::Auto
);

/**
 * @brief Composes two NFTs with a single synchronization level and no jumps: @c compose_fast() under
 *  @c JumpMode::NoJump.
 *
 * @param[in] lhs First transducer to compose.
 * @param[in] rhs Second transducer to compose.
 * @param[in] lhs_sync_level The synchronization level of the @p lhs.
 * @param[in] rhs_sync_level The synchronization level of the @p rhs.
 * @param[in] project_out_sync_levels Whether we wont to project out the synchronization levels.
 * @param[in] normalization How to normalize the result after the construction; see @c CompositionNormalization.
 *
 * @return A new NFT after the composition.
 * @throws std::invalid_argument When a synchronization level is out of range for its operand, or when the
 *  composition would leave no levels. The no-jump shape of the operands is only checked by @c MATA_ASSERT.
 */
Nft compose_fast_no_jump(
	const Nft& lhs,
	const Nft& rhs,
	Level lhs_sync_level = 1,
	Level rhs_sync_level = 0,
	bool project_out_sync_levels = true,
	CompositionNormalization normalization = CompositionNormalization::Auto
);

} // namespace mata::nft::algorithms

#endif
