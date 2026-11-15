/** @file
 * @brief Composition of two NFTs.
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <format>
#include <functional>
#include <optional>
#include <queue>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mata/nft/algorithms.hh"
#include "mata/nft/nft.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/two-dimensional-map.hh"

using namespace mata::utils;

namespace {
using mata::Symbol;
using namespace mata::nft;

/**
 * Enum class for a state flag to indicate which synchronization
 * types (synchronization on epsilon or symbol) can be reached from the state.
 * The synchronization type helps to determine if a state from lhs can
 * at some point in the next levels synchronize with a state from rhs.
 * The following table shows all possible combinations of synchronization types
 * for lhs and rhs states, and whether they can synchronize or if one of them will wait
 * because there is an EPSILON on the synchronization in the other that it
 * could not synchronize with.
 *
 * You can notice that we do not want to synchronize on EPSILONs.
 * This is a part of the optimization to avoid creating redundant EPSILON transitions.
 * For more details, see https://cs.nyu.edu/~mohri/pub/nway.pdf Figure 3.
 *                       ||                   RHS sync type
 *    LHS sync type      ||  OnlyOnSymbol  |  OnlyOnEpsilon  |  OnEpsilonAndSymbol
 * ======================||================|=================|======================
 *                       ||   synchronize  |                 |      synchronize
 *      OnlyOnSymbol     ||                |   wait on LHS   |      wait on LHS
 *                       ||                |                 |
 * ----------------------||----------------|-----------------|----------------------
 *                       ||                |                 |
 *     OnlyOnEpsilon     ||                |   wait on LHS   |      wait on LHS
 *                       ||   wait on RHS  |   wait on RHS   |      wait on RHS
 * ----------------------||----------------|-----------------|----------------------
 *                       ||   synchronize  |                 |      synchronize
 *  OnEpsilonAndSymbol   ||                |   wait on LHS   |      wait on LHS
 *                       ||   wait on RHS  |   wait on RHS   |      wait on RHS
 * =================================================================================
 *
 * A state with the type Undefined neither synchronizes nor makes the other one wait.
 */
enum class SynchronizationType : uint8_t {
	/// No synchronization can be reached from the state: it has passed its synchronization level, or it is on or before
	///  it but cannot reach one (a dead end). Only the level of the state tells the two apart.
	Undefined = 0b0000'0000,
	OnlyOnSymbol = 0b0000'0001, ///< Synchronization on symbol.
	OnlyOnEpsilon = 0b0000'0010, ///< Synchronization on EPSILON.
	OnEpsilonAndSymbol = 0b0000'0011, ///< Synchronization on EPSILON and symbol.
	UnderComputation = 0b0000'0100 ///< The synchronization is being computed (only a helper value).
};
inline SynchronizationType operator|(SynchronizationType lhs, SynchronizationType rhs) {
	return static_cast<SynchronizationType>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}
inline SynchronizationType& operator|=(SynchronizationType& lhs, SynchronizationType rhs) {
	lhs = lhs | rhs;
	return lhs;
}
inline bool exist_intersection(SynchronizationType lhs, SynchronizationType rhs) {
	return (static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs)) != 0;
}
/// Whether a state of each type can reach a synchronization on a symbol, see the table above.
inline bool exist_synchronization(SynchronizationType lhs, SynchronizationType rhs) {
	return exist_intersection(lhs, SynchronizationType::OnlyOnSymbol) &&
		   exist_intersection(rhs, SynchronizationType::OnlyOnSymbol);
}

/**
 * @brief Filter state of the EPSILON-composition filter of Mohri (https://cs.nyu.edu/~mohri/pub/nway.pdf).
 *
 * The unfiltered composer explores every interleaving of independent one-sided moves of the operands,
 * so n silent EPSILON moves on each side already build n^2 configurations for a relation that is a
 * single tuple. The filter state is a part of the composition key of two zero-level states and keeps
 * only the canonical interleaving, in which every silent LHS-only move comes before any silent
 * RHS-only move. A one-sided move is *silent* if it emits EPSILONs only, i.e., it is a fast EPSILON
 * transition or a whole-round EPSILON move of a one-level NFT. Silent moves of either side commute
 * even on the flattened word of the result: each of them appends the same count of EPSILONs and its
 * availability depends only on its own side's state, so every accepted interleaving has a canonical
 * representative with the very same word, and pruning the rest does not change the language of the
 * result. One-sided moves emitting real symbols (multi-level waiting rounds) do not commute on the
 * flattened word, so they are not reordered: they always stay allowed and reset the filter to
 * LhsMayMoveAlone.
 */
enum class EpsilonFilterMode : uint8_t {
	LhsMayMoveAlone = 0, ///< No silent RHS-only move happened yet; both sides may move alone.
	RhsMovedAlone = 1, ///< A silent RHS-only move happened; silent LHS-only moves are refused.
};
constexpr size_t NUMBER_OF_EPSILON_FILTER_MODES = 2;

/// A one-sided move of a composition step, classified by the EPSILON it does or does not emit.
enum class EpsilonFilterAction : uint8_t {
	SilentLhsOnlyMove, ///< A fast EPSILON of the LHS or a waiting round of a one-level LHS.
	NonSilentLhsOnlyMove, ///< A waiting round of a multi-level LHS.
	SilentRhsOnlyMove, ///< A fast EPSILON of the RHS or a waiting round of a one-level RHS.
	NonSilentRhsOnlyMove, ///< A waiting round of a multi-level RHS.
};

/**
 * @brief Compute the next filter mode for a one-sided move, or refuse the move.
 *
 * @param mode The filter mode of the source configuration.
 * @param action The kind of the one-sided move being taken.
 * @param lhs_has_a_silent_lhs_only_move Whether the LHS state can still move alone silently
 *                                       (a fast EPSILON move of it or a waiting round of a
 *                                       one-level LHS exists).
 * @param lhs_gets_stuck_without_moving Whether every move of the LHS state is a silent LHS-only
 *                                      move and the LHS state is not final, so that the LHS could
 *                                      never move again once the filter refuses its silent moves.
 * @return The filter mode of the target configuration, or std::nullopt if the move is refused.
 */
inline std::optional<EpsilonFilterMode> epsilon_filter_step(
	const EpsilonFilterMode mode,
	const EpsilonFilterAction action,
	const bool lhs_has_a_silent_lhs_only_move,
	const bool lhs_gets_stuck_without_moving
) {
	if (action == EpsilonFilterAction::NonSilentLhsOnlyMove || action == EpsilonFilterAction::NonSilentRhsOnlyMove) {
		// A move emitting real symbols is not reordered; it starts a fresh interleaving.
		return EpsilonFilterMode::LhsMayMoveAlone;
	}
	if (action == EpsilonFilterAction::SilentLhsOnlyMove) {
		// The canonical schedule runs every silent LHS-only move before any silent RHS-only move,
		// so once the RHS has moved alone silently, a silent LHS-only move is refused from this
		// configuration; its canonical counterpart came before the silent RHS-only moves.
		if (mode == EpsilonFilterMode::RhsMovedAlone) { return std::nullopt; }
		return EpsilonFilterMode::LhsMayMoveAlone;
	}
	if (mode == EpsilonFilterMode::RhsMovedAlone) { return EpsilonFilterMode::RhsMovedAlone; }
	if (lhs_gets_stuck_without_moving) {
		// After switching the filter, the LHS could never move again, so the move would be dead.
		return std::nullopt;
	}
	// If the LHS has no silent LHS-only move at all, the two filter modes coincide for it;
	// keep the LhsMayMoveAlone mode instead of splitting the pair by the mode to no effect.
	return lhs_has_a_silent_lhs_only_move ? EpsilonFilterMode::RhsMovedAlone : EpsilonFilterMode::LhsMayMoveAlone;
}

/// Whether @p state has a fast EPSILON transition (an EPSILON transition to another zero-level state)
/// in @p nft; @p state must be a zero-level state itself. @p Operand is an @c Nft or a @c CursorOperand.
template <typename Operand> bool has_fast_epsilon_move(const Operand& nft, const State state) {
	if (nft.levels.num_of_levels == 1) { return false; }
	const auto epsilon_post_it = nft.delta[state].find(EPSILON);
	if (epsilon_post_it == nft.delta[state].end()) { return false; }
	return std::ranges::any_of(epsilon_post_it->targets, [&nft](const State target) {
		return nft.levels[target] == 0;
	});
}

/// Whether every outgoing transition of @p state is a silent LHS-only move, i.e., an EPSILON
/// transition to another zero-level state (a fast EPSILON transition), or any EPSILON transition
/// of a one-level NFT; @p state must be a zero-level state of the LHS. A state with no outgoing
/// transition at all trivially qualifies.
template <typename Operand> bool has_only_silent_lhs_only_moves(const Operand& nft, const State state) {
	const StatePost& state_post = nft.delta[state];
	for (const SymbolPost& symbol_post : state_post) {
		if (symbol_post.symbol != EPSILON) { return false; }
		if (nft.levels.num_of_levels != 1 &&
			std::ranges::any_of(symbol_post.targets, [&nft](const State target) { return nft.levels[target] != 0; })) {
			// An EPSILON on the synchronization level starts a non-silent waiting round of a
			// multi-level NFT, which the filter does not reorder.
			return false;
		}
	}
	return true;
}

/// Whether @p rhs can ever make a silent RHS-only move: a fast EPSILON transition, or a waiting round of a one-level
/// RHS. Only such a move switches the EPSILON-composition filter, so without one the filter keeps every configuration
/// in the LhsMayMoveAlone mode and refuses nothing.
template <typename Operand> bool can_move_alone_silently(const Operand& rhs) {
	if (rhs.levels.num_of_levels == 1) { return true; }
	const size_t num_of_states{rhs.num_of_states()};
	for (State state{0}; state < num_of_states; ++state) {
		if (rhs.levels[state] == 0 && has_fast_epsilon_move(rhs, state)) { return true; }
	}
	return false;
}

/**
 * @brief An operand of the fast composition that reads its jump transitions level by level, without unwinding it.
 *
 * A place is a state of the NFT or a cursor inside the jumps of a symbol post, numbered after the states. The jumps of
 *  a symbol post share one cursor per level they cover after their first one: on its level the cursor reads the
 *  symbol of the post under @c JumpMode::RepeatSymbol and DONT_CARE under @c JumpMode::AppendDontCares, and leads to
 *  the targets of the jumps that end on the next level and to the cursor of the next level if a jump ends later. The
 *  composition pairs places as it pairs states, so a configuration with a cursor carries what remains of the jump its
 *  operand is in. A state with a jump reads a post of its own, in which the cursor of the next level replaces the
 *  targets of the jumps; every other state reads its post in the NFT, which is not copied.
 *
 * The composer reads a place through the members it reads an @c Nft by. Under @c JumpMode::RepeatSymbol, an EPSILON
 *  transition between two zero-level states of a multi-level NFT is the fast EPSILON transition the composer reads as a
 *  whole round of EPSILON anyway, so it gets no cursor.
 */
class CursorOperand {
  public:
	/// The posts of the places.
	struct PlacePosts {
		std::vector<const StatePost*> posts{};
		const StatePost& operator[](const State place) const { return *posts[place]; }
	};
	/// The levels of the places.
	struct PlaceLevels {
		std::vector<Level> levels{};
		size_t num_of_levels{0};
		Level operator[](const State place) const { return levels[place]; }
	};

	PlacePosts delta{};
	PlaceLevels levels{};
	const mata::utils::SparseSet<State>& initial;
	const mata::utils::SparseSet<State>& final; ///< No cursor is final: the cursors are numbered after the states.

	/// @throws std::invalid_argument When a transition of @p nft goes to an earlier level of its round.
	CursorOperand(const Nft& nft, const JumpMode jump_mode) : initial{nft.initial}, final{nft.final} {
		MATA_ASSERT(jump_mode != JumpMode::NoJump);
		const size_t num_of_states{nft.num_of_states()};
		const size_t num_of_levels{nft.levels.num_of_levels};
		levels.num_of_levels = num_of_levels;
		levels.levels.assign(nft.levels.begin(), nft.levels.end());
		levels.levels.resize(num_of_states, 0);
		delta.posts.resize(num_of_states);
		// The level a transition into @p target ends on, num_of_levels for a zero-level target.
		const auto end_of{[&](const State target) -> size_t {
			return levels.levels[target] == 0 ? num_of_levels : levels.levels[target];
		}};
		for (State source{0}; source < num_of_states; ++source) {
			const StatePost& state_post{nft.delta[source]};
			delta.posts[source] = &state_post;
			const Level source_level{levels.levels[source]};
			const auto is_jump{[&](const Symbol symbol, const State target) {
				const size_t end{end_of(target)};
				if (end <= source_level) {
					throw std::invalid_argument(
						std::format(
							"compose_fast: the transition {} -{}-> {} goes from level {} back to level {}.", source,
							symbol, target, source_level, end
						)
					);
				}
				return end > source_level + 1u && !(jump_mode == JumpMode::RepeatSymbol && symbol == EPSILON &&
													end == num_of_levels && source_level == 0);
			}};
			const bool has_jump{std::ranges::any_of(state_post, [&](const SymbolPost& symbol_post) {
				return std::ranges::any_of(symbol_post.targets, [&](const State target) {
					return is_jump(symbol_post.symbol, target);
				});
			})};
			if (!has_jump) { continue; }
			StatePost& own_post{own_posts_.emplace_back()};
			for (const SymbolPost& symbol_post : state_post) {
				SymbolPost own_symbol_post{symbol_post.symbol};
				size_t last_end{source_level + 1u};
				for (const State target : symbol_post.targets) {
					if (is_jump(symbol_post.symbol, target)) {
						last_end = std::max(last_end, end_of(target));
					} else {
						own_symbol_post.targets.push_back(target);
					}
				}
				if (last_end > source_level + 1u) {
					const Symbol cursor_symbol{jump_mode == JumpMode::RepeatSymbol ? symbol_post.symbol : DONT_CARE};
					// The cursor of each level is the place after the one of the previous level.
					const State first_cursor{static_cast<State>(levels.levels.size())};
					own_symbol_post.targets.push_back(first_cursor);
					for (size_t level{source_level + 1u}; level < last_end; ++level) {
						SymbolPost cursor_symbol_post{cursor_symbol};
						for (const State target : symbol_post.targets) {
							if (end_of(target) == level + 1 && is_jump(symbol_post.symbol, target)) {
								cursor_symbol_post.targets.push_back(target);
							}
						}
						if (level + 1 < last_end) {
							cursor_symbol_post.targets.push_back(static_cast<State>(levels.levels.size() + 1));
						}
						StatePost& cursor_post{own_posts_.emplace_back()};
						cursor_post.push_back(std::move(cursor_symbol_post));
						delta.posts.push_back(&cursor_post);
						levels.levels.push_back(static_cast<Level>(level));
					}
				}
				own_post.push_back(std::move(own_symbol_post));
			}
			delta.posts[source] = &own_post;
		}
	}

	size_t num_of_states() const { return levels.levels.size(); }

  private:
	std::deque<StatePost>
		own_posts_{}; ///< The posts of the states with a jump and of the cursors, at stable addresses.
};

/// A state pair of the composition worklist with its state in the result, so that popping it needs no lookup in the
/// map of composition states, and the EPSILON-composition filter mode it was reached in when @p UseFilter is set.
template <bool UseFilter> struct CompositionKey {
	State lhs_state;
	State rhs_state;
	State composition_state;
	EpsilonFilterMode epsilon_filter_mode;

	CompositionKey(
		const State lhs_state,
		const State rhs_state,
		const State composition_state,
		const EpsilonFilterMode epsilon_filter_mode
	)
		: lhs_state{lhs_state},
		  rhs_state{rhs_state},
		  composition_state{composition_state},
		  epsilon_filter_mode{epsilon_filter_mode} {}
	EpsilonFilterMode mode() const { return epsilon_filter_mode; }
};

/// Without the filter, every configuration stays in the LhsMayMoveAlone mode, so the key does not store it.
template <> struct CompositionKey<false> {
	State lhs_state;
	State rhs_state;
	State composition_state;

	CompositionKey(
		const State lhs_state,
		const State rhs_state,
		const State composition_state,
		[[maybe_unused]] const EpsilonFilterMode mode
	)
		: lhs_state{lhs_state},
		  rhs_state{rhs_state},
		  composition_state{composition_state} {
		MATA_ASSERT(mode == EpsilonFilterMode::LhsMayMoveAlone);
	}
	static constexpr EpsilonFilterMode mode() { return EpsilonFilterMode::LhsMayMoveAlone; }
};

/**
 * @brief A class to hold properties related to synchronization during the composition of NFTs.
 * This simplifies passing multiple synchronization-related parameters.
 */
template <typename Operand> class SynchronizationProperties {
  public:
	// A stack-allocated helper that borrows the composed transducer for its lifetime; it is never copied nor assigned.
	const Operand& nft;
	Level sync_level;
	size_t num_of_levels_before_sync;
	size_t num_of_levels_after_sync;
	std::vector<SynchronizationType> sync_types_vec;
	/// For each state on or before its synchronization level, a signature of the symbols but EPSILON it can read on
	///  that level: one of 64 bits per symbol, all of them for @c DONT_CARE, which matches every symbol but EPSILON.
	///  Two states whose signatures do not intersect read no symbol both can synchronize on.
	std::vector<uint64_t> sync_symbols_vec;

	SynchronizationProperties(const Operand& nft, const Level sync_level)
		: nft(nft),
		  sync_level(sync_level),
		  num_of_levels_before_sync(sync_level),
		  num_of_levels_after_sync(nft.levels.num_of_levels - sync_level - 1),
		  sync_types_vec(nft.num_of_states(), SynchronizationType::Undefined),
		  sync_symbols_vec(nft.num_of_states(), 0) {
		get_synchronization_types();
	}

	/// Whether @p state may read a symbol on the synchronization level of its round that @p other_state of @p other
	///  can synchronize with on its own: a constant-time test that never refutes a pair that can synchronize.
	bool may_share_a_symbol(const State state, const SynchronizationProperties& other, const State other_state) const {
		return (sync_symbols_vec[state] & other.sync_symbols_vec[other_state]) != 0;
	}

  private:
	/// The signature bit of @p symbol, spread by a multiplicative hash so that close symbols get different bits.
	static uint64_t symbol_bit(const Symbol symbol) {
		if (symbol == DONT_CARE) { return ~uint64_t{0}; }
		return uint64_t{1} << ((static_cast<uint64_t>(symbol) * 0x9E'37'79'B9'7F'4A'7C'15ULL) >> 58);
	}

	/**
	 * @brief For each state, sets the type of synchronization that follows.
	 *
	 * The scope of the synchronization type ends at the next zero-level state.
	 * Each state has a synchronization type that is a combination of the
	 * synchronization types of its children.
	 */
	void get_synchronization_types() {
		const size_t num_of_states = nft.num_of_states();
		for (State root = 0; root < num_of_states; ++root) {
			if (nft.levels[root] != 0) {
				continue; // Skip states not on the zero level.
			}

			// To perform the computation, which is a post-order traversal, we do not
			// use recursion but a stack for better performance. To determine if a parent state
			// has already been visited, we use the helper value SynchronizationType::UnderComputation.
			std::stack<State> stack;
			stack.push(root);
			while (!stack.empty()) {
				const State state = stack.top();
				stack.pop();
				const Level state_level = nft.levels[state];
				const StatePost& state_post = nft.delta[state];
				SynchronizationType current_sync_type = sync_types_vec[state];
				if (current_sync_type != SynchronizationType::Undefined &&
					current_sync_type != SynchronizationType::UnderComputation) {
					// We already computed the synchronization type for this state, so we can skip it.
					// It can happen that when a transducer has a state that has multiple transitions to the same
					//  target, the target can be, in some cases, added to the stack multiple times before the first
					//  occurrence of the target can be processed.
					// See https://github.com/VeriFIT/mata/pull/673.
					continue;
				}
				MATA_ASSERT(
					current_sync_type == SynchronizationType::Undefined ||
					current_sync_type == SynchronizationType::UnderComputation
				);

				// If it has been visited, then we know that it has already been computed for its children.
				if (current_sync_type == SynchronizationType::UnderComputation) {
					// Combine the synchronization types of its children.
					current_sync_type = SynchronizationType::Undefined;
					uint64_t current_sync_symbols{0};
					for (const SymbolPost& symbol_post : state_post) {
						for (const State target : symbol_post.targets) {
							// Skip fast EPSILON transitions.
							if (symbol_post.symbol == EPSILON && nft.levels.num_of_levels != 1 &&
								nft.levels[target] == 0) {
								MATA_ASSERT(state_level == 0);
								continue;
							}
							MATA_ASSERT(nft.levels[target] > nft.levels[state]);
							current_sync_type |= sync_types_vec[target];
							current_sync_symbols |= sync_symbols_vec[target];
						}
					}
					sync_types_vec[state] = current_sync_type;
					sync_symbols_vec[state] = current_sync_symbols;
					continue; // We already visited its children.
				}

				// If we are on the sync level, we can determine the synchronization type.
				if (state_level == sync_level) {
					if (const auto epsilon_post_it = state_post.find(EPSILON); epsilon_post_it != state_post.end()) {
						// We don't want to count fast EPSILON transitions.
						const bool has_not_only_fast_epsilon =
							(state_level != 0 || nft.levels.num_of_levels == 1 ||
							 std::ranges::any_of(epsilon_post_it->targets, [this](const State target) {
								 return nft.levels[target] != 0;
							 }));
						if (has_not_only_fast_epsilon) { current_sync_type = SynchronizationType::OnlyOnEpsilon; }
						if (state_post.size() > 1) { current_sync_type |= SynchronizationType::OnlyOnSymbol; }
					} else if (!state_post.empty()) {
						current_sync_type = SynchronizationType::OnlyOnSymbol;
					}
					sync_types_vec[state] = current_sync_type;
					uint64_t current_sync_symbols{0};
					for (const SymbolPost& symbol_post : state_post) {
						if (symbol_post.symbol != EPSILON) { current_sync_symbols |= symbol_bit(symbol_post.symbol); }
					}
					sync_symbols_vec[state] = current_sync_symbols;
					continue; // No need to visit its children.
				}

				// We need to visit its children.
				sync_types_vec[state] = SynchronizationType::UnderComputation; // Mark this state as under computation.
				stack.push(state); // Push it back to the stack to compute it later.
				for (const SymbolPost& symbol_post : state_post) {
					for (const State target : symbol_post.targets) {
						// Skip fast EPSILON transitions.
						if (symbol_post.symbol == EPSILON && nft.levels.num_of_levels != 1 && nft.levels[target] == 0) {
							MATA_ASSERT(state_level == 0);
							continue;
						}
						MATA_ASSERT(nft.levels[target] > nft.levels[state]);
						MATA_ASSERT(sync_types_vec[target] != SynchronizationType::UnderComputation);
						if (sync_types_vec[target] != SynchronizationType::Undefined) {
							current_sync_type |= sync_types_vec[target];
						} else {
							stack.push(target); // Push the target state onto the stack to visit it.
						}
					}
				}
			}
		}

		MATA_ASSERT(std::ranges::all_of(sync_types_vec, [](SynchronizationType type) {
			return type != SynchronizationType::UnderComputation;
		}));
	}
};

} // namespace

namespace mata::nft {

namespace {
/// @brief Number of levels the composition leaves, or 0 if it leaves none.
size_t count_result_levels(
	const Nft& lhs, const Nft& rhs, const size_t num_of_sync_levels, const bool project_out_sync_levels
) {
	const size_t removed{num_of_sync_levels * (project_out_sync_levels ? 2 : 1)};
	const size_t kept{lhs.levels.num_of_levels + rhs.levels.num_of_levels};
	return kept > removed ? kept - removed : 0;
}

void check_sync_level_in_range(
	const std::string_view caller, const std::string_view side, const Nft& nft, const Level level
) {
	if (level >= nft.levels.num_of_levels) {
		throw std::invalid_argument(
			std::format(
				"{}: synchronization level {} is out of range for the {} NFT with {} levels.", caller, level, side,
				nft.levels.num_of_levels
			)
		);
	}
}

void check_result_num_of_levels(
	const std::string_view caller,
	const Nft& lhs,
	const Nft& rhs,
	const size_t num_of_sync_levels,
	const bool project_out_sync_levels
) {
	if (count_result_levels(lhs, rhs, num_of_sync_levels, project_out_sync_levels) == 0) {
		throw std::invalid_argument(
			std::format(
				"{}: composing {} and {} levels on {} synchronization level(s){} leaves no levels.", caller,
				lhs.levels.num_of_levels, rhs.levels.num_of_levels, num_of_sync_levels,
				project_out_sync_levels ? " projected out" : ""
			)
		);
	}
}

void check_sync_levels(
	const std::string_view caller, const std::string_view side, const Nft& nft, const OrdVector<Level>& sync_levels
) {
	// `OrdVector`'s constructors sort and deduplicate, but `push_back()` does not, and the composition subtracts
	//  consecutive levels unsigned.
	if (std::ranges::adjacent_find(sync_levels, std::greater_equal{}) != sync_levels.end()) {
		throw std::invalid_argument(
			std::format("{}: the {} synchronization levels must be strictly increasing.", caller, side)
		);
	}
	check_sync_level_in_range(caller, side, nft, sync_levels.back());
}

/// @brief Check everything the composition engines assume about the request, in Release too.
void validate_composition_request(
	const std::string_view caller,
	const Nft& lhs,
	const Nft& rhs,
	const OrdVector<Level>& lhs_sync_levels,
	const OrdVector<Level>& rhs_sync_levels,
	const bool project_out_sync_levels
) {
	if (lhs_sync_levels.size() != rhs_sync_levels.size() || lhs_sync_levels.empty()) {
		throw std::invalid_argument(
			std::format(
				"{}: requires a non-empty pair of synchronization level vectors of the same size: got sizes {} and {}.",
				caller, lhs_sync_levels.size(), rhs_sync_levels.size()
			)
		);
	}
	check_sync_levels(caller, "lhs", lhs, lhs_sync_levels);
	check_sync_levels(caller, "rhs", rhs, rhs_sync_levels);
	check_result_num_of_levels(caller, lhs, rhs, lhs_sync_levels.size(), project_out_sync_levels);
}

/// Why the fast composer created a result state. Both normalization rules treat the generated kinds alike.
enum class GeneratedKind : uint8_t {
	Semantic, ///< A configuration of the two operands. No normalization rule eliminates it.
	Padding, ///< An inner state of an EPSILON chain padding a one-sided move to a whole round.
	ProjectedBridge, ///< An inner state of the EPSILON chain replacing an EPSILON synchronization while waiting.
	Waiting, ///< A state of the waiting loop over a stationary operand state.
};

/**
 * @brief What the fast composer records about its result, for the normalization of the result.
 *
 * The builder keeps these invariants: a generated state has a nonzero level, it is neither initial nor final, and
 *  it gets an incoming transition when it is created. @c in_degree of a state is at least the number of its
 *  incoming transitions: every insertion of a target counts, a repeated one too, which only makes the
 *  normalization more conservative.
 */
struct GeneratedStates {
	std::vector<GeneratedKind> kind{}; ///< Indexed by state; the states past its end are @c Semantic.
	/// Indexed by state; the states past its end have none. An eliminated generated state has 0.
	std::vector<State> in_degree{};
	std::vector<State> states{}; ///< The generated states in the order of their creation.
	std::vector<State> epsilon_sources{}; ///< Zero-level states that got an EPSILON transition, possibly repeated.

	void add(const State state, const GeneratedKind generated_kind) {
		MATA_ASSERT(generated_kind != GeneratedKind::Semantic);
		if (state >= kind.size()) { kind.resize(state + 1, GeneratedKind::Semantic); }
		kind[state] = generated_kind;
		states.push_back(state);
	}

	bool is_generated(const State state) const { return state < kind.size() && kind[state] != GeneratedKind::Semantic; }

	State in_degree_of(const State state) const { return state < in_degree.size() ? in_degree[state] : 0; }

	/// Count one more incoming transition of each of @p targets.
	void link(const SmallStateSet& targets) {
		if (const State max_target{targets.back()}; max_target >= in_degree.size()) {
			in_degree.resize(std::max<size_t>(max_target + 1, in_degree.size() * 2), 0);
		}
		for (const State target : targets) { ++in_degree[target]; }
	}
};

/// A whole-round EPSILON run: @c source -EPSILON-> @c head -EPSILON-> ... -EPSILON-> @c end, with zero-level
///  @c source and @c end and generated inner states, each with exactly one incoming and one outgoing transition.
///  In a one-level result, @c head is @c end and there are no inner states.
struct EpsilonRound {
	State source;
	State head;
	State end;
};

/**
 * @brief Normalizes the result of the fast composer on the states it generated.
 *
 * Rule "close a whole-round EPSILON run": the zero-level source of an @c EpsilonRound takes over the outgoing
 *  transitions and the finality of the run's end, and the run's transitions and inner states go away.
 *  Invariant: the run reads EPSILON on every level, so the source accepts after it exactly what the end accepts,
 *  and the inner states are reachable through the run only, so nothing else loses them. If the run is the only way
 *  into a non-initial end, the end's transitions move to the source and the end is left unreachable; otherwise the
 *  source gets a copy, and the run is closed only if the copy does not outgrow the run. The runs are closed in
 *  reverse topological order, so the transitions an end hands over are final; the runs inside nontrivial strongly
 *  connected components (EPSILON cycles), and the runs into them, are left unchanged.
 *
 * Rule "merge identical generated states": generated states of the same level whose outgoing transitions are the
 *  same, after the targets merged already are replaced by their representatives, become one state. Invariant: such
 *  states are neither initial nor final and read the same continuations of the same tuples, so redirecting their
 *  incoming transitions to the representative keeps the relation and the level of every remaining state. The rule
 *  runs after the construction ends, when every state is complete, from the last level down, so the targets of a
 *  state are merged before the state is compared.
 *
 * Neither rule eliminates a @c GeneratedKind::Semantic state, it can only leave one unreachable. The compaction
 *  removes the eliminated states and those, or, if the result is to be trimmed anyway, every useless state.
 */
class GeneratedStatesNormalizer {
  public:
	GeneratedStatesNormalizer(Nft& result, GeneratedStates& generated) : result_{result}, generated_{generated} {}

	void run(const bool trim_result) {
		close_epsilon_rounds();
		merge_identical_states();
		compact(trim_result);
	}

  private:
	struct Node {
		State state;
		State low_link;
		size_t rounds_begin;
		size_t rounds_end;
		bool on_stack;
		bool in_cycle;
	};

	Nft& result_;
	GeneratedStates& generated_;
	size_t num_of_closed_rounds_{0};
	std::vector<State> dead_ends_{}; ///< The zero-level ends of closed runs that lost their only predecessor.
	/// The representative of each merged generated state; @c Limits::max_state for the others.
	std::vector<State> representative_{};
	size_t num_of_merged_states_{0};

	/// The zero-level end of the whole-round EPSILON run entered by an EPSILON transition to @p head, or
	///  @c Limits::max_state if an inner state is not a generated state with exactly one incoming and one outgoing
	///  transition, both over EPSILON.
	State end_of_private_epsilon_round(const State head) const {
		State state{head};
		while (result_.levels[state] != 0) {
			if (!generated_.is_generated(state) || generated_.in_degree_of(state) != 1) { return Limits::max_state; }
			const StatePost& state_post{result_.delta[state]};
			if (state_post.size() != 1 || state_post.front().symbol != EPSILON ||
				state_post.front().targets.size() != 1) {
				return Limits::max_state;
			}
			const State next{state_post.front().targets.front()};
			// No jumps: the levels increase by one until the run wraps around to level 0, so the walk ends.
			MATA_ASSERT(result_.levels[next] == 0 || result_.levels[next] == result_.levels[state] + 1);
			state = next;
		}
		return state;
	}

	void collect_rounds(const State source, std::vector<EpsilonRound>& rounds) const {
		const StatePost& state_post{result_.delta[source]};
		// EPSILON is the greatest symbol.
		if (state_post.empty() || state_post.back().symbol != EPSILON) { return; }
		for (const State head : state_post.back().targets) {
			if (const State end{end_of_private_epsilon_round(head)}; end != Limits::max_state) {
				rounds.push_back({source, head, end});
			}
		}
	}

	/// Tarjan's algorithm over the graph of the runs: it finishes the components in reverse topological order,
	///  which is the order in which the runs can be closed, and a run never waits for its own cycle.
	void close_epsilon_rounds() {
		if (generated_.epsilon_sources.empty()) { return; }
		// A node is identified by its index in `nodes`, which is also its Tarjan index.
		std::vector<State> node_of(result_.num_of_states(), Limits::max_state);
		std::vector<Node> nodes{};
		std::vector<EpsilonRound> rounds{};
		std::vector<State> component_stack{};
		std::vector<std::pair<State, size_t>> call_stack{}; // A node and its next round to follow.

		const auto visit = [&](const State state) {
			const auto node{static_cast<State>(nodes.size())};
			const size_t rounds_begin{rounds.size()};
			collect_rounds(state, rounds);
			nodes.push_back(
				Node{
					.state = state,
					.low_link = node,
					.rounds_begin = rounds_begin,
					.rounds_end = rounds.size(),
					.on_stack = true,
					.in_cycle = false
				}
			);
			node_of[state] = node;
			component_stack.push_back(node);
			call_stack.emplace_back(node, rounds_begin);
		};

		for (const State source : generated_.epsilon_sources) {
			if (node_of[source] != Limits::max_state) { continue; }
			visit(source);
			while (!call_stack.empty()) {
				const State node{call_stack.back().first};
				if (const size_t round_index{call_stack.back().second}; round_index < nodes[node].rounds_end) {
					++call_stack.back().second;
					const State end{rounds[round_index].end};
					if (const State end_node{node_of[end]}; end_node == Limits::max_state) {
						visit(end);
					} else if (nodes[end_node].on_stack) {
						nodes[node].low_link = std::min(nodes[node].low_link, end_node);
					}
					continue;
				}
				call_stack.pop_back();
				if (!call_stack.empty()) {
					State& parent_low_link{nodes[call_stack.back().first].low_link};
					parent_low_link = std::min(parent_low_link, nodes[node].low_link);
				}
				if (nodes[node].low_link != node) { continue; }

				// `node` is the root of a finished component, the top of the stack down to it.
				const auto component_begin{
					std::ranges::find(component_stack.rbegin(), component_stack.rend(), node).base() - 1
				};
				const auto node_rounds{std::ranges::subrange(
					rounds.begin() + static_cast<std::ptrdiff_t>(nodes[node].rounds_begin),
					rounds.begin() + static_cast<std::ptrdiff_t>(nodes[node].rounds_end)
				)};
				const bool in_cycle{
					component_stack.end() - component_begin > 1 ||
					std::ranges::any_of(node_rounds, [&](const EpsilonRound& round) {
						return round.end == nodes[node].state;
					})
				};
				for (auto member_it{component_begin}; member_it != component_stack.end(); ++member_it) {
					Node& member{nodes[*member_it]};
					member.on_stack = false;
					member.in_cycle = in_cycle;
				}
				if (!in_cycle) {
					for (size_t i{nodes[node].rounds_begin}; i < nodes[node].rounds_end; ++i) {
						if (!nodes[node_of[rounds[i].end]].in_cycle) { close_round(rounds[i]); }
					}
				}
				component_stack.erase(component_begin, component_stack.end());
			}
		}
	}

	void close_round(const EpsilonRound& round) {
		MATA_ASSERT(round.source != round.end);
		MATA_ASSERT(end_of_private_epsilon_round(round.head) == round.end);
		size_t num_of_inner_states{0};
		for (State state{round.head}; state != round.end; state = result_.delta[state].front().targets.front()) {
			++num_of_inner_states;
		}
		// The end dies with the run iff the run is its only way in; its transitions then move to the source.
		const bool end_dies{generated_.in_degree_of(round.end) == 1 && !result_.initial.contains(round.end)};
		if (!end_dies) {
			// Otherwise the source gets a copy of them, which must not grow the result.
			size_t num_of_copied{0};
			for (const SymbolPost& symbol_post : result_.delta[round.end]) {
				num_of_copied += symbol_post.targets.size();
			}
			const size_t num_of_removed{num_of_inner_states + 1};
			if (num_of_copied > num_of_removed || (num_of_copied == num_of_removed && num_of_inner_states == 0)) {
				return;
			}
		}

		StatePost& source_post{result_.delta.mutable_state_post(round.source)};
		MATA_ASSERT(!source_post.empty() && source_post.back().symbol == EPSILON);
		source_post.back().targets.erase(round.head);
		if (source_post.back().targets.empty()) { source_post.erase(std::prev(source_post.end())); }
		for (State state{round.head}; state != round.end;) {
			MATA_ASSERT(generated_.is_generated(state), "No rule eliminates a semantic state.");
			StatePost& state_post{result_.delta.mutable_state_post(state)};
			const State next{state_post.front().targets.front()};
			state_post.clear();
			generated_.in_degree[state] = 0;
			state = next;
		}
		--generated_.in_degree[round.end];
		if (end_dies) { dead_ends_.push_back(round.end); }

		take_over_post(round.source, round.end, end_dies);
		if (result_.final.contains(round.end)) { result_.final.insert(round.source); }
		++num_of_closed_rounds_;
	}

	/// Add the transitions of @p from to @p to, merging the two posts in one pass; with @p move, @p from loses them.
	void take_over_post(const State to, const State from, const bool move) {
		if (from >= result_.delta.num_of_states() || result_.delta[from].empty()) { return; }
		// Neither call grows the delta: `to` had an EPSILON transition, `from` has some transition.
		StatePost& to_post{result_.delta.mutable_state_post(to)};
		StatePost& from_post{result_.delta.mutable_state_post(from)};
		if (!move) {
			for (const SymbolPost& symbol_post : from_post) { generated_.link(symbol_post.targets); }
		}
		if (to_post.empty() && move) {
			// Exchange rather than move: 'from_post' is cleared below and has to be in a known state.
			to_post = std::exchange(from_post, {});
		} else if (to_post.empty()) {
			to_post = from_post;
		} else {
			std::vector<SymbolPost> merged{};
			merged.reserve(to_post.size() + from_post.size());
			auto to_it{to_post.begin()};
			auto from_it{from_post.begin()};
			while (to_it != to_post.end() || from_it != from_post.end()) {
				if (from_it == from_post.end() || (to_it != to_post.end() && to_it->symbol < from_it->symbol)) {
					merged.push_back(std::move(*to_it++));
				} else if (to_it == to_post.end() || from_it->symbol < to_it->symbol) {
					merged.push_back(move ? std::move(*from_it++) : *from_it++);
				} else {
					to_it->targets.insert(from_it->targets);
					merged.push_back(std::move(*to_it++));
					++from_it;
				}
			}
			to_post = StatePost{utils::sorted_unique, std::move(merged)};
		}
		if (move) { from_post.clear(); }
	}

	struct FlatPostHash {
		size_t operator()(const std::vector<State>& flat_post) const {
			size_t hash{flat_post.size()};
			for (const State value : flat_post) {
				hash ^= std::hash<State>{}(value) + 0x9e'37'79'b9'7f'4a'7c'15ULL + (hash << 6) + (hash >> 2);
			}
			return hash;
		}
	};

	State representative_of(const State state) const {
		if (!generated_.is_generated(state) || representative_.empty()) { return state; }
		const State representative{representative_[state]};
		return representative == Limits::max_state ? state : representative;
	}

	void merge_identical_states() {
		const size_t num_of_levels{result_.levels.num_of_levels};
		if (num_of_levels < 2) { return; }
		// Bucket the live generated states by level.
		std::vector<size_t> level_begin(num_of_levels + 1, 0);
		for (const State state : generated_.states) {
			if (generated_.in_degree_of(state) != 0) { ++level_begin[result_.levels[state] + 1]; }
		}
		for (size_t level{1}; level <= num_of_levels; ++level) { level_begin[level] += level_begin[level - 1]; }
		if (level_begin[num_of_levels] < 2) { return; }
		std::vector<State> by_level(level_begin[num_of_levels]);
		{
			std::vector<size_t> position{level_begin.begin(), level_begin.end() - 1};
			for (const State state : generated_.states) {
				if (generated_.in_degree_of(state) != 0) { by_level[position[result_.levels[state]]++] = state; }
			}
		}

		representative_.assign(result_.num_of_states(), Limits::max_state);
		// Most generated states forward one symbol to one target: those are grouped by sorting, the rest by hashing.
		struct SingleTransition {
			Symbol symbol;
			State target;
			State state;
			auto operator<=>(const SingleTransition&) const = default;
		};
		std::vector<SingleTransition> single_transitions{};
		std::unordered_map<std::vector<State>, State, FlatPostHash> by_flat_post{};
		std::vector<State> flat_post{};
		const auto merge{[&](const State state, const State representative) {
			if (representative == state) { return; }
			representative_[state] = representative;
			++num_of_merged_states_;
		}};
		for (size_t level{num_of_levels - 1}; level >= 1; --level) {
			single_transitions.clear();
			by_flat_post.clear();
			for (size_t i{level_begin[level]}; i < level_begin[level + 1]; ++i) {
				const State state{by_level[i]};
				MATA_ASSERT(!result_.initial.contains(state) && !result_.final.contains(state));
				const StatePost& state_post{result_.delta[state]};
				if (state_post.size() == 1 && state_post.front().targets.size() == 1) {
					single_transitions.push_back(
						{state_post.front().symbol, representative_of(state_post.front().targets.front()), state}
					);
					continue;
				}
				flat_post.clear();
				for (const SymbolPost& symbol_post : state_post) {
					flat_post.push_back(symbol_post.symbol);
					flat_post.push_back(static_cast<State>(symbol_post.targets.size()));
					const size_t targets_begin{flat_post.size()};
					for (const State target : symbol_post.targets) { flat_post.push_back(representative_of(target)); }
					std::sort(flat_post.begin() + static_cast<std::ptrdiff_t>(targets_begin), flat_post.end());
				}
				// Targets merged into one representative are counted twice, which only misses a merge.
				merge(state, by_flat_post.try_emplace(flat_post, state).first->second);
			}
			// The smallest state of a group represents it.
			std::ranges::sort(single_transitions);
			for (size_t group_begin{0}, i{0}; i < single_transitions.size(); ++i) {
				const SingleTransition& transition{single_transitions[i]};
				const SingleTransition& first{single_transitions[group_begin]};
				if (transition.symbol != first.symbol || transition.target != first.target) {
					group_begin = i;
				} else {
					merge(transition.state, first.state);
				}
			}
		}
	}

	/// Point every transition into a merged state at its representative.
	void redirect_to_representatives() {
		for (State source{0}; source < result_.delta.num_of_states(); ++source) {
			for (SymbolPost& symbol_post : result_.delta.mutable_state_post(source)) {
				bool redirected{false};
				for (State& target : symbol_post.targets) {
					if (const State representative{representative_of(target)}; representative != target) {
						target = representative;
						redirected = true;
					}
				}
				if (redirected) { symbol_post.targets.sort_and_rmdupl(); }
			}
		}
	}

	/// Remove the eliminated states: the inner states of the closed runs, the ends that died, and the merged states.
	///  With @p trim_result, the useless states too, which include all the eliminated ones, as nothing reaches them.
	void compact(const bool trim_result) {
		if (num_of_merged_states_ > 0) { redirect_to_representatives(); }
		if (trim_result) {
			result_.trim();
			return;
		}
		if (num_of_closed_rounds_ == 0 && num_of_merged_states_ == 0) { return; }

		const size_t num_of_states{result_.num_of_states()};
		BoolVector staying(num_of_states, true);
		for (const State state : generated_.states) {
			if (generated_.in_degree_of(state) == 0 || representative_of(state) != state) { staying[state] = false; }
		}
		for (const State state : dead_ends_) { staying[state] = false; }
		std::vector<State> renaming(num_of_states, Limits::max_state);
		for (State state{0}, num_of_staying{0}; state < num_of_states; ++state) {
			if (staying[state]) { renaming[state] = num_of_staying++; }
		}
		result_.delta.allocate(num_of_states);
		result_.delta.defragment(staying, renaming);
		State moved{0};
		// `levels` may hold more entries than there are states.
		result_.levels.erase(
			std::ranges::remove_if(
				result_.levels,
				[&](Level&) {
					const State state{moved++};
					return state >= num_of_states || !staying[state];
				}
			).begin(),
			result_.levels.end()
		);
		const auto is_staying{[&](const State state) { return static_cast<bool>(staying[state]); }};
		const auto rename{[&](const State state) { return renaming[state]; }};
		result_.initial.filter(is_staying);
		result_.final.filter(is_staying);
		result_.initial.rename(rename);
		result_.final.rename(rename);
		result_.initial.truncate();
		result_.final.truncate();
	}
};

/// The generic part of @c CompositionNormalization::Full, run after the composer's own normalization.
void normalize_fully(Nft& result, const JumpMode jump_mode) {
	std::shared_ptr<AlphabetLevels> alphabets{result.alphabets};
	// `remove_epsilon()` takes every transition for one level long.
	if (jump_mode == JumpMode::NoJump) { result = remove_epsilon(result); }
	result.trim();
	result = reduce(result);
	result.alphabets = std::move(alphabets);
}
} // namespace

std::shared_ptr<mata::AlphabetLevels> compose_alphabets(
	const Nft& lhs,
	const Nft& rhs,
	const OrdVector<Level>& lhs_sync_levels,
	const OrdVector<Level>& rhs_sync_levels,
	const bool project_out_sync_levels
) {
	validate_composition_request(
		"compose_alphabets", lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels
	);
	if (lhs.alphabets == nullptr || rhs.alphabets == nullptr) { return nullptr; }

	// Raw slot for a level, honoring Global (single shared alphabet) vs MultiLevel (per-level) mode.
	auto slot_for_level = [](const mata::AlphabetLevels& alphabets,
							 const Level level) -> std::shared_ptr<mata::Alphabet> {
		return alphabets.mode() == mata::AlphabetLevels::Mode::Global ? alphabets.at(0) : alphabets.at(level);
	};

	std::vector<std::shared_ptr<mata::Alphabet>> composed_alphabets;
	composed_alphabets.reserve(lhs.levels.num_of_levels + rhs.levels.num_of_levels);

	const size_t num_of_sync_levels = lhs_sync_levels.size();
	auto lhs_sync_it = lhs_sync_levels.cbegin();
	auto rhs_sync_it = rhs_sync_levels.cbegin();
	Level lhs_lvl{0};
	Level rhs_lvl{0};
	for (size_t i{0}; i < num_of_sync_levels; ++i) {
		for (; lhs_lvl < *lhs_sync_it; ++lhs_lvl) {
			composed_alphabets.push_back(slot_for_level(*lhs.alphabets, lhs_lvl));
		}
		for (; rhs_lvl < *rhs_sync_it; ++rhs_lvl) {
			composed_alphabets.push_back(slot_for_level(*rhs.alphabets, rhs_lvl));
		}
		if (!project_out_sync_levels) {
			if (slot_for_level(*lhs.alphabets, *lhs_sync_it) != slot_for_level(*rhs.alphabets, *rhs_sync_it)) {
				throw std::invalid_argument(
					std::format(
						"compose_alphabets: lhs and rhs must share the same alphabet instance on their kept "
						"synchronization levels {} and {}.",
						*lhs_sync_it, *rhs_sync_it
					)
				);
			}
			composed_alphabets.push_back(slot_for_level(*lhs.alphabets, *lhs_sync_it));
		}
		++lhs_lvl;
		++rhs_lvl; // Skip over the synchronization level itself (already handled above).
		++lhs_sync_it;
		++rhs_sync_it;
	}
	for (; lhs_lvl < lhs.levels.num_of_levels; ++lhs_lvl) {
		composed_alphabets.push_back(slot_for_level(*lhs.alphabets, lhs_lvl));
	}
	for (; rhs_lvl < rhs.levels.num_of_levels; ++rhs_lvl) {
		composed_alphabets.push_back(slot_for_level(*rhs.alphabets, rhs_lvl));
	}

	return std::make_shared<mata::AlphabetLevels>(std::move(composed_alphabets));
}

namespace {
/**
 * Whether the fast composition, which reads @p nft with @c JumpMode::NoJump, composes it as the general composition
 *  does under any jump mode: @p nft has no jump transitions, so the jump mode does not matter, and neither EPSILON nor
 *  DONT_CARE, which the two compositions place and match differently (#763).
 */
bool reads_jump_mode_alike(const Nft& nft) {
	static_assert(EPSILON > DONT_CARE, "EPSILON and DONT_CARE are expected to sort last in a state post");
	for (State state{0}; state < nft.num_of_states(); ++state) {
		const StatePost& state_post{nft.delta[state]};
		if (!state_post.empty() && state_post.back().symbol >= DONT_CARE) { return false; }
	}
	return !nft.contains_jump_transitions();
}
} // namespace

// Dispatch function for composition modes.
Nft compose(
	const Nft& lhs,
	const Nft& rhs,
	const utils::OrdVector<Level>& lhs_sync_levels,
	const utils::OrdVector<Level>& rhs_sync_levels,
	const bool project_out_sync_levels,
	const JumpMode jump_mode,
	const CompositionMode composition_mode,
	const CompositionNormalization normalization
) {
	validate_composition_request("compose", lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels);
	switch (composition_mode) {
		case CompositionMode::FastNoJump:
			if (jump_mode != JumpMode::NoJump) {
				throw std::invalid_argument("FastNoJump composition mode only supports NoJump jump mode.");
			}
			if (lhs_sync_levels.size() != 1 || rhs_sync_levels.size() != 1) {
				throw std::invalid_argument(
					"FastNoJump composition mode only supports a single synchronization level per NFT."
				);
			}
			return algorithms::compose_fast_no_jump(
				lhs, rhs, lhs_sync_levels.front(), rhs_sync_levels.front(), project_out_sync_levels, normalization
			);
		case CompositionMode::Auto:
			if (lhs_sync_levels.size() == 1 &&
				(jump_mode == JumpMode::NoJump || (reads_jump_mode_alike(lhs) && reads_jump_mode_alike(rhs)))) {
				return algorithms::compose_fast_no_jump(
					lhs, rhs, lhs_sync_levels.front(), rhs_sync_levels.front(), project_out_sync_levels, normalization
				);
			}
			[[fallthrough]];
		case CompositionMode::General:
		default:
			return algorithms::compose_general(
				lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels, jump_mode, normalization
			);
	}
}

namespace algorithms {
namespace {
/// The body of @c compose_fast(), with the EPSILON-composition filter compiled in only when @p UseFilter is set; the
/// inputs are validated by the caller. With @p normalize_generated, the builder records the states it generates and
/// normalizes the result on them (@c CompositionNormalization::GeneratedOnly). @p Operand is an @c Nft without jump
/// transitions (fast EPSILON transitions apart) or a @c CursorOperand; the result has no alphabets. @p schedule
/// drives the construction: it is given a function that builds the result until it has more than the given number of
/// states and returns whether the result is complete, which it may call again to continue. It returns whether to
/// keep the result, which has to be complete then; otherwise nothing is returned.
template <bool UseFilter, typename Operand, typename Schedule>
std::optional<Nft> compose_fast_with(
	const Operand& lhs,
	const Operand& rhs,
	const Level lhs_sync_level,
	const Level rhs_sync_level,
	const bool project_out_sync_levels,
	const bool normalize_generated,
	Schedule&& schedule
) {
	// Number of Levels and States.
	const size_t lhs_num_of_states = lhs.num_of_states();
	const size_t rhs_num_of_states = rhs.num_of_states();
	const size_t result_num_of_levels =
		lhs.levels.num_of_levels + rhs.levels.num_of_levels - (project_out_sync_levels ? (2) : 1);
	MATA_ASSERT(result_num_of_levels > 0);

	// Initialize the synchronization properties for both NFTs.
	const SynchronizationProperties lhs_sync_props(lhs, lhs_sync_level);
	const SynchronizationProperties rhs_sync_props(rhs, rhs_sync_level);

	Nft result{};
	result.levels.num_of_levels = result_num_of_levels;
	// Use composition storage without tracking inverted indices,
	// because waiting in virtual states would spoil the inverse mapping.
	// The EPSILON-composition filter mode is a part of the key of zero-level states only: after a
	// synchronized symbol both modes coincide again, so the intermediate configurations of a
	// synchronized round are shared in the LhsMayMoveAlone map.
	std::array<TwoDimensionalMap<State, false>, UseFilter ? NUMBER_OF_EPSILON_FILTER_MODES : 1> composition_storage{
		[&]<size_t... Index>(std::index_sequence<Index...>) {
			return std::array{((void) Index, TwoDimensionalMap<State, false>{lhs_num_of_states, rhs_num_of_states})...};
		}(std::make_index_sequence<UseFilter ? NUMBER_OF_EPSILON_FILTER_MODES : 1>{})
	};
	/// The state map of the zero-level configurations reached in @p epsilon_filter_mode.
	const auto storage_for{[&](const EpsilonFilterMode epsilon_filter_mode) -> TwoDimensionalMap<State, false>& {
		if constexpr (UseFilter) {
			return composition_storage[static_cast<size_t>(epsilon_filter_mode)];
		} else {
			MATA_ASSERT(epsilon_filter_mode == EpsilonFilterMode::LhsMayMoveAlone);
			return composition_storage[0];
		}
	}};
	std::unordered_map<State, State> waiting_state_storage;
	GeneratedStates generated{};

	// I use a queue for the worklist to process states in a breadth-first manner.
	// This helps the branch prediction in the CPU because all processed states
	// are likely to be at the same level and thus enter the same branch.
	std::queue<CompositionKey<UseFilter>> worklist;

	/**
	 * @brief Efficiently inserts a symbol post into the delta of the result NFT.
	 *
	 * @param source The source state of the transition.
	 * @param symbol_post The symbol post to insert.
	 */
	auto insert_symbol_post_to_delta = [&](const State source, SymbolPost&& symbol_post) {
		MATA_ASSERT(not symbol_post.targets.empty());
		if (normalize_generated) {
			if (symbol_post.symbol == EPSILON && result.levels[source] == 0) {
				generated.epsilon_sources.push_back(source);
			}
			generated.link(symbol_post.targets);
		}
		auto& mutable_state_post = result.delta.mutable_state_post(source);
		if (const auto symbol_post_it = mutable_state_post.find(symbol_post.symbol);
			symbol_post_it != mutable_state_post.end()) {
			// The symbol post already exists, just add the targets.
			symbol_post_it->targets.insert(symbol_post.targets);
			return;
		}
		// The symbol post does not exist.
		if (mutable_state_post.empty() || mutable_state_post.back().symbol < symbol_post.symbol) {
			// It is the greatest symbol, just add it to the end.
			mutable_state_post.push_back(std::move(symbol_post));
			return;
		}
		// Use insert method to insert at the correct position.
		mutable_state_post.insert(std::move(symbol_post));
	};

	/**
	 * @brief Creates a chain of EPSILON transitions of the given length starting from the source state.
	 *
	 * @param source The source state from which the EPSILON transitions will start.
	 * @param common_path_length The length of the EPSILON transition chain to create.
	 * @param kind Why the chain is created.
	 * @return The last state in the created EPSILON transition chain.
	 */
	auto create_epsilon_transition_with_common_path = [&](const State source, const size_t common_path_length,
														  const GeneratedKind kind = GeneratedKind::Padding) {
		MATA_ASSERT(source < result.num_of_states());
		State current_source = source;
		Level current_level = result.levels[current_source];
		for (size_t i = 0; i < common_path_length; ++i) {
			SymbolPost symbol_post{EPSILON};
			const State new_composition_state = result.add_state_with_level(++current_level);
			if (normalize_generated) { generated.add(new_composition_state, kind); }
			MATA_ASSERT(current_level < result.levels.num_of_levels);
			symbol_post.push_back(new_composition_state);
			insert_symbol_post_to_delta(current_source, std::move(symbol_post));
			current_source = new_composition_state;
		}
		return current_source;
	};

	/**
	 * @brief Creates an EPSILON transition from source to target, possibly with a common path in between.
	 * @param source The source state of the EPSILON transition.
	 * @param target The target state of the EPSILON transition.
	 */
	auto create_epsilon_transition_with_target = [&](const State source, const State target) {
		MATA_ASSERT(source < result.num_of_states() && target < result.num_of_states());
		const Level source_level = result.levels[source];
		const Level target_level = result.levels[target];
		const size_t common_path_length =
			(target_level == 0 ? result.levels.num_of_levels : target_level) - source_level - 1;
		const State internal_state = create_epsilon_transition_with_common_path(source, common_path_length);
		SymbolPost symbol_post{EPSILON};
		symbol_post.insert(target);
		insert_symbol_post_to_delta(internal_state, std::move(symbol_post));
	};

	/**
	 * @brief Creates a new composition state for the given pair of states, if it does not already exist.
	 *
	 * @param first The first state (from lhs or rhs).
	 * @param second The second state (from rhs or lhs).
	 * @param level The level of the new composition state.
	 * @param is_first_lhs If true, @p first is from the lhs NFT; otherwise, it is from the rhs NFT.
	 * @param epsilon_filter_mode The EPSILON-filter mode the pair is reached in; only zero-level states
	 *                            are split by the mode, which therefore defaults to the mode shared by
	 *                            all synchronized rounds.
	 * @param composition_state_to_add If provided, this is the state to be added to the result NFT;
	 *                                 if not provided, a new state will be created.
	 *
	 * @return The composition state for the given pair of states.
	 */
	auto create_composition_state = [&](const State first, const State second, const Level level,
										const bool is_first_lhs = true,
										const EpsilonFilterMode epsilon_filter_mode =
											EpsilonFilterMode::LhsMayMoveAlone,
										const State composition_state_to_add = Limits::max_state) {
		const auto key = is_first_lhs ? std::make_pair(first, second) : std::make_pair(second, first);
		MATA_ASSERT(
			level == 0 || epsilon_filter_mode == EpsilonFilterMode::LhsMayMoveAlone,
			"Only zero-level configurations are split by the EPSILON filter mode"
		);

		TwoDimensionalMap<State, false>& storage = storage_for(epsilon_filter_mode);
		const State next_state = composition_state_to_add != Limits::max_state
									 ? composition_state_to_add
									 : static_cast<State>(result.num_of_states());
		const auto [new_state, inserted]{storage.insert_if_absent(key.first, key.second, next_state)};
		if (!inserted) {
			MATA_ASSERT(result.levels[new_state] == level);
			return new_state;
		}
		MATA_ASSERT(composition_state_to_add == Limits::max_state || result.levels[composition_state_to_add] == level);

		// The state must not be created before the insertion above: a duplicate configuration must leave the result
		// alone.
		if (composition_state_to_add == Limits::max_state) { result.add_state_with_level(level); }
		if (level == 0) {
			// If the level is zero, check for final states and add the state to the worklist.
			if ((is_first_lhs && lhs.final.contains(first) && rhs.final.contains(second)) ||
				(!is_first_lhs && rhs.final.contains(first) && lhs.final.contains(second))) {
				result.final.insert(new_state);
			}
		}
		worklist.push({key.first, key.second, new_state, epsilon_filter_mode});
		return new_state;
	};

	/**
	 * @brief Perform the synchronization of the LHS and RHS at the given state pair.
	 *
	 * @param composition_state The state in the composition NFT from which the synchronization result will proceed.
	 * @param lhs_state The state in the LHS NFT being synchronized.
	 * @param rhs_state The state in the RHS NFT being synchronized.
	 * @param reconnect If true, the synchronization will use the specified symbol on the resulting
	 *                  synchronization transition, virtually reconnecting the predecessors of
	 *                  the synchronization level to its successors; otherwise, it will simply
	 *                  synchronize and keep or remove the synchronization level depending on
	 *                  the project_out_sync_levels flag.
	 * @param reconnection_symbol The symbol to use for the reconnection transition if reconnect is true.
	 */
	auto synchronize = [&](const State composition_state, const State lhs_state, const State rhs_state,
						   const bool reconnect = false, const Symbol reconnection_symbol = Limits::max_symbol) {
		const Level composition_state_level = result.levels[composition_state];
		const auto composition_target_level =
			static_cast<Level>((composition_state_level + 1) % result.levels.num_of_levels);
		// When projecting out the synchronization levels, and we do not want to connect their
		// predecessors to their successors, we simply perform the synchronization, note the reached targets,
		// and remove (vanish) the synchronization level and its transition from the result NFT.
		const bool vanish_sync_level = !reconnect && project_out_sync_levels;

		// Helper function to combine LHS and RHS targets over the given symbol.
		auto combine_targets = [&](const SmallStateSet& lhs_sync_targets, const SmallStateSet& rhs_sync_targets,
								   const Symbol symbol) {
			if (!vanish_sync_level) {
				// Do a standard combination of targets.
				SymbolPost symbol_post{symbol};
				for (const State lhs_sync_target : lhs_sync_targets) {
					for (const State rhs_sync_target : rhs_sync_targets) {
						symbol_post.insert(
							create_composition_state(lhs_sync_target, rhs_sync_target, composition_target_level)
						);
					}
				}
				insert_symbol_post_to_delta(composition_state, std::move(symbol_post));
				return;
			}

			// The synchronization level will vanish.
			// We know that there is at least one transition in LHS or RHS after the synchronization.
			// We are going to use those transitions to connect composition_state to their targets.
			const bool is_remaining_transition_in_lhs = lhs_sync_level + 1 < lhs.levels.num_of_levels;
			MATA_ASSERT(is_remaining_transition_in_lhs || rhs_sync_level + 1 < rhs.levels.num_of_levels);

			const SmallStateSet& moving_sync_targets =
				is_remaining_transition_in_lhs ? lhs_sync_targets : rhs_sync_targets;
			const SmallStateSet& static_sync_targets =
				is_remaining_transition_in_lhs ? rhs_sync_targets : lhs_sync_targets;
			const Operand& moving_nft = is_remaining_transition_in_lhs ? lhs : rhs;

			mata::utils::SynchronizedExistentialIterator<mata::utils::OrdVector<SymbolPost>::const_iterator>
				moving_sync_iterator(moving_sync_targets.size());
			for (const State moving_sync_target : moving_sync_targets) {
				mata::utils::push_back(moving_sync_iterator, moving_nft.delta[moving_sync_target]);
			}
			while (moving_sync_iterator.advance()) {
				const std::vector<StatePost::const_iterator>& same_symbol_posts{moving_sync_iterator.get_current()};
				MATA_ASSERT(!same_symbol_posts.empty());

				const Symbol moving_symbol = same_symbol_posts[0]->symbol;
				SymbolPost symbol_post{moving_symbol};
				for (const auto& moving_next_targets : same_symbol_posts) {
					for (const State moving_next_target : moving_next_targets->targets) {
						for (const State static_sync_target : static_sync_targets) {
							symbol_post.insert(create_composition_state(
								moving_next_target, static_sync_target, composition_target_level,
								is_remaining_transition_in_lhs
							));
						}
					}
				}
				insert_symbol_post_to_delta(composition_state, std::move(symbol_post));
			}
		};

		// Helper function to perform synchronization on DONT_CARE transitions.
		auto synchronization_on_dont_care = [&](const State dont_care_state, const State other_state,
												const bool is_dont_care_lhs) {
			const Operand& dont_care_nft = is_dont_care_lhs ? lhs : rhs;
			const Operand& other_nft = is_dont_care_lhs ? rhs : lhs;

			auto dont_care_sync_it = dont_care_nft.delta[dont_care_state].find(DONT_CARE);
			if (dont_care_sync_it != dont_care_nft.delta[dont_care_state].end()) {
				for (const SymbolPost& symbol_post : other_nft.delta[other_state]) {
					// DONT_CARE is not synchronized with EPSILON, and DONT_CARE with DONT_CARE has been synchronized
					//  by the synchronized iterator already.
					if (symbol_post.symbol == EPSILON || symbol_post.symbol == DONT_CARE) { continue; }
					const Symbol symbol = reconnect ? reconnection_symbol : symbol_post.symbol;
					if (is_dont_care_lhs) {
						combine_targets(dont_care_sync_it->targets, symbol_post.targets, symbol);
					} else {
						combine_targets(symbol_post.targets, dont_care_sync_it->targets, symbol);
					}
				}
			}
		};

		// The body of the synchronization.
		mata::utils::SynchronizedUniversalIterator<mata::utils::OrdVector<SymbolPost>::const_iterator> sync_iterator(2);
		mata::utils::push_back(sync_iterator, lhs.delta[lhs_state]);
		mata::utils::push_back(sync_iterator, rhs.delta[rhs_state]);
		while (sync_iterator.advance()) {
			const std::vector<StatePost::const_iterator>& same_symbol_posts{sync_iterator.get_current()};
			MATA_ASSERT(same_symbol_posts.size() == 2); // One move per state in the pair.
			const Symbol symbol = reconnect ? reconnection_symbol : same_symbol_posts[0]->symbol;

			if (same_symbol_posts[0]->symbol == EPSILON) {
				// We don't want to synchronize EPSILON with EPSILON.
				// This creates a redundant EPSILON transitions in this implementation.
				// This is an optimization according to https://cs.nyu.edu/~mohri/pub/nway.pdf Figure 3.
				continue;
			}
			combine_targets(same_symbol_posts[0]->targets, same_symbol_posts[1]->targets, symbol);
		}

		synchronization_on_dont_care(lhs_state, rhs_state, true);
		synchronization_on_dont_care(rhs_state, lhs_state, false);
	};

	/**
	 * @brief Copy transitions from the copy NFT to the composition NFT.
	 *
	 * @param composition_state The source state in the composition NFT from which the transitions will be connected.
	 * @param copy_state The state in the copy NFT from which the transitions will be copied.
	 * @param stationar_state The state in the other NFT (the stationary NFT) that does not move.
	 * @param is_copy_state_lhs If true, the copy_state is from the LHS NFT; otherwise, it is from the RHS NFT.
	 * @param waiting_worklist If set, the function is called from the waiting simulation,
	 *                         meaning that we are waiting in the virtual loop at the stationary state,
	 *                         and we will use the waiting_worklist to store the next state pairs.
	 */
	auto copy_transition = [&](const State composition_state, const State copy_state, const State stationar_state,
							   const bool is_copy_state_lhs,
							   const EpsilonFilterMode epsilon_filter_mode = EpsilonFilterMode::LhsMayMoveAlone,
							   std::queue<std::pair<State, State>>* waiting_worklist = nullptr) {
		const auto& copy_sync_props = is_copy_state_lhs ? lhs_sync_props : rhs_sync_props;
		const auto& stationar_sync_props = is_copy_state_lhs ? rhs_sync_props : lhs_sync_props;
		const Operand& copy_nft = copy_sync_props.nft;
		const Level composition_state_level = result.levels[composition_state];
		const Level copy_state_level = copy_nft.levels[copy_state];
		const Level stationar_state_level = stationar_sync_props.nft.levels[stationar_state];
		const Level copy_sync_level = copy_sync_props.sync_level;
		const std::vector<SynchronizationType>& copy_sync_types = copy_sync_props.sync_types_vec;
		const SynchronizationType stationar_state_sync_type = stationar_sync_props.sync_types_vec[stationar_state];

		// It may happen that
		// 1) we want to project out synchronization levels,
		// 2) there are no transitions after the synchronization level in both NFTs, and
		// 3) targets of the copy_state are at the synchronization level.
		// In this case, we are going to need to perform synchronization here and use
		// the copied transitions to connect directly to the next zero-level states.
		const bool handle_synchronization_in_place =
			project_out_sync_levels && stationar_sync_props.num_of_levels_after_sync == 0 &&
			copy_sync_props.num_of_levels_after_sync == 0 &&
			(!is_copy_state_lhs || stationar_sync_props.num_of_levels_before_sync == 0) &&
			(waiting_worklist != nullptr || stationar_state_level == stationar_sync_props.sync_level);

		for (const SymbolPost& copy_symbol_post : copy_nft.delta[copy_state]) {
			SymbolPost symbol_post{copy_symbol_post.symbol};
			for (const State copy_target : copy_symbol_post.targets) {
				if (copy_symbol_post.symbol == EPSILON && copy_state_level == 0 && copy_nft.levels.num_of_levels != 1 &&
					copy_nft.levels[copy_target] == 0) {
					// Skip fast EPSILON transitions.
					continue;
				}

				const Level target_level = copy_nft.levels[copy_target];
				const SynchronizationType copy_target_sync_type = copy_sync_types[copy_target];
				// It makes sense to continue only if we believe that we will not get stuck
				// later due to an inability to synchronize. When this function is called
				// from the waiting simulation, we are interested in only synchronizations on EPSILON.
				// Undefined marks both a target past its synchronization level (or ending the round), which always
				// continues, and a dead end on or before it, which never synchronizes; only the level tells them apart.
				// Until the copied side passes its synchronization level, neither has the stationary one, so both
				//  read their next synchronization symbols in this round and must share one.
				const bool can_synchronize_in_the_future =
					target_level == 0 || target_level > copy_sync_level ||
					(waiting_worklist != nullptr
						 ? exist_intersection(copy_target_sync_type, SynchronizationType::OnlyOnEpsilon)
						 : exist_synchronization(stationar_state_sync_type, copy_target_sync_type) &&
							   copy_sync_props.may_share_a_symbol(copy_target, stationar_sync_props, stationar_state));
				if (!can_synchronize_in_the_future) {
					// There is no way we would be able to synchronize in the future.
					// We do not need to explore this path further.
					continue;
				}

				const size_t trans_len =
					(target_level == 0 ? copy_nft.levels.num_of_levels : target_level) - copy_state_level;
				MATA_ASSERT(target_level == 0 || target_level > copy_state_level);
				MATA_ASSERT(trans_len > 0);

				if (handle_synchronization_in_place && target_level == copy_sync_level) {
					// The target is at the synchronization level that will be projected out.
					// We know that there are no transitions after the synchronization level
					// in any of the NFTs. Therefore, we must perform the synchronization here
					// and connect this transition directly to the next zero-level state.
					if (waiting_worklist != nullptr) {
						// We have been called from the waiting simulation.
						// We are waiting in the stationary state.
						// During this, only EPSILON synchronizations are allowed.
						const auto& copy_eps_symbol_post = copy_nft.delta[copy_target].find(EPSILON);
						// A dead end never gets here: its type has no EPSILON.
						MATA_ASSERT(copy_eps_symbol_post != copy_nft.delta[copy_target].end());
						for (const State sync_target : copy_eps_symbol_post->targets) {
							MATA_ASSERT(copy_nft.levels[sync_target] == 0);
							symbol_post.insert(create_composition_state(
								sync_target, stationar_state, 0, is_copy_state_lhs, epsilon_filter_mode
							));
						}
						// We do not need to add new pairs to the waiting_worklist,
						// because, if necessary, they have already been added to the main
						// worklist by the create_composition_state function.
					} else {
						// We encountered a standard synchronization and need to synchronize the LHS and RHS.
						// Call the synchronization function with the reconnection parameters set.
						// This point can be reached only if there is not transition in the LHS and we are
						// copying transitions from the RHS, or if the synchronization in the RHS occurs
						// on the 0-level, and we are copying transitions from the LHS.
						MATA_ASSERT(!is_copy_state_lhs || rhs_sync_level == 0);
						if (!is_copy_state_lhs) {
							synchronize(composition_state, stationar_state, copy_target, true, copy_symbol_post.symbol);
						} else {
							synchronize(composition_state, copy_target, stationar_state, true, copy_symbol_post.symbol);
						}
					}
				} else {
					// We are not at a synchronization point.
					// We just need to copy the transition.
					const auto new_composition_state_level =
						static_cast<Level>((composition_state_level + trans_len) % result.levels.num_of_levels);
					if (waiting_worklist != nullptr && new_composition_state_level != 0) {
						// We are not connecting to the zero-level state, and we were
						// called from the waiting simulation. Therefore, we need to create an auxiliary state
						// that will not be tracked in the composition_storage. We cannot use composition_storage
						// because, while waiting in the stationary state, we are not exactly at that state,
						// but rather in virtual (nonexistent) states of the waiting loop over it.
						// A waiting state is expanded once: a cache hit only adds the transition.
						State new_composition_state;
						if (const auto it{waiting_state_storage.find(copy_target)}; it != waiting_state_storage.end()) {
							new_composition_state = it->second;
						} else {
							new_composition_state = result.add_state_with_level(new_composition_state_level);
							if (normalize_generated) { generated.add(new_composition_state, GeneratedKind::Waiting); }
							waiting_state_storage.emplace(copy_target, new_composition_state);
							waiting_worklist->emplace(new_composition_state, copy_target);
						}
						// A cached waiting state can be smaller than a target added before it.
						symbol_post.insert(new_composition_state);
					} else {
						// Just copy this transition.
						symbol_post.insert(create_composition_state(
							copy_target, stationar_state, new_composition_state_level, is_copy_state_lhs,
							epsilon_filter_mode
						));
					}
				}
			}
			if (!symbol_post.targets.empty()) {
				insert_symbol_post_to_delta(composition_state, std::move(symbol_post));
			}
		}
	};

	/**
	 * @brief Models the "waiting" in one of the NFTs when it cannot synchronize
	 * with the other NFT doe to a lacking EPSILON on the synchronization level.
	 *
	 * For example, if there is an EPSILON transition at the synchronization level
	 * in the RHS, and the LHS cannot synchronize on it, then the LHS must "wait"
	 * in its root (zero-level) state that precedes the synchronization. Meanwhile,
	 * the RHS continues from its corresponding root state, traversing the problematic
	 * EPSILON synchronization to the next zero-level state. The RHS behaves as if it
	 * is interleaving with the LHS transitions (LHS transitions always occur before RHS
	 * transitions), but it will insert an EPSILON for each such transition from the LHS.
	 *
	 * A special case arises when we want to project out synchronization levels,
	 * but there are no transitions following them in either the LHS or RHS (i.e.,
	 * the synchronization level is the last level in both NFTs). In this situation,
	 * one state before the synchronization level, we need to connect the successors
	 * of such states to the zero-level state that follows the synchronization level.
	 *
	 * @param composition_root_state The root state of the composition NFT.
	 * @param waiting_root_state The root state of the NFT that is waiting (either LHS or RHS).
	 * @param running_root_state The root state of the NFT that is running (either LHS or RHS).
	 * @param is_lhs_waiting If true, the waiting root is from the LHS;
	 *                       otherwise, it is from the RHS.
	 * @param epsilon_filter_mode The EPSILON-filter mode the zero-level successors of this one-sided
	 *                            move are reached in.
	 */
	auto model_waiting = [&](const State composition_root_state, const State waiting_root_state,
							 const State running_root_state, const bool is_lhs_waiting,
							 const EpsilonFilterMode epsilon_filter_mode) {
		waiting_state_storage.clear();
		const auto& running_sync_props = is_lhs_waiting ? rhs_sync_props : lhs_sync_props;
		const auto& waiting_sync_props = is_lhs_waiting ? lhs_sync_props : rhs_sync_props;
		const Level sync_level = running_sync_props.sync_level;
		const auto& running_levels = running_sync_props.nft.levels;
		const auto& running_delta = running_sync_props.nft.delta;
		const size_t running_num_of_levels = running_sync_props.nft.levels.num_of_levels;
		const size_t running_trans_after_sync = running_sync_props.num_of_levels_after_sync;
		const size_t waiting_trans_before_sync = waiting_sync_props.num_of_levels_before_sync;
		const size_t waiting_trans_after_sync = waiting_sync_props.num_of_levels_after_sync;
		// EPSILON synchronization is handled differently here than in the main loop.
		// To avoid redundant transitions caused by early branching, we replace each
		// EPSILON synchronization with N EPSILON transitions, where:
		// - Add waiting_trans_before_sync to N if NOT waiting in LHS
		//   (RHS transitions go after LHS, just before the synchronization).
		// - Add waiting_trans_after_sync to N if waiting in LHS, or if there are no
		//   running transitions after the synchronization.
		// - Add 1 to N if synchronization levels are NOT being projected out.
		// The first N-1 EPSILON transitions are common for all targets,
		// and only the last EPSILON transition is used to connect to the appropriate targets.

		const size_t num_of_epsilons_to_replace_sync =
			((is_lhs_waiting ? 0 : waiting_trans_before_sync) +
			 ((is_lhs_waiting || running_trans_after_sync == 0) ? waiting_trans_after_sync : 0) +
			 (project_out_sync_levels ? 0 : 1));

		// A special case we need to handle here.
		// In some cases, the running NFT has only one level.
		// This can happen when applying NFA to NFT. In that case,
		// we can and need to simply add EPSILON transitions to the targets
		// of the EPSILON transitions from the running root state.
		if (running_num_of_levels == 1) {
			// The EPSILON move of a one-level operand is a whole round of EPSILON in the result, however many
			//  EPSILONs would replace a synchronization elsewhere; none, e.g., when the LHS waits on its last
			//  level, which is the projected synchronization level.
			const auto epsilon_target_it = running_delta[running_root_state].find(EPSILON);
			// There has to be some EPSILON transition. Otherwise, there would be no reason to wait.
			MATA_ASSERT(epsilon_target_it != running_delta[running_root_state].end());
			const StateSet new_composition_targets;
			const State new_composition_state =
				create_epsilon_transition_with_common_path(composition_root_state, result.levels.num_of_levels - 1);
			SymbolPost symbol_post{EPSILON};
			for (const State epsilon_target : epsilon_target_it->targets) {
				MATA_ASSERT(running_levels[epsilon_target] == 0);
				symbol_post.insert(
					create_composition_state(waiting_root_state, epsilon_target, 0, is_lhs_waiting, epsilon_filter_mode)
				);
			}
			// This function ensures that the transitions are added in the most optimal way.
			// It creates one common path for all targets and then use only the last transitions of
			// that path to connect to all targets.
			insert_symbol_post_to_delta(new_composition_state, std::move(symbol_post));
			return;
		}

		// Initialization of the local worklist.
		// I chose to use a queue instead of a stack to help the branch predictor
		// with conditions inside the loop. With the queue, the algorithm hits the
		// same branch more often, because all states being explored advance "together".
		std::queue<std::pair<State, State>> worklist;
		State first_composition_state = composition_root_state;
		if (is_lhs_waiting && waiting_trans_before_sync > 0) {
			// Since LHS is waiting and LHS transitions have to be put before
			// RHS transitions, we now add waiting_trans_before_sync EPSILON
			// transitions. Doing this here simplifies the conditions in the loop below.
			first_composition_state =
				create_epsilon_transition_with_common_path(composition_root_state, waiting_trans_before_sync);
		}
		worklist.emplace(first_composition_state, running_root_state);

		// The main loop of the waiting simulation.
		while (!worklist.empty()) {
			auto [composition_state, running_state] = worklist.front();
			worklist.pop();
			const Level running_state_level = running_levels[running_state];
			// If the synchronization level is on 0. Than we can be careful to not use it
			// again when encountering next zero-level states in the running NFT.
			const bool first_running_transition =
				(running_state == running_root_state && composition_state == first_composition_state);
			const bool is_sync_level =
				running_state_level == sync_level && (running_state_level != 0 || first_running_transition);
			if (is_sync_level) {
				// We are at the synchronization level.
				const bool is_last_running_transition =
					(running_state_level == running_sync_props.nft.levels.num_of_levels - 1);

				// It cannot happen that the synchronization level is projected out
				// and there are no other transitions to add before reaching the next zero-level state.
				// This is because such a scenario would already be handled by the copy_transition function.
				MATA_ASSERT(
					!project_out_sync_levels || !is_last_running_transition || waiting_trans_after_sync > 0 ||
					(!is_lhs_waiting && waiting_sync_props.num_of_levels_before_sync > 0)
				);

				// Do the EPSILON synchronization.
				// We cannot use the synchronize function here, because we want to
				// focus only on the EPSILON transitions.
				const auto& running_epsilon_post_it = running_delta[running_state].find(EPSILON);
				// There should be an EPSILON transition. It has been told by the SynchronizationType.
				MATA_ASSERT(running_epsilon_post_it != running_delta[running_state].end());

				if (num_of_epsilons_to_replace_sync > 1) {
					// The synchronization will be replaced by an EPSILON transition anyway.
					// To reduce the number of redundant EPSILON transitions in the resulting NFT,
					// we can add N-1 EPSILON transitions now, and then use the last one to connect
					// to the adequate target state, where N is num_of_epsilons_to_replace_sync.
					composition_state = create_epsilon_transition_with_common_path(
						composition_state, num_of_epsilons_to_replace_sync - 1, GeneratedKind::ProjectedBridge
					);
				}

				SymbolPost epsilon_symbol_post{EPSILON};
				for (const State running_epsilon_target : running_epsilon_post_it->targets) {
					if (running_state_level == 0 && running_num_of_levels != 1 &&
						running_levels[running_epsilon_target] == 0) {
						// Skip fast EPSILON transitions.
						continue;
					}
					MATA_ASSERT(
						running_levels[running_epsilon_target] == 0 ||
						running_levels[running_epsilon_target] > running_state_level
					);
					if (num_of_epsilons_to_replace_sync == 0 && !is_last_running_transition) {
						// This synchronization level is being projected out (it will vanish).
						// There are no EPSILON transitions to add before or after the synchronization level.
						// However, we are sure, that there will be a least one "running" transition after this.
						worklist.emplace(composition_state, running_epsilon_target);
					} else if (is_last_running_transition) {
						// We are connecting to the zero-level state, therefore we have to
						// create a new composition state that will be put into the main worklist
						// (side effect of the create_composition_state).
						MATA_ASSERT(result.levels[composition_state] + 1 == result.levels.num_of_levels);
						MATA_ASSERT(
							result.levels[composition_state] + (num_of_epsilons_to_replace_sync >= 1 ? 1 : 0) ==
							result.levels.num_of_levels
						);
						epsilon_symbol_post.insert(create_composition_state(
							waiting_root_state, running_epsilon_target, 0, is_lhs_waiting, epsilon_filter_mode
						));
					} else {
						// We are connecting to the next state in the waiting loop,
						// so we simply add a transition to the next state and
						// continue the waiting.
						MATA_ASSERT(result.levels[composition_state] + 1 < result.levels.num_of_levels);
						State new_composition_state;
						if (const auto it{waiting_state_storage.find(running_epsilon_target)};
							it != waiting_state_storage.end()) {
							new_composition_state = it->second;
						} else {
							new_composition_state = result.add_state_with_level(result.levels[composition_state] + 1);
							if (normalize_generated) { generated.add(new_composition_state, GeneratedKind::Waiting); }
							waiting_state_storage.emplace(running_epsilon_target, new_composition_state);
							worklist.emplace(new_composition_state, running_epsilon_target);
						}
						// A cached waiting state can be smaller than a target added before it.
						epsilon_symbol_post.insert(new_composition_state);
					}
				}
				if (!epsilon_symbol_post.targets.empty()) {
					insert_symbol_post_to_delta(composition_state, std::move(epsilon_symbol_post));
				}
			} else if (running_state_level == 0 && !first_running_transition) {
				// We are at a leaf zero-level state in the running NFT.
				// We will create a connection to the zero-level state in the composition NFT
				// and continue exploring that state in the main composition loop
				// (i.e., we push it to the main worklist, if not already visited).
				// Note: This situation can only occur if the waiting NFT is the RHS,
				// because RHS transitions follow LHS transitions. Moreover, the last
				// transition in the running LHS NFT should not be at the synchronization
				// level; otherwise, it would be handled by the condition above.
				MATA_ASSERT(!is_lhs_waiting);
				MATA_ASSERT(sync_level != running_sync_props.nft.levels.num_of_levels - 1);
				MATA_ASSERT(waiting_trans_after_sync > 0);
				MATA_ASSERT(
					(result.levels[composition_state] + waiting_trans_after_sync) == result.levels.num_of_levels
				);
				create_epsilon_transition_with_target(
					composition_state,
					create_composition_state(running_state, waiting_root_state, 0, true, epsilon_filter_mode)
				);
			} else {
				// We are at an internal (non-sync) level in the running NFT.
				// We can simply copy the transitions to the next states.
				// We do not need to worry that the target state might be at the
				// synchronization level with no further transitions in either
				// the LHS or RHS after it, because such cases will be handled
				// inside the copy_transition function.
				copy_transition(
					composition_state, running_state, waiting_root_state, !is_lhs_waiting, epsilon_filter_mode,
					&worklist
				);
			}
		}
	};

	/**
	 * @brief Process potential fast EPSILON transitions (transitions over EPSILON,
	 * between two zero-level states).
	 *
	 * Fast EPSILON moves of both sides emit EPSILONs only, so the LHS-only moves, the RHS-only
	 * moves and the simultaneous moves all cover the same advance. The EPSILON filter keeps the
	 * canonical route through them: the simultaneous (Cartesian) move equals the LHS-then-RHS
	 * route and is dropped, the LHS-only moves of the other interleavings are refused by the
	 * filter mode, and every allowed RHS-only move either switches the filter or stays in the
	 * LhsMayMoveAlone mode when the LHS cannot move alone anymore.
	 *
	 * @param composition_state The state in the resulting composition NFT.
	 * @param lhs_src The source state of a potential EPSILON transition in the LHS NFT.
	 * @param rhs_src The source state of a potential EPSILON transition in the RHS NFT.
	 * @param epsilon_filter_mode The filter mode of the source configuration.
	 * @param lhs_has_a_silent_lhs_only_move Whether the LHS state can still move alone silently.
	 * @param lhs_gets_stuck_without_moving Whether every move of the LHS state is a silent LHS-only
	 *                                      move and the LHS state is not final.
	 */
	auto process_fast_epsilon_transitions = [&](const State composition_state, const State lhs_src, const State rhs_src,
												const EpsilonFilterMode epsilon_filter_mode,
												const bool lhs_has_a_silent_lhs_only_move,
												const bool lhs_gets_stuck_without_moving) {
		MATA_ASSERT(lhs.levels[lhs_src] == 0 && rhs.levels[rhs_src] == 0);
		const auto lhs_eps_post_it = lhs.delta[lhs_src].find(EPSILON);
		const auto rhs_eps_post_it = rhs.delta[rhs_src].find(EPSILON);
		const bool lhs_eps_exists = lhs_eps_post_it != lhs.delta[lhs_src].end();
		const bool rhs_eps_exists = rhs_eps_post_it != rhs.delta[rhs_src].end();
		// Without an EPSILON post on either side there is no fast EPSILON transition, and nothing to allocate for.
		if (!lhs_eps_exists && !rhs_eps_exists) { return; }

		StateSet filtered_lhs_fast_eps_targets{lhs_src};
		StateSet filtered_rhs_fast_eps_targets{rhs_src};
		if (lhs_eps_exists && lhs.levels.num_of_levels != 1) {
			std::copy_if(
				lhs_eps_post_it->targets.cbegin(), lhs_eps_post_it->targets.cend(),
				std::back_inserter(filtered_lhs_fast_eps_targets), [&](State s) { return lhs.levels[s] == 0; }
			);
		}
		if (rhs_eps_exists && rhs.levels.num_of_levels != 1) {
			std::copy_if(
				rhs_eps_post_it->targets.cbegin(), rhs_eps_post_it->targets.cend(),
				std::back_inserter(filtered_rhs_fast_eps_targets), [&](State s) { return rhs.levels[s] == 0; }
			);
		}

		if (filtered_lhs_fast_eps_targets.size() == 1 && filtered_rhs_fast_eps_targets.size() == 1) {
			// There are no fast EPSILON transitions to process.
			return;
		}

		// The one-sided fast EPSILON moves the filter allows, with the filter mode of their targets.
		std::vector<State> lhs_only_targets;
		std::vector<std::pair<State, EpsilonFilterMode>> rhs_only_targets;
		if (epsilon_filter_mode == EpsilonFilterMode::LhsMayMoveAlone) {
			for (const State lhs_target : filtered_lhs_fast_eps_targets) {
				if (lhs_target != lhs_src) { lhs_only_targets.push_back(lhs_target); }
			}
		}
		for (const State rhs_target : filtered_rhs_fast_eps_targets) {
			if (rhs_target == rhs_src) { continue; }
			if (const std::optional<EpsilonFilterMode> next_mode = epsilon_filter_step(
					epsilon_filter_mode, EpsilonFilterAction::SilentRhsOnlyMove, lhs_has_a_silent_lhs_only_move,
					lhs_gets_stuck_without_moving
				)) {
				rhs_only_targets.emplace_back(rhs_target, *next_mode);
			}
		}
		if (lhs_only_targets.empty() && rhs_only_targets.empty()) {
			// The filter refused every fast EPSILON move.
			return;
		}

		// Create a common path of EPSILON transitions to connect to all the allowed one-sided moves.
		// Branch only at the last level of that common path.
		MATA_ASSERT(result.levels.num_of_levels > 0);
		const size_t common_path_len = result_num_of_levels - 1;
		const State source = create_epsilon_transition_with_common_path(composition_state, common_path_len);

		SymbolPost symbol_post{EPSILON};
		for (const State lhs_target : lhs_only_targets) {
			symbol_post.insert(
				create_composition_state(lhs_target, rhs_src, 0, true, EpsilonFilterMode::LhsMayMoveAlone)
			);
		}
		for (const auto& [rhs_target, next_mode] : rhs_only_targets) {
			symbol_post.insert(create_composition_state(lhs_src, rhs_target, 0, true, next_mode));
		}
		insert_symbol_post_to_delta(source, std::move(symbol_post));
	};

	// Initialization of the main worklist.
	for (const State lhs_root : lhs.initial) {
		for (const State rhs_root : rhs.initial) {
			// Get the root state in the result NFT
			result.initial.insert(create_composition_state(lhs_root, rhs_root, 0));
		}
	}

	// The main loop of the composition algorithm, which stops between two configurations.
	const auto build_until = [&](const size_t max_num_of_states) -> bool {
		while (not worklist.empty()) {
			// Every added state is allocated in the delta, whose size is inline; Nft::num_of_states() is a call.
			if (result.delta.num_of_states() > max_num_of_states) { return false; }
			const CompositionKey<UseFilter> key{worklist.front()};
			worklist.pop();
			const State lhs_state{key.lhs_state};
			const State rhs_state{key.rhs_state};
			const EpsilonFilterMode epsilon_filter_mode{key.mode()};
			const State composition_state{key.composition_state};
			MATA_ASSERT(composition_state == storage_for(epsilon_filter_mode).get(lhs_state, rhs_state));
			const Level lhs_level = lhs.levels[lhs_state];
			const Level rhs_level = rhs.levels[rhs_state];
			const SynchronizationType lhs_sync_type = lhs_sync_props.sync_types_vec[lhs_state];
			const SynchronizationType rhs_sync_type = rhs_sync_props.sync_types_vec[rhs_state];

			if (lhs_level == 0 and rhs_level == 0) {
				// We are at the zero-level states before any synchronization level.
				// Now is the time to decide whether to continue synchronization and/or
				// to wait in the lhs_state and/or rhs_state. This is necessary when
				// there is a synchronization where one of the transducers has an EPSILON
				// and the other has an alphabet symbol. Note that if future synchronization
				// is impossible due to the EPSILON, we will simulate waiting in the zero-level
				// state of the transducer that does not have that EPSILON.

				// TODO: Optimize/reduce the number of similar combination of waiting simulations.
				// You can see the table defining operations for pair of synchronization types
				// in the documentation of the SynchronizationType enum class.
				const bool perform_wait_on_lhs = exist_intersection(rhs_sync_type, SynchronizationType::OnlyOnEpsilon);
				const bool perform_wait_on_rhs = exist_intersection(lhs_sync_type, SynchronizationType::OnlyOnEpsilon);
				const bool can_synchronize_in_the_future =
					exist_synchronization(lhs_sync_type, rhs_sync_type) &&
					lhs_sync_props.may_share_a_symbol(lhs_state, rhs_sync_props, rhs_state);

				// The inputs of the EPSILON-composition filter. A silent LHS-only move is a fast EPSILON
				// move of the LHS or a waiting round of a one-level LHS. The LHS gets stuck once the
				// filter refuses its silent moves if every move of its state is a silent LHS-only move
				// (and the state is not final); in that case a silent move of the RHS would lead nowhere.
				// The filter reads both only for a silent RHS-only move, so they are not computed for the pairs
				// where the RHS cannot move alone silently, nor at all without the filter.
				const bool rhs_may_move_alone_silently{
					UseFilter && ((perform_wait_on_lhs && rhs_sync_props.nft.levels.num_of_levels == 1) ||
								  has_fast_epsilon_move(rhs_sync_props.nft, rhs_state))
				};
				const bool lhs_has_a_silent_lhs_only_move =
					rhs_may_move_alone_silently &&
					(has_fast_epsilon_move(lhs_sync_props.nft, lhs_state) ||
					 (perform_wait_on_rhs && lhs_sync_props.nft.levels.num_of_levels == 1));
				const bool lhs_gets_stuck_without_moving = rhs_may_move_alone_silently &&
														   !lhs.final.contains(lhs_state) &&
														   has_only_silent_lhs_only_moves(lhs, lhs_state);

				// Process potential fast EPSILON transitions.
				process_fast_epsilon_transitions(
					composition_state, lhs_state, rhs_state, epsilon_filter_mode, lhs_has_a_silent_lhs_only_move,
					lhs_gets_stuck_without_moving
				);

				if (perform_wait_on_lhs) {
					// LHS is waiting (i.e., there is an EPSILON in the RHS that LHS can not synchronize on).
					const bool is_a_silent_move = rhs_sync_props.nft.levels.num_of_levels == 1;
					const std::optional<EpsilonFilterMode> next_mode = epsilon_filter_step(
						epsilon_filter_mode,
						is_a_silent_move ? EpsilonFilterAction::SilentRhsOnlyMove
										 : EpsilonFilterAction::NonSilentRhsOnlyMove,
						lhs_has_a_silent_lhs_only_move, lhs_gets_stuck_without_moving
					);
					if (next_mode) { model_waiting(composition_state, lhs_state, rhs_state, true, *next_mode); }
				}
				if (perform_wait_on_rhs) {
					// RHS is waiting (i.e., there is an EPSILON in the LHS that RHS can not synchronize on).
					const bool is_a_silent_move = lhs_sync_props.nft.levels.num_of_levels == 1;
					const std::optional<EpsilonFilterMode> next_mode = epsilon_filter_step(
						epsilon_filter_mode,
						is_a_silent_move ? EpsilonFilterAction::SilentLhsOnlyMove
										 : EpsilonFilterAction::NonSilentLhsOnlyMove,
						lhs_has_a_silent_lhs_only_move, lhs_gets_stuck_without_moving
					);
					if (next_mode) { model_waiting(composition_state, rhs_state, lhs_state, false, *next_mode); }
				}
				if (!can_synchronize_in_the_future) {
					// There is no way to perform the synchronization after these states.
					// Therefore, it does not make sense to continue exploring this pair.
					continue;
				}
			}

			// It makes sense to continue only if we believe that synchronization is possible,
			// or if we have already passed the synchronization level (i.e., lhs_sync_type ==
			// SynchronizationType::Undefined or rhs_sync_type == SynchronizationType::Undefined).
			MATA_ASSERT(
				exist_synchronization(lhs_sync_type, rhs_sync_type) ||
				(lhs_level == 0 && rhs_level > 0 && rhs_sync_type == SynchronizationType::Undefined) ||
				(rhs_level == 0 && lhs_level > 0 && lhs_sync_type == SynchronizationType::Undefined) ||
				(lhs_level > lhs_sync_level && rhs_level > rhs_sync_level &&
				 lhs_sync_type == SynchronizationType::Undefined && rhs_sync_type == SynchronizationType::Undefined)
			);

			// Both LHS and RHS can take a step if they are not at the synchronization level.
			// The LHS always moves before the RHS (i.e., lhs_level < rhs_level).
			// However, there is a special case when lhs_level < rhs_level, but the LHS cannot move.
			// This occurs when the LHS has already reached its last (leave) zero-level state,
			// which happens if there are no transitions after the synchronization level in the LHS.
			const bool do_step_in_lhs = lhs_level != lhs_sync_level && (lhs_level != 0 || rhs_level == 0);
			const bool do_step_in_rhs = rhs_level != rhs_sync_level;

			if (do_step_in_lhs) {
				// Always try to do the step in the LHS first.
				// We don't need to worry about the case when this is the last
				// transition in the LHS, the RHS is already at the synchronization level,
				// and we are projecting out the synchronization levels.
				// In such situation, the copy_transition function will handle it.
				copy_transition(composition_state, lhs_state, rhs_state, true);
			} else if (do_step_in_rhs) {
				// LHS is probably at the synchronization level or at the last (leave) zero-level state.
				// It's time for RHS to make a step.
				// We don't need to worry about the case when this is the last
				// transition in the RHS, the LHS is already at the synchronization level,
				// and we are projecting out the synchronization levels.
				// In such situation, the copy_transition function will handle it.
				copy_transition(composition_state, rhs_state, lhs_state, false);
			} else {
				// Both LHS and RHS are at the synchronization level.
				// Synchronization at this point is only possible if:
				// 1) There is at least one transition in either the LHS or
				//    RHS that follows the synchronization level
				//    (so we can connect to their successors), or
				// 2) We are not projecting out the synchronization levels.
				//    In this case, we use the synchronization transition to
				//    connect to the zero-level successors that follow.
				MATA_ASSERT(lhs_level == lhs_sync_level && rhs_level == rhs_sync_level);
				MATA_ASSERT(
					!project_out_sync_levels || lhs_sync_props.num_of_levels_after_sync > 0 ||
					rhs_sync_props.num_of_levels_after_sync > 0
				);
				synchronize(composition_state, lhs_state, rhs_state);
			}
		}
		return true;
	};
	if (!schedule(build_until)) { return std::nullopt; }

	if (normalize_generated) {
		// One compaction serves both the normalization and the trim.
		GeneratedStatesNormalizer{result, generated}.run(project_out_sync_levels);
	} else if (project_out_sync_levels) {
		// TODO: Make trim on demand.
		result.trim();
	}

	return result;
}

/// Run @c compose_fast_with() with the EPSILON-composition filter if @p rhs needs it. The filter switches only on a
///  silent move of the RHS alone. Without one it keeps every configuration in the LhsMayMoveAlone mode and refuses
///  nothing, so the composition runs without its bookkeeping.
template <typename Operand, typename Schedule>
std::optional<Nft> compose_fast_choosing_filter(
	const Operand& lhs,
	const Operand& rhs,
	const Level lhs_sync_level,
	const Level rhs_sync_level,
	const bool project_out_sync_levels,
	const bool normalize_generated,
	Schedule&& schedule
) {
	return can_move_alone_silently(rhs)
			   ? compose_fast_with<true>(
					 lhs, rhs, lhs_sync_level, rhs_sync_level, project_out_sync_levels, normalize_generated, schedule
				 )
			   : compose_fast_with<false>(
					 lhs, rhs, lhs_sync_level, rhs_sync_level, project_out_sync_levels, normalize_generated, schedule
				 );
}

/// @c compose_fast_choosing_filter() built in one go.
template <typename Operand>
Nft compose_fast_at_once(
	const Operand& lhs,
	const Operand& rhs,
	const Level lhs_sync_level,
	const Level rhs_sync_level,
	const bool project_out_sync_levels,
	const bool normalize_generated
) {
	std::optional<Nft> result{compose_fast_choosing_filter(
		lhs, rhs, lhs_sync_level, rhs_sync_level, project_out_sync_levels, normalize_generated,
		[](const auto& build_until) { return build_until(SIZE_MAX); }
	)};
	MATA_ASSERT(result.has_value());
	return std::move(*result);
}

/// The states a composition may build before the other direction takes its first turn; a composition this small
///  takes milliseconds, so most compositions are built forward only.
constexpr size_t INITIAL_DIRECTION_BUDGET{size_t{1} << 16};

/**
 * @brief The trimmed fast composition of operands without jump transitions, built forward or as the reverse of the
 *  composition of the reverted operands, whichever finishes first.
 *
 * Reverting an NFT reverses the order of its levels (@c revert()), so the reverted composition synchronizes level
 *  `n - 1 - l` of an operand of `n` levels instead of level `l`, and composing the reverted RHS with the reverted LHS
 *  keeps the order of the levels of the result. The forward composition builds every state reachable from the
 *  initial states, the reverted one every state from which a final state is reachable; trimming keeps the states
 *  that are both, so either direction gives the same trimmed relation. On the transducers of string constraints,
 *  either direction can build tens of times more states than the other, and only building them tells which. The two
 *  directions take turns with a state budget doubled every round. The forward composition of the first turn is
 *  thrown away: most compositions finish in it, or in the first turn of the reverted one, which is built faster
 *  in the memory the forward one has just freed. From then on each direction continues where it stopped, so the
 *  composition builds at most a small multiple of the states of the cheaper direction. The forward composer then
 *  runs inside the schedule of the reverted one, which keeps both ordinary functions: suspending the composer as a
 *  coroutine makes every composition slower, also those that never take turns. The result has no alphabets.
 */
Nft compose_fast_trimmed(
	const Nft& lhs,
	const Nft& rhs,
	const Level lhs_sync_level,
	const Level rhs_sync_level,
	const bool project_out_sync_levels
) {
	// The composer trims a result without the synchronization levels itself.
	const auto trimmed = [&](Nft& result) -> Nft& {
		if (!project_out_sync_levels) { result.trim(); }
		return result;
	};
	const auto compose_forward = [&](auto&& schedule) {
		return compose_fast_choosing_filter(
			lhs, rhs, lhs_sync_level, rhs_sync_level, project_out_sync_levels, false, schedule
		);
	};
	size_t budget{INITIAL_DIRECTION_BUDGET};
	if (std::optional<Nft> forward{compose_forward([&](const auto& build_forward) { return build_forward(budget); })};
		forward.has_value()) {
		return std::move(trimmed(*forward));
	}
	const Nft reverted_lhs{revert(lhs)};
	const Nft reverted_rhs{revert(rhs)};
	std::optional<Nft> forward{};
	std::optional<Nft> backward{compose_fast_choosing_filter(
		reverted_rhs, reverted_lhs, static_cast<Level>(rhs.levels.num_of_levels - 1 - rhs_sync_level),
		static_cast<Level>(lhs.levels.num_of_levels - 1 - lhs_sync_level), project_out_sync_levels, false,
		[&](const auto& build_backward) {
			if (build_backward(budget)) { return true; }
			bool backward_finished{false};
			forward = compose_forward([&](const auto& build_forward) {
				while (true) {
					budget = budget > SIZE_MAX / 2 ? SIZE_MAX : budget * 2;
					if (build_forward(budget)) { return true; }
					if (build_backward(budget)) {
						backward_finished = true;
						return false;
					}
				}
			});
			return backward_finished;
		}
	)};
	if (forward.has_value()) { return std::move(trimmed(*forward)); }
	return revert(trimmed(*backward));
}

/// The body of @c compose_fast() and @c compose_fast_no_jump(), which name themselves @p caller in their errors.
Nft compose_fast_checked(
	const std::string_view caller,
	const Nft& lhs,
	const Nft& rhs,
	const Level lhs_sync_level,
	const Level rhs_sync_level,
	const bool project_out_sync_levels,
	const JumpMode jump_mode,
	const CompositionNormalization normalization
) {
	check_sync_level_in_range(caller, "lhs", lhs, lhs_sync_level);
	check_sync_level_in_range(caller, "rhs", rhs, rhs_sync_level);
	check_result_num_of_levels(caller, lhs, rhs, 1, project_out_sync_levels);
	const bool normalize_generated{
		normalization == CompositionNormalization::GeneratedOnly || normalization == CompositionNormalization::Full
	};
	// Operands that read as they are race both directions only when the caller asks for the trimmed result: on most
	//  compositions the forward one is the cheaper, and the turns of the reverted one are wasted there.
	const auto compose_plain = [&]() {
		if (normalization == CompositionNormalization::Trim) {
			return compose_fast_trimmed(lhs, rhs, lhs_sync_level, rhs_sync_level, project_out_sync_levels);
		}
		return compose_fast_at_once(
			lhs, rhs, lhs_sync_level, rhs_sync_level, project_out_sync_levels, normalize_generated
		);
	};
	Nft result{};
	if (jump_mode == JumpMode::NoJump) {
		MATA_ASSERT(
			std::all_of(
				lhs.delta.transitions().begin(), lhs.delta.transitions().end(),
				[&](const Transition& transition) {
					return static_cast<Level>((lhs.levels[transition.source] + 1) % lhs.levels.num_of_levels) ==
							   lhs.levels[transition.target] ||
						   (lhs.levels[transition.source] == 0 && lhs.levels[transition.target] == 0 &&
							transition.symbol == EPSILON);
				}
			),
			"There are only explicit synchronization transitions of length 1 with exception for fast EPSILON "
			"transitions"
		);
		MATA_ASSERT(
			std::all_of(
				rhs.delta.transitions().begin(), rhs.delta.transitions().end(),
				[&](const Transition& transition) {
					return static_cast<Level>((rhs.levels[transition.source] + 1) % rhs.levels.num_of_levels) ==
							   rhs.levels[transition.target] ||
						   (rhs.levels[transition.source] == 0 && rhs.levels[transition.target] == 0 &&
							transition.symbol == EPSILON);
				}
			),
			"There are only explicit synchronization transitions of length 1 with exception for fast EPSILON "
			"transitions"
		);
		result = compose_plain();
	} else {
		const CursorOperand lhs_operand{lhs, jump_mode};
		const CursorOperand rhs_operand{rhs, jump_mode};
		// Without a cursor the operands read as they are, and the composer reads them without the indirection.
		if (lhs_operand.num_of_states() == lhs.num_of_states() && rhs_operand.num_of_states() == rhs.num_of_states()) {
			result = compose_plain();
		} else {
			result = compose_fast_at_once(
				lhs_operand, rhs_operand, lhs_sync_level, rhs_sync_level, project_out_sync_levels, normalize_generated
			);
			if (normalization == CompositionNormalization::Trim && !project_out_sync_levels) { result.trim(); }
		}
	}
	result.alphabets = compose_alphabets(
		lhs, rhs, OrdVector<Level>{lhs_sync_level}, OrdVector<Level>{rhs_sync_level}, project_out_sync_levels
	);
	// The result has no jump transitions, whatever the jump mode of the operands.
	if (normalization == CompositionNormalization::Full) { normalize_fully(result, JumpMode::NoJump); }
	return result;
}
} // namespace
} // namespace algorithms

Nft algorithms::compose_fast(
	const Nft& lhs,
	const Nft& rhs,
	const Level lhs_sync_level,
	const Level rhs_sync_level,
	const bool project_out_sync_levels,
	const JumpMode jump_mode,
	const CompositionNormalization normalization
) {
	return compose_fast_checked(
		"compose_fast", lhs, rhs, lhs_sync_level, rhs_sync_level, project_out_sync_levels, jump_mode, normalization
	);
}

Nft algorithms::compose_fast_no_jump(
	const Nft& lhs,
	const Nft& rhs,
	const Level lhs_sync_level,
	const Level rhs_sync_level,
	const bool project_out_sync_levels,
	const CompositionNormalization normalization
) {
	return compose_fast_checked(
		"compose_fast_no_jump", lhs, rhs, lhs_sync_level, rhs_sync_level, project_out_sync_levels, JumpMode::NoJump,
		normalization
	);
}

algorithms::CompositionSchedule::CompositionSchedule(
	const size_t lhs_num_of_levels,
	const size_t rhs_num_of_levels,
	const OrdVector<Level>& lhs_sync_levels,
	const OrdVector<Level>& rhs_sync_levels
) {
	MATA_ASSERT(lhs_sync_levels.size() == rhs_sync_levels.size());
	positions.reserve(lhs_num_of_levels + rhs_num_of_levels - lhs_sync_levels.size());
	Level lhs_level{0};
	Level rhs_level{0};
	// The levels only one operand reads before its level @p lhs_end or @p rhs_end: those of `lhs` first.
	auto append_one_sided_levels = [&](const size_t lhs_end, const size_t rhs_end) {
		// Once an operand has read its last level, its state is a zero-level state again.
		const Level waiting_rhs_level{rhs_level == rhs_num_of_levels ? Level{0} : rhs_level};
		for (; lhs_level < lhs_end; ++lhs_level) { positions.push_back({Owner::Lhs, lhs_level, waiting_rhs_level}); }
		const Level waiting_lhs_level{lhs_level == lhs_num_of_levels ? Level{0} : lhs_level};
		for (; rhs_level < rhs_end; ++rhs_level) { positions.push_back({Owner::Rhs, waiting_lhs_level, rhs_level}); }
	};
	for (auto lhs_sync_it{lhs_sync_levels.cbegin()}, rhs_sync_it{rhs_sync_levels.cbegin()};
		 lhs_sync_it != lhs_sync_levels.cend(); ++lhs_sync_it, ++rhs_sync_it) {
		append_one_sided_levels(*lhs_sync_it, *rhs_sync_it);
		positions.push_back({Owner::Sync, lhs_level++, rhs_level++});
	}
	append_one_sided_levels(lhs_num_of_levels, rhs_num_of_levels);
}

BoolVector algorithms::CompositionSchedule::lhs_new_levels_mask() const {
	BoolVector mask(positions.size(), false);
	for (size_t i{0}; i < positions.size(); ++i) { mask[i] = positions[i].owner == Owner::Rhs; }
	return mask;
}

BoolVector algorithms::CompositionSchedule::rhs_new_levels_mask() const {
	BoolVector mask(positions.size(), false);
	for (size_t i{0}; i < positions.size(); ++i) { mask[i] = positions[i].owner == Owner::Lhs; }
	return mask;
}

OrdVector<Level> algorithms::CompositionSchedule::sync_positions() const {
	OrdVector<Level> sync_positions{};
	for (size_t i{0}; i < positions.size(); ++i) {
		if (positions[i].owner == Owner::Sync) { sync_positions.push_back(static_cast<Level>(i)); }
	}
	return sync_positions;
}

namespace {
using algorithms::CompositionSchedule;

/// Whether the direct general composition reads @p nft as the pipeline of @c compose_general_reference() does: every
///  transition goes to a later level of its round or ends the round.
bool is_read_directly(const Nft& nft) {
	const size_t num_of_levels{nft.levels.num_of_levels};
	for (State source{0}; source < nft.num_of_states(); ++source) {
		const Level source_level{nft.levels[source]};
		if (source_level >= num_of_levels) { return false; }
		for (const SymbolPost& symbol_post : nft.delta[source]) {
			for (const State target : symbol_post.targets) {
				const Level target_level{nft.levels[target]};
				if (target_level >= num_of_levels || (target_level != 0 && target_level <= source_level)) {
					return false;
				}
			}
		}
	}
	return true;
}

/// Whether @p symbol matches the EPSILON a waiting operand reads: EPSILON itself, or DONT_CARE, which the product of
///  the pipeline matches with EPSILON too (#561).
constexpr bool matches_epsilon(const Symbol symbol) { return symbol == EPSILON || symbol == DONT_CARE; }

/// The synchronization type of reading @p symbol on a synchronization level: DONT_CARE matches both kinds.
constexpr SynchronizationType sync_type_of(const Symbol symbol) {
	if (symbol == EPSILON) { return SynchronizationType::OnlyOnEpsilon; }
	if (symbol == DONT_CARE) { return SynchronizationType::OnEpsilonAndSymbol; }
	return SynchronizationType::OnlyOnSymbol;
}

/**
 * @brief Pair the groups of reads of two operands on a synchronization level both of them read.
 *
 * The groups of each operand have a @c symbol each, in increasing order. Equal symbols synchronize, and DONT_CARE
 *  synchronizes with every symbol of the other operand, EPSILON included, as in the product of the pipeline (#561);
 *  @p on_pair gets the symbol the pair emits, which is DONT_CARE only for DONT_CARE against DONT_CARE.
 */
template <typename LhsGroups, typename RhsGroups, typename OnPair>
void synchronize(const LhsGroups& lhs_groups, const RhsGroups& rhs_groups, OnPair&& on_pair) {
	const auto symbol_of{[](const auto& group) { return group.symbol; }};
	const auto lhs_dont_care{std::ranges::lower_bound(lhs_groups, DONT_CARE, {}, symbol_of)};
	const bool lhs_has_dont_care{lhs_dont_care != std::ranges::end(lhs_groups) && lhs_dont_care->symbol == DONT_CARE};
	if (lhs_has_dont_care) {
		for (const auto& rhs_group : rhs_groups) { on_pair(rhs_group.symbol, *lhs_dont_care, rhs_group); }
	}
	if (const auto rhs_dont_care{std::ranges::lower_bound(rhs_groups, DONT_CARE, {}, symbol_of)};
		rhs_dont_care != std::ranges::end(rhs_groups) && rhs_dont_care->symbol == DONT_CARE) {
		for (const auto& lhs_group : lhs_groups) {
			if (lhs_group.symbol != DONT_CARE) { on_pair(lhs_group.symbol, lhs_group, *rhs_dont_care); }
		}
	}
	// A two-pointer merge finds the other shared symbols, EPSILON included.
	auto lhs_group{std::ranges::begin(lhs_groups)};
	auto rhs_group{std::ranges::begin(rhs_groups)};
	while (lhs_group != std::ranges::end(lhs_groups) && rhs_group != std::ranges::end(rhs_groups)) {
		if (lhs_group->symbol < rhs_group->symbol) {
			++lhs_group;
		} else if (rhs_group->symbol < lhs_group->symbol) {
			++rhs_group;
		} else {
			if (lhs_group->symbol != DONT_CARE) { on_pair(lhs_group->symbol, *lhs_group, *rhs_group); }
			++lhs_group;
			++rhs_group;
		}
	}
}

/**
 * @brief What each state of an operand can still do in its round, computed backwards over the levels.
 *
 * A round of an operand runs from a zero-level state over every level back to a zero-level state. The direct
 *  composition takes a round of each operand together, or a round of one operand while the other one waits: the
 *  waiting operand reads EPSILON on every synchronization level, so the moving one has to read EPSILON (or DONT_CARE)
 *  there too. A jump transition reads its symbol on each level it covers (@c JumpMode::RepeatSymbol and
 *  @c JumpMode::NoJump, which the pipeline reads alike), or on its first level and DONT_CARE on the others
 *  (@c JumpMode::AppendDontCares).
 *
 * A place of the operand that is not a state, a cursor inside a jump transition (@c DirectOperand), is not looked
 *  ahead: it may still do anything. Neither is a state with a jump transition of an operand that chooses its jumps
 *  again (@c DirectOperand::may_choose_again()), which can combine its jump transitions.
 */
class RoundLookahead {
  public:
	/// Look ahead in the operand @p nft with @p num_of_places places, see @c DirectOperand.
	RoundLookahead(
		const Nft& nft,
		const OrdVector<Level>& sync_levels,
		const size_t num_of_places,
		const JumpMode jump_mode,
		const bool chooses_again
	)
		: facts_(num_of_places, ANYTHING) {
		const size_t num_of_levels{nft.levels.num_of_levels};
		BoolVector is_sync(num_of_levels, false);
		for (const Level level : sync_levels) { is_sync[level] = true; }
		// The first synchronization level from each level on, or num_of_levels if there is none.
		std::vector<size_t> first_sync(num_of_levels + 1, num_of_levels);
		for (size_t level{num_of_levels}; level-- > 0;) {
			first_sync[level] = is_sync[level] ? level : first_sync[level + 1];
		}
		std::vector<std::vector<State>> states_by_level(num_of_levels);
		for (State state{0}; state < nft.num_of_states(); ++state) {
			if (nft.levels[state] < num_of_levels) { states_by_level[nft.levels[state]].push_back(state); }
		}
		// A transition goes to a later level or to a zero-level state, which ends the round, so the facts of its
		//  targets are known before those of its source.
		for (size_t level{num_of_levels}; level-- > 0;) {
			for (const State state : states_by_level[level]) {
				uint8_t facts{0};
				for (const SymbolPost& symbol_post : nft.delta[state]) {
					// The symbol the transitions read on the levels they jump over.
					const Symbol later_symbol{jump_mode == JumpMode::AppendDontCares ? DONT_CARE : symbol_post.symbol};
					for (const State target : symbol_post.targets) {
						const size_t end{nft.levels[target] == 0 ? num_of_levels : nft.levels[target]};
						if (chooses_again && end != level + 1) { facts = ANYTHING; }
						const uint8_t target_facts{nft.levels[target] == 0 ? FINISHED : facts_[target]};
						facts |= target_facts & CAN_FINISH;
						const bool reads_sync_first{is_sync[level] != 0};
						const bool reads_sync_later{first_sync[level + 1] < end};
						if ((!reads_sync_first || matches_epsilon(symbol_post.symbol)) &&
							(!reads_sync_later || matches_epsilon(later_symbol)) && (target_facts & CAN_WAIT) != 0) {
							facts |= CAN_WAIT;
						}
						if (reads_sync_first || reads_sync_later) {
							if ((target_facts & CAN_FINISH) != 0) {
								facts |= static_cast<uint8_t>(
									sync_type_of(reads_sync_first ? symbol_post.symbol : later_symbol)
								);
							}
						} else if (first_sync[end] < num_of_levels) {
							facts |= target_facts & SYNC_TYPE;
						}
					}
				}
				facts_[state] = facts;
			}
		}
	}

	/// What @p place can read on the next synchronization level of its round on a path that finishes the round.
	SynchronizationType sync_type(const State place) const {
		return static_cast<SynchronizationType>(facts_of(place) & SYNC_TYPE);
	}
	/// Whether @p place can finish its round.
	bool can_finish(const State place) const { return (facts_of(place) & CAN_FINISH) != 0; }
	/// Whether @p place can finish its round reading EPSILON on each synchronization level left, as it has to while
	///  the other operand waits.
	bool can_finish_alone(const State place) const { return (facts_of(place) & CAN_WAIT) != 0; }

  private:
	static constexpr uint8_t SYNC_TYPE{static_cast<uint8_t>(SynchronizationType::OnEpsilonAndSymbol)};
	static constexpr uint8_t CAN_FINISH{0b0000'0100};
	static constexpr uint8_t CAN_WAIT{0b0000'1000};
	static constexpr uint8_t FINISHED{CAN_FINISH | CAN_WAIT};
	static constexpr uint8_t ANYTHING{SYNC_TYPE | FINISHED};

	uint8_t facts_of(const State place) const { return facts_[place]; }

	std::vector<uint8_t> facts_;
};

/**
 * @brief An operand of the direct general composition as it reads the aligned levels.
 *
 * A place of the operand is one of its states, or a cursor inside its jump transitions, numbered after the states: a
 *  source state and one of its symbol posts with a jump transition. The symbol was read on the first level of the
 *  jump; the cursor stands
 *  for the targets of the symbol post not reached yet and reads the symbol again (@c JumpMode::RepeatSymbol,
 *  @c JumpMode::NoJump) or DONT_CARE (@c JumpMode::AppendDontCares) on each level up to them, so a jump is read level
 *  by level without inner states.
 *
 * The pipeline keeps the jump transitions of the operand only when it inserts no level into it, that is, when the
 *  operand reads every aligned level; otherwise @c insert_levels() spells each jump out level by level. Inside such a
 *  kept jump, @c product() under @c JumpMode::RepeatSymbol or @c JumpMode::NoJump leaves the operand in the source
 *  state, and on each level where the copy of the other operand is at a state, it reads any transition of the
 *  source state that ends later: the operand chooses its jump again (@c may_choose_again()). The cursor does the
 *  same, so the relation stays the one of the pipeline.
 */
class DirectOperand {
  public:
	DirectOperand(
		const Nft& nft,
		const CompositionSchedule& schedule,
		const bool is_lhs,
		const OrdVector<Level>& sync_levels,
		const JumpMode jump_mode
	)
		: nft_{nft},
		  num_of_states_{nft.num_of_states()},
		  jump_mode_{jump_mode},
		  cursors_{nft.contains_jump_transitions() ? cursors_of(nft) : Cursors{}},
		  is_padded_{nft.levels.num_of_levels < schedule.positions.size()},
		  chooses_again_{!is_padded_ && jump_mode != JumpMode::AppendDontCares && has_jumps()},
		  lookahead_{nft, sync_levels, num_of_places(), jump_mode, chooses_again_} {
		const size_t num_of_positions{schedule.positions.size()};
		next_level_.resize(num_of_positions, 0);
		for (size_t position{0}; position + 1 < num_of_positions; ++position) {
			const CompositionSchedule::Position& next{schedule.positions[position + 1]};
			next_level_[position] = is_lhs ? next.lhs_level : next.rhs_level;
		}
		if (is_padded_) {
			// insert_levels() gives the padded copy a state on each level the operand reads, on the level after it,
			//  and on level 0; it jumps over the other inserted levels.
			const auto reads{[&](const size_t position) {
				const CompositionSchedule::Owner owner{schedule.positions[position].owner};
				return owner == CompositionSchedule::Owner::Sync ||
					   owner == (is_lhs ? CompositionSchedule::Owner::Lhs : CompositionSchedule::Owner::Rhs);
			}};
			has_padded_state_at_.resize(num_of_positions, true);
			for (size_t position{1}; position < num_of_positions; ++position) {
				has_padded_state_at_[position] = reads(position) || reads(position - 1);
			}
		}
	}

	/// An upper bound on the number of places of @p nft.
	static size_t max_num_of_places(const Nft& nft) {
		size_t num_of_places{nft.num_of_states()};
		for (State state{0}; state < nft.num_of_states(); ++state) { num_of_places += nft.delta[state].size(); }
		return num_of_places;
	}

	/// A run of reads on one symbol: the indices of their places in @c Reads::targets.
	struct ReadRun {
		Symbol symbol;
		size_t begin;
		size_t end;
	};
	/// Reads grouped by symbol, in increasing order.
	struct Reads {
		std::vector<State> targets{};
		std::vector<ReadRun> runs{};
	};

	const Nft& nft() const { return nft_; }
	const RoundLookahead& lookahead() const { return lookahead_; }
	/// The number of places: the states, then the cursors.
	size_t num_of_places() const { return num_of_states_ + cursors_.symbol_post.size(); }
	bool has_jumps() const { return !cursors_.symbol_post.empty(); }

	/// Whether the operand at @p place may choose its jump again, which it does on an aligned level where the other
	///  operand is at a state (@c has_state_at()).
	bool may_choose_again(const State place) const { return chooses_again_ && place >= num_of_states_; }
	/**
	 * @brief Whether the copy of the operand the pipeline builds is at a state, not inside a jump, on the aligned
	 *  level @p position when the operand is at @p place.
	 *
	 * A waiting operand, which goes along its self-loop, is at a state on every level.
	 */
	bool has_state_at(const State place, const size_t position, const bool waits) const {
		if (waits) { return true; }
		if (is_padded_) { return has_padded_state_at_[position] != 0; }
		return place < num_of_states_;
	}

	/**
	 * @brief Read the aligned level @p position, one the operand reads, from @p place: call @p on_read with each
	 *  symbol read and the place after it.
	 *
	 * With @p chooses_again, a cursor reads every transition of its source state that ends later, see
	 *  @c may_choose_again().
	 */
	template <typename OnRead>
	void read_level(const State place, const size_t position, const bool chooses_again, OnRead&& on_read) const {
		const Level next_level{next_level_[position]};
		if (place < num_of_states_ || chooses_again) {
			const State source{place < num_of_states_ ? place : cursor_of(place).first};
			const StatePost& state_post{nft_.delta[source]};
			for (auto symbol_post{state_post.begin()}; symbol_post != state_post.end(); ++symbol_post) {
				read_symbol_post(source, symbol_post, symbol_post->symbol, next_level, on_read);
			}
			return;
		}
		const auto [source, symbol_post]{cursor_of(place)};
		read_symbol_post(source, symbol_post, symbol_inside(*symbol_post), next_level, on_read);
	}

	/// Read as the other @c read_level() into @p reads.
	void read_level(const State place, const size_t position, const bool chooses_again, Reads& reads) const {
		reads.targets.clear();
		reads.runs.clear();
		read_level(place, position, chooses_again, [&](const Symbol symbol, const State target) {
			if (reads.runs.empty() || reads.runs.back().symbol != symbol) {
				reads.runs.push_back({symbol, reads.targets.size(), reads.targets.size()});
			}
			reads.targets.push_back(target);
			++reads.runs.back().end;
		});
	}

	/// As the first @c read_level(), restricted to the reads of @p symbol.
	template <typename OnRead>
	void read_level(
		const State place, const size_t position, const bool chooses_again, const Symbol symbol, OnRead&& on_read
	) const {
		const Level next_level{next_level_[position]};
		if (place < num_of_states_ || chooses_again) {
			const State source{place < num_of_states_ ? place : cursor_of(place).first};
			const StatePost& state_post{nft_.delta[source]};
			if (const auto symbol_post{state_post.find(symbol)}; symbol_post != state_post.end()) {
				read_symbol_post(source, symbol_post, symbol, next_level, on_read);
			}
			return;
		}
		const auto [source, symbol_post]{cursor_of(place)};
		if (symbol_inside(*symbol_post) == symbol) {
			read_symbol_post(source, symbol_post, symbol, next_level, on_read);
		}
	}

  private:
	/// The symbol a jump along @p symbol_post reads on the levels after its first one.
	Symbol symbol_inside(const SymbolPost& symbol_post) const {
		return jump_mode_ == JumpMode::AppendDontCares ? DONT_CARE : symbol_post.symbol;
	}

	/// Read @p symbol along @p symbol_post of @p source: reach its targets on @p next_level, and keep a cursor for its
	///  targets further on.
	template <typename OnRead>
	void read_symbol_post(
		const State source,
		const StatePost::const_iterator symbol_post,
		const Symbol symbol,
		const Level next_level,
		OnRead&& on_read
	) const {
		if (!has_jumps()) {
			// Without jumps, every target is on the next level.
			for (const State target : symbol_post->targets) { on_read(symbol, target); }
			return;
		}
		bool goes_on{false};
		for (const State target : symbol_post->targets) {
			const Level target_level{nft_.levels[target]};
			if (target_level == next_level) {
				on_read(symbol, target);
			} else if (next_level != 0 && (target_level == 0 || target_level > next_level)) {
				goes_on = true;
			}
		}
		if (goes_on) { on_read(symbol, cursor(source, symbol_post)); }
	}

	/// The cursors of an operand: one for each symbol post with a jump transition.
	struct Cursors {
		/// Indexed by state: the index of its first symbol post among all symbol posts.
		std::vector<size_t> first_symbol_post{};
		/// Indexed by symbol post: its cursor, or @c Limits::max_state.
		std::vector<State> of_symbol_post{};
		/// Indexed by cursor: its source state and the index of its symbol post in the post of the source.
		std::vector<std::pair<State, size_t>> symbol_post{};
	};
	static Cursors cursors_of(const Nft& nft) {
		Cursors cursors{};
		const size_t num_of_levels{nft.levels.num_of_levels};
		cursors.first_symbol_post.reserve(nft.num_of_states() + 1);
		for (State source{0}; source < nft.num_of_states(); ++source) {
			cursors.first_symbol_post.push_back(cursors.of_symbol_post.size());
			const size_t next_level{nft.levels[source] + size_t{1}};
			size_t index{0};
			for (const SymbolPost& symbol_post : nft.delta[source]) {
				const bool jumps{std::ranges::any_of(symbol_post.targets, [&](const State target) {
					return (nft.levels[target] == 0 ? num_of_levels : nft.levels[target]) != next_level;
				})};
				cursors.of_symbol_post.push_back(Limits::max_state);
				if (jumps) {
					cursors.of_symbol_post.back() = static_cast<State>(cursors.symbol_post.size());
					cursors.symbol_post.emplace_back(source, index);
				}
				++index;
			}
		}
		cursors.first_symbol_post.push_back(cursors.of_symbol_post.size());
		return cursors;
	}

	State cursor(const State source, const StatePost::const_iterator symbol_post) const {
		const State cursor{
			cursors_.of_symbol_post
				[cursors_.first_symbol_post[source] + static_cast<size_t>(symbol_post - nft_.delta[source].begin())]
		};
		MATA_ASSERT(cursor != Limits::max_state);
		return static_cast<State>(num_of_states_ + cursor);
	}
	std::pair<State, StatePost::const_iterator> cursor_of(const State place) const {
		const auto [source, index]{cursors_.symbol_post[place - num_of_states_]};
		return {source, nft_.delta[source].begin() + static_cast<std::ptrdiff_t>(index)};
	}

	const Nft& nft_;
	const size_t num_of_states_;
	const JumpMode jump_mode_;
	/// Empty without jump transitions.
	const Cursors cursors_;
	/// Whether the pipeline inserts levels into the operand.
	const bool is_padded_;
	const bool chooses_again_;
	const RoundLookahead lookahead_;
	/// Indexed by aligned level, for a padded operand: whether its padded copy is at a state on it.
	BoolVector has_padded_state_at_{};
	/// Indexed by aligned level: the level of the operand after reading it, 0 at the end of the round.
	std::vector<Level> next_level_{};
};

/**
 * @brief The general composition of two NFTs by a traversal of the aligned configurations reachable from the
 *  initial pairs.
 *
 * The pipeline of @c compose_general_reference() pads copies of the operands to the aligned levels, adds a whole-round
 *  self-loop of EPSILON and DONT_CARE to each of their zero-level states, and builds their product. A round of that
 *  product is a round of both operands, or a round of one operand against the self-loop of the other one. This class
 *  builds exactly these rounds over the original operands: a configuration is a pair of operand places (a state, or
 *  a cursor inside a jump transition, see @c DirectOperand), the aligned level read next (@c CompositionSchedule), and
 *  which operands move in the current round; a padded level is a cursor move that emits the symbol of the operand
 *  reading it, or EPSILON for an operand waiting in its self-loop.
 *  On a synchronization level, DONT_CARE matches every symbol of the other operand and emits it, EPSILON included, as
 *  the product of the pipeline does (#561); so a moving operand reads DONT_CARE against a waiting one as EPSILON.
 *  The result has the same relation as the pipeline result, EPSILON included, except that a round where both
 *  operands would wait does not exist (the pipeline has one only when all aligned levels are a single
 *  synchronization level, which is left to it).
 *
 * Pruning, none of which changes the relation:
 * - A configuration is created only when both operands can still synchronize in the round (the synchronization
 *   types of the fast composer), and a waiting round only when the moving operand can read EPSILON on every
 *   synchronization level left.
 * - When every level of both operands is a synchronization level, a round of one operand alone emits nothing but
 *   EPSILON. Such rounds of the two operands commute, so the EPSILON-composition filter of the fast composer keeps
 *   only the interleavings in which the LHS moves alone before the RHS does.
 *
 * A projected synchronization level gets no result level: reading it is a step of the move that reads the kept level
 *  before it, which goes on to the configurations after it.
 */
class DirectGeneralComposer {
  public:
	DirectGeneralComposer(
		const Nft& lhs,
		const Nft& rhs,
		const CompositionSchedule& schedule,
		const OrdVector<Level>& lhs_sync_levels,
		const OrdVector<Level>& rhs_sync_levels,
		const bool project_out_sync_levels,
		const JumpMode jump_mode
	)
		: lhs_{lhs, schedule, true, lhs_sync_levels, jump_mode},
		  rhs_{rhs, schedule, false, rhs_sync_levels, jump_mode},
		  schedule_{schedule},
		  num_of_positions_{schedule.positions.size()},
		  lhs_alone_action_{
			  lhs.levels.num_of_levels == lhs_sync_levels.size() ? EpsilonFilterAction::SilentLhsOnlyMove
																 : EpsilonFilterAction::NonSilentLhsOnlyMove
		  },
		  rhs_alone_action_{
			  rhs.levels.num_of_levels == rhs_sync_levels.size() ? EpsilonFilterAction::SilentRhsOnlyMove
																 : EpsilonFilterAction::NonSilentRhsOnlyMove
		  },
		  num_of_filter_modes_{
			  lhs_alone_action_ == EpsilonFilterAction::SilentLhsOnlyMove &&
					  rhs_alone_action_ == EpsilonFilterAction::SilentRhsOnlyMove
				  ? NUMBER_OF_EPSILON_FILTER_MODES
				  : 1
		  },
		  num_of_layers_{num_of_filter_modes_ + NUMBER_OF_MODES * (num_of_positions_ - 1)},
		  configurations_{lhs_.num_of_places() * num_of_layers_, rhs_.num_of_places()},
		  lhs_reads_(num_of_positions_),
		  rhs_reads_(num_of_positions_) {
		MATA_ASSERT(num_of_positions_ > 1);
		is_sync_ahead_.resize(num_of_positions_ + 1, false);
		is_lhs_done_.resize(num_of_positions_ + 1, true);
		is_rhs_done_.resize(num_of_positions_ + 1, true);
		for (size_t position{num_of_positions_}; position-- > 0;) {
			const CompositionSchedule::Owner owner{schedule.positions[position].owner};
			is_sync_ahead_[position] = owner == CompositionSchedule::Owner::Sync || is_sync_ahead_[position + 1];
			is_lhs_done_[position] = owner == CompositionSchedule::Owner::Rhs && is_lhs_done_[position + 1];
			is_rhs_done_[position] = owner == CompositionSchedule::Owner::Lhs && is_rhs_done_[position + 1];
		}
		is_projected_.resize(num_of_positions_, false);
		result_level_.resize(num_of_positions_, 0);
		for (size_t position{0}; position < num_of_positions_; ++position) {
			is_projected_[position] =
				project_out_sync_levels && schedule.positions[position].owner == CompositionSchedule::Owner::Sync;
			result_level_[position] = num_of_result_levels_;
			if (!is_projected_[position]) { ++num_of_result_levels_; }
		}
		MATA_ASSERT(num_of_result_levels_ > 0);
	}

	/// Whether the configurations of @p lhs and @p rhs on @p num_of_positions positions can be numbered.
	static bool fits(const Nft& lhs, const Nft& rhs, const size_t num_of_positions) {
		const size_t num_of_layers{NUMBER_OF_EPSILON_FILTER_MODES + NUMBER_OF_MODES * (num_of_positions - 1)};
		return DirectOperand::max_num_of_places(lhs) < (Limits::max_state - 1) / num_of_layers &&
			   DirectOperand::max_num_of_places(rhs) < Limits::max_state;
	}

	/// Build the composition.
	Nft compose() {
		SparseSet<State> initial{};
		for (const State lhs_initial : lhs_.nft().initial) {
			for (const State rhs_initial : rhs_.nft().initial) {
				initial.insert(intern(lhs_initial, rhs_initial, start_layer(EpsilonFilterMode::LhsMayMoveAlone), 0));
			}
		}
		// The states are numbered in the order they are found, so expanding them in that order is a breadth-first
		//  search that appends the post of each state to the delta in order.
		for (State source{0}; source < configuration_of_.size(); ++source) { expand(source); }
		return Nft::with_levels(
			Levels{num_of_result_levels_, std::move(levels_)}, delta_builder_.finish(), std::move(initial),
			std::move(final_)
		);
	}

  private:
	/// Which operands move in the current round.
	enum class Mode : uint8_t {
		Both = 0, ///< Both operands read a round.
		LhsAlone = 1, ///< The LHS reads a round, the RHS waits in its zero-level state.
		RhsAlone = 2, ///< The RHS reads a round, the LHS waits in its zero-level state.
	};
	static constexpr size_t NUMBER_OF_MODES{3};

	/// A configuration: the operand places and the layer, which encodes either the filter mode of a configuration at
	///  the start of a round, where the places are zero-level states, or the aligned level read next and the mode of
	///  the round.
	struct Configuration {
		State lhs;
		State rhs;
		size_t layer;
	};

	size_t start_layer(const EpsilonFilterMode filter_mode) const {
		MATA_ASSERT(static_cast<size_t>(filter_mode) < num_of_filter_modes_);
		return static_cast<size_t>(filter_mode);
	}
	size_t inner_layer(const size_t position, const Mode mode) const {
		MATA_ASSERT(0 < position && position < num_of_positions_);
		return num_of_filter_modes_ + NUMBER_OF_MODES * (position - 1) + static_cast<size_t>(mode);
	}

	/// The state of the configuration, created on the first visit.
	State intern(const State lhs_state, const State rhs_state, const size_t layer, const Level level) {
		const State row{static_cast<State>(lhs_state * num_of_layers_ + layer)};
		// The number the configuration would get: the states are numbered by their creation.
		const auto [state, inserted]{
			configurations_.insert_if_absent(row, rhs_state, static_cast<State>(configuration_of_.size()))
		};
		if (!inserted) { return state; }
		configuration_of_.push_back({lhs_state, rhs_state, layer});
		levels_.push_back(level);
		if (layer < num_of_filter_modes_ && lhs_.nft().final.contains(lhs_state) &&
			rhs_.nft().final.contains(rhs_state)) {
			final_.insert(state);
		}
		return state;
	}

	/**
	 * @brief Read the aligned level @p position from the operand places in @p mode: call @p on_move with the emitted
	 *  symbol and the operand places after each move.
	 */
	template <typename OnMove>
	void read_level(
		const State lhs_place, const State rhs_place, const size_t position, const Mode mode, OnMove&& on_move
	) {
		const bool lhs_chooses_again{
			lhs_.may_choose_again(lhs_place) && rhs_.has_state_at(rhs_place, position, mode == Mode::LhsAlone)
		};
		const bool rhs_chooses_again{
			rhs_.may_choose_again(rhs_place) && lhs_.has_state_at(lhs_place, position, mode == Mode::RhsAlone)
		};
		switch (schedule_.positions[position].owner) {
			case CompositionSchedule::Owner::Lhs:
				if (mode == Mode::RhsAlone) {
					on_move(EPSILON, lhs_place, rhs_place);
					return;
				}
				lhs_.read_level(
					lhs_place, position, lhs_chooses_again,
					[&](const Symbol symbol, const State lhs_target) { on_move(symbol, lhs_target, rhs_place); }
				);
				return;
			case CompositionSchedule::Owner::Rhs:
				if (mode == Mode::LhsAlone) {
					on_move(EPSILON, lhs_place, rhs_place);
					return;
				}
				rhs_.read_level(
					rhs_place, position, rhs_chooses_again,
					[&](const Symbol symbol, const State rhs_target) { on_move(symbol, lhs_place, rhs_target); }
				);
				return;
			case CompositionSchedule::Owner::Sync:
				break;
		}
		if (mode == Mode::LhsAlone) {
			// The waiting RHS reads EPSILON on its own levels, which EPSILON and DONT_CARE match.
			for (const Symbol symbol : {DONT_CARE, EPSILON}) {
				lhs_.read_level(lhs_place, position, lhs_chooses_again, symbol, [&](Symbol, const State lhs_target) {
					on_move(EPSILON, lhs_target, rhs_place);
				});
			}
			return;
		}
		if (mode == Mode::RhsAlone) {
			for (const Symbol symbol : {DONT_CARE, EPSILON}) {
				rhs_.read_level(rhs_place, position, rhs_chooses_again, symbol, [&](Symbol, const State rhs_target) {
					on_move(EPSILON, lhs_place, rhs_target);
				});
			}
			return;
		}
		if (!lhs_.has_jumps() && !rhs_.has_jumps()) {
			// Every target of a state of an operand without jumps is on its next level.
			synchronize(
				lhs_.nft().delta[lhs_place], rhs_.nft().delta[rhs_place],
				[&](const Symbol symbol, const SymbolPost& lhs_symbol_post, const SymbolPost& rhs_symbol_post) {
					for (const State lhs_target : lhs_symbol_post.targets) {
						for (const State rhs_target : rhs_symbol_post.targets) {
							on_move(symbol, lhs_target, rhs_target);
						}
					}
				}
			);
			return;
		}
		// A move on a later level reads into the buffers of that level.
		DirectOperand::Reads& lhs_reads{lhs_reads_[position]};
		lhs_.read_level(lhs_place, position, lhs_chooses_again, lhs_reads);
		if (lhs_reads.runs.empty()) { return; }
		DirectOperand::Reads& rhs_reads{rhs_reads_[position]};
		rhs_.read_level(rhs_place, position, rhs_chooses_again, rhs_reads);
		synchronize(
			lhs_reads.runs, rhs_reads.runs,
			[&](const Symbol symbol, const DirectOperand::ReadRun& lhs_run, const DirectOperand::ReadRun& rhs_run) {
				for (size_t lhs_index{lhs_run.begin}; lhs_index < lhs_run.end; ++lhs_index) {
					for (size_t rhs_index{rhs_run.begin}; rhs_index < rhs_run.end; ++rhs_index) {
						on_move(symbol, lhs_reads.targets[lhs_index], rhs_reads.targets[rhs_index]);
					}
				}
			}
		);
	}

	/// Whether a round in @p mode can still be finished from the operand states before the aligned level @p position.
	bool can_finish(const State lhs_state, const State rhs_state, const size_t position, const Mode mode) const {
		switch (mode) {
			case Mode::Both:
				if (is_sync_ahead_[position]) {
					return exist_intersection(
						lhs_.lookahead().sync_type(lhs_state), rhs_.lookahead().sync_type(rhs_state)
					);
				}
				return (is_lhs_done_[position] || lhs_.lookahead().can_finish(lhs_state)) &&
					   (is_rhs_done_[position] || rhs_.lookahead().can_finish(rhs_state));
			case Mode::LhsAlone:
				return is_lhs_done_[position] || lhs_.lookahead().can_finish_alone(lhs_state);
			case Mode::RhsAlone:
				return is_rhs_done_[position] || rhs_.lookahead().can_finish_alone(rhs_state);
		}
		return false;
	}

	/// The filter mode after a round in @p mode. A round of the RHS alone leaves the LHS state as it was, and the
	///  filter refuses no round of the RHS alone, so the result does not depend on the mode the round started in.
	EpsilonFilterMode filter_mode_after(const Mode mode, const State lhs_state) const {
		if (mode != Mode::RhsAlone) { return EpsilonFilterMode::LhsMayMoveAlone; }
		return epsilon_filter_step(
				   EpsilonFilterMode::LhsMayMoveAlone, rhs_alone_action_,
				   lhs_alone_action_ == EpsilonFilterAction::SilentLhsOnlyMove &&
					   lhs_.lookahead().can_finish_alone(lhs_state),
				   false
		)
			.value();
	}

	/// A move inside a round on its way through the projected levels: the symbol it emits on its kept level and the
	///  operand places it has reached.
	struct Step {
		Symbol symbol;
		State lhs;
		State rhs;
		auto operator<=>(const Step&) const = default;
	};

	/**
	 * @brief Record the moves in @p mode that read the first kept aligned level from @p position on, from the operand
	 *  places @p lhs_place and @p rhs_place.
	 *
	 * A projected synchronization level has no transition of its own. Projected levels before the kept one, which only
	 *  the start of a round has, are read on the way to it; those after it are steps of the moves, which reach the
	 *  configurations before the next kept level or at the end of the round. The steps reached on each level are
	 *  deduplicated before the next level is read, so the work is bounded by the configurations of each level, not by
	 *  the paths through the projected levels, which can be exponentially many.
	 */
	void read_kept_level(const State lhs_place, const State rhs_place, size_t position, const Mode mode) {
		std::vector<Step>& steps{steps_};
		std::vector<Step>& next_steps{next_steps_};
		steps.assign(1, Step{EPSILON, lhs_place, rhs_place});
		bool has_read_kept_level{false};
		for (; position < num_of_positions_ && !steps.empty(); ++position) {
			const bool is_kept{!is_projected_[position]};
			if (is_kept && has_read_kept_level) { break; }
			next_steps.clear();
			for (const Step& step : steps) {
				read_level(
					step.lhs, step.rhs, position, mode,
					[&](const Symbol symbol, const State lhs_target, const State rhs_target) {
						if (position + 1 == num_of_positions_ ||
							can_finish(lhs_target, rhs_target, position + 1, mode)) {
							next_steps.push_back({is_kept ? symbol : step.symbol, lhs_target, rhs_target});
						}
					}
				);
			}
			has_read_kept_level = has_read_kept_level || is_kept;
			std::ranges::sort(next_steps);
			const auto [duplicates_begin, duplicates_end]{std::ranges::unique(next_steps)};
			next_steps.erase(duplicates_begin, duplicates_end);
			std::swap(steps, next_steps);
		}
		for (const Step& step : steps) {
			moves_.emplace_back(
				step.symbol, position == num_of_positions_
								 ? intern(step.lhs, step.rhs, start_layer(filter_mode_after(mode, step.lhs)), 0)
								 : intern(step.lhs, step.rhs, inner_layer(position, mode), result_level_[position])
			);
		}
	}

	void expand(const State source) {
		const auto [lhs_state, rhs_state, layer] = configuration_of_[source];
		moves_.clear();
		if (layer < num_of_filter_modes_) {
			const auto filter_mode{static_cast<EpsilonFilterMode>(layer)};
			if (can_finish(lhs_state, rhs_state, 0, Mode::Both)) {
				read_kept_level(lhs_state, rhs_state, 0, Mode::Both);
			}
			if (lhs_.lookahead().can_finish_alone(lhs_state) &&
				epsilon_filter_step(filter_mode, lhs_alone_action_, false, false).has_value()) {
				read_kept_level(lhs_state, rhs_state, 0, Mode::LhsAlone);
			}
			if (rhs_.lookahead().can_finish_alone(rhs_state)) {
				read_kept_level(lhs_state, rhs_state, 0, Mode::RhsAlone);
			}
		} else {
			const size_t inner{layer - num_of_filter_modes_};
			read_kept_level(
				lhs_state, rhs_state, inner / NUMBER_OF_MODES + 1, static_cast<Mode>(inner % NUMBER_OF_MODES)
			);
		}
		if (moves_.empty()) { return; }
		std::ranges::sort(moves_);
		const auto [duplicates_begin, duplicates_end]{std::ranges::unique(moves_)};
		moves_.erase(duplicates_begin, duplicates_end);
		delta_builder_.begin_state(source);
		for (size_t index{0}; index < moves_.size(); ++index) {
			if (index == 0 || moves_[index - 1].first != moves_[index].first) {
				if (index != 0) { delta_builder_.finish_symbol(); }
				delta_builder_.begin_symbol(moves_[index].first);
			}
			delta_builder_.push_sorted_target(moves_[index].second);
		}
		delta_builder_.finish_symbol();
		delta_builder_.finish_state();
	}

	const DirectOperand lhs_;
	const DirectOperand rhs_;
	const CompositionSchedule& schedule_;
	const size_t num_of_positions_;
	const EpsilonFilterAction lhs_alone_action_;
	const EpsilonFilterAction rhs_alone_action_;
	const size_t num_of_filter_modes_;
	const size_t num_of_layers_;
	/// Indexed by aligned level: whether a synchronization level is read at it or later in the round, and whether
	///  the LHS or the RHS has read all its levels of the round before it.
	BoolVector is_sync_ahead_{};
	BoolVector is_lhs_done_{};
	BoolVector is_rhs_done_{};
	/// Indexed by aligned level: whether it is projected out, and the result level of a configuration before it.
	BoolVector is_projected_{};
	std::vector<Level> result_level_{};
	Level num_of_result_levels_{0};
	/// The result state of each configuration, keyed by the pair of the LHS state and the layer, and the RHS state.
	TwoDimensionalMap<State, false> configurations_;
	std::vector<Configuration> configuration_of_{};
	std::vector<Level> levels_{};
	SparseSet<State> final_{};
	nfa::DeltaBuilder delta_builder_{};
	std::vector<std::pair<Symbol, State>> moves_{};
	/// The steps of read_kept_level() on the current level and on the next one.
	std::vector<Step> steps_{};
	std::vector<Step> next_steps_{};
	/// Indexed by aligned level: the reads of each operand on a synchronization level both operands read.
	std::vector<DirectOperand::Reads> lhs_reads_;
	std::vector<DirectOperand::Reads> rhs_reads_;
};

/// The body of @c compose_general_direct() for a validated request.
std::optional<Nft> compose_directly(
	const Nft& lhs,
	const Nft& rhs,
	const OrdVector<Level>& lhs_sync_levels,
	const OrdVector<Level>& rhs_sync_levels,
	const bool project_out_sync_levels,
	const JumpMode jump_mode,
	const CompositionNormalization normalization
) {
	const CompositionSchedule schedule{
		lhs.levels.num_of_levels, rhs.levels.num_of_levels, lhs_sync_levels, rhs_sync_levels
	};
	// A single aligned level is a single synchronization level, where the pipeline also synchronizes the
	//  self-loops of the two operands into an EPSILON self-loop of the result.
	if (schedule.positions.size() == 1 || !is_read_directly(lhs) || !is_read_directly(rhs) ||
		!DirectGeneralComposer::fits(lhs, rhs, schedule.positions.size())) {
		return std::nullopt;
	}
	Nft result{
		DirectGeneralComposer{lhs, rhs, schedule, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels, jump_mode}
			.compose()
	};
	// A projected result is trimmed, as the pipeline one is.
	if (project_out_sync_levels || normalization == CompositionNormalization::Trim) { result.trim(); }
	result.alphabets = compose_alphabets(lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels);
	if (normalization == CompositionNormalization::Full) { normalize_fully(result, jump_mode); }
	return result;
}
} // namespace

std::optional<Nft> algorithms::compose_general_direct(
	const Nft& lhs,
	const Nft& rhs,
	const OrdVector<Level>& lhs_sync_levels,
	const OrdVector<Level>& rhs_sync_levels,
	const bool project_out_sync_levels,
	const JumpMode jump_mode,
	const CompositionNormalization normalization
) {
	validate_composition_request(
		"compose_general_direct", lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels
	);
	return compose_directly(
		lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels, jump_mode, normalization
	);
}

Nft algorithms::compose_general(
	const Nft& lhs,
	const Nft& rhs,
	const OrdVector<Level>& lhs_sync_levels,
	const OrdVector<Level>& rhs_sync_levels,
	const bool project_out_sync_levels,
	const JumpMode jump_mode,
	const CompositionNormalization normalization
) {
	validate_composition_request(
		"compose_general", lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels
	);
	if (std::optional<Nft> direct{compose_directly(
			lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels, jump_mode, normalization
		)}) {
		return std::move(*direct);
	}
	return compose_general_reference(
		lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels, jump_mode, normalization
	);
}

Nft algorithms::compose_general_reference(
	const Nft& lhs,
	const Nft& rhs,
	const OrdVector<Level>& lhs_sync_levels,
	const OrdVector<Level>& rhs_sync_levels,
	bool project_out_sync_levels,
	const JumpMode jump_mode,
	const CompositionNormalization normalization
) {
	validate_composition_request(
		"compose_general_reference", lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels
	);

	// Inserts loop into the given Nft for each state with level 0.
	// The loop word is constructed using the EPSILON symbol for all levels, except for the levels
	// where is_dont_care_on_transition is true, in which case the DONT_CARE symbol is used.
	auto insert_self_loops = [&](Nft& nft, const BoolVector& is_dont_care_on_transition) {
		Word loop_word(nft.levels.num_of_levels, EPSILON);
		for (size_t i{0}; i < nft.levels.num_of_levels; i++) {
			if (is_dont_care_on_transition[i]) { loop_word[i] = DONT_CARE; }
		}

		const size_t original_num_of_states = nft.num_of_states();
		for (State s{0}; s < original_num_of_states; s++) {
			if (nft.levels[s] == 0) { nft.insert_word(s, loop_word, s); }
		}
	};

	const CompositionSchedule schedule{
		lhs.levels.num_of_levels, rhs.levels.num_of_levels, lhs_sync_levels, rhs_sync_levels
	};
	const BoolVector lhs_new_levels_mask{schedule.lhs_new_levels_mask()};
	const BoolVector rhs_new_levels_mask{schedule.rhs_new_levels_mask()};
	const OrdVector<Level> sync_levels_to_project_out{schedule.sync_positions()};

	// Insert new levels into lhs and rhs.
	Nft lhs_synced = insert_levels(lhs, lhs_new_levels_mask, {}, jump_mode);
	Nft rhs_synced = insert_levels(rhs, rhs_new_levels_mask, {}, jump_mode);

	// Two auxiliary states (states from inserted loops) can not create a product state.
	const auto lhs_first_aux_state{static_cast<State>(lhs_synced.num_of_states())};
	const auto rhs_first_aux_state{static_cast<State>(rhs_synced.num_of_states())};

	// Insert self-loops into lhs and rhs to ensure synchronization on epsilon transitions.
	insert_self_loops(lhs_synced, lhs_new_levels_mask);
	insert_self_loops(rhs_synced, rhs_new_levels_mask);

	Nft result{intersection(lhs_synced, rhs_synced, nullptr, jump_mode, lhs_first_aux_state, rhs_first_aux_state)};
	if (project_out_sync_levels) {
		// `project_out()` allocates per state and runs its closure over all of them, and only trims at the end.
		result.trim();
		result = project_out(result, sync_levels_to_project_out, jump_mode);
	}

	result.alphabets = compose_alphabets(lhs, rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels);
	// The general composer generates no states of its own, so only `Full` and `Trim` have anything to do.
	if (normalization == CompositionNormalization::Full) {
		normalize_fully(result, jump_mode);
	} else if (normalization == CompositionNormalization::Trim && !project_out_sync_levels) {
		result.trim();
	}

	return result;
}

} // namespace mata::nft
