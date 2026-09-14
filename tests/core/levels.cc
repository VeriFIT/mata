/** @file
 * @brief Naming a level of a relation, building a chain, and writing to one at any arity.
 *
 * Three things that were all the same defect wearing different clothes: a relation spans every key
 *  level, but its vocabulary was singular. `Delta::Key` meant *the outermost* key, `Delta::Reserved`
 *  likewise, and `add(source, key, target)` named exactly one — which is right at @c key_arity 1 and
 *  quietly wrong above it. "Quietly" is the operative word for the last of the three: @c mata::DeltaLike
 *  *required* @c add, so an arity-2 relation satisfied the contract, matched the call in its
 *  declaration, and then failed several template instantiations deep inside @c SymbolPost::insert.
 *  That is the diagnostic the Plan's §3.10 exists to avoid, promised by the very concept meant to
 *  prevent it.
 *
 * So the checks below come in two kinds, and the second kind matters as much as the first: that the
 *  indexed names give the right types, *and* that the members which genuinely cannot be generalised
 *  are now **absent** above arity 1 rather than present-and-broken. A `requires`-probe returning
 *  false is the whole point; there is no runtime behaviour to observe.
 *
 * See the Plan, Phase 5, and @ref arity in @c mata/core/concepts.hh.
 */

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <cstddef>
#include <tuple>
#include <vector>

#include "mata/core/automaton.hh"
#include "mata/core/concepts.hh"
#include "mata/core/delta.hh"

using namespace mata;

namespace {

/// Built with @c RelationOf rather than by hand, which is the ergonomic half of this file: the
///  by-hand spelling is inside-out, so every level has to be named before it can be referred to and
///  the arity is only countable by matching brackets. Compare @c tests/core/cursor.cc, written
///  before this existed.
using D1 = posts::RelationOf<Symbol, StateSet>;
using D2 = posts::RelationOf<Symbol, Symbol, StateSet>;
using D3 = posts::RelationOf<Symbol, Symbol, Symbol, StateSet>;

/// A level with its own reserved tail, in the middle of a chain: a descriptor stands in for the key
///  at its own position, so no level has to be spelled out to give one level a different epsilon.
using Mixed2 = posts::RelationOf<Symbol, ReservedKeys<Symbol, 100>, StateSet>;

/// @name Probes
/// Each asks only "does this relation offer the member at all". @see the file comment.
///@{
template <typename D> concept HasAdd = requires(D d) { d.add(0, 1, 2); };
template <typename D> concept HasAddTargets = requires(D d, const typename D::Nested& n) { d.add(0, 1, n); };
template <typename D> concept HasRemove = requires(D d) { d.remove(0, 1, 2); };
template <typename D> concept HasContains = requires(const D d) { d.contains(0, 1, 2); };
template <typename D> concept HasTransitions = requires(const D d) { d.transitions(); };
template <typename D> concept HasTransitionsTo = requires(const D d) { d.get_transitions_to(0); };
template <typename D> concept HasTransitionsBetween = requires(const D d) { d.get_transitions_between(0, 1); };
template <typename D> concept HasKeyedSuccessors = requires(const D d) { d.get_successors(0, 1); };
template <typename D> concept HasAddTarget = requires(D d, const typename D::KeyPath& p) { d.add_target(0, 1, p); };
///@}

/// Every target under @p delta, gathered the independent way — through the generic walk rather than
///  through anything under test here.
template <typename D> std::vector<State> all_targets(const D& delta) {
	std::vector<State> targets{};
	const size_t states{delta.num_of_states()};
	for (State source{0}; source < states; ++source) {
		posts::walk_targets(delta.state_post(source), [&targets](const State target) {
			targets.push_back(target);
		});
	}
	return targets;
}

} // namespace.

/// @name A chain built from its keys is the chain spelled by hand
///
/// The strongest statement available about @c RelationOf: not that it produces *a* working relation,
///  but that it produces the **same type** the library already ships. If it did not, every other
///  assertion in this file would be about something other than @c mata::Delta.
///@{
static_assert(std::same_as<posts::PostChain<Symbol, StateSet>, StatePost>);
static_assert(std::same_as<D1, Delta>);
static_assert(std::same_as<posts::PostChain<Symbol, StateSet>::Entry, SymbolPost>);

/// A descriptor position contributes its key and keeps itself as that level's convention.
static_assert(std::same_as<D2::Key<1>, Symbol> && std::same_as<Mixed2::Key<1>, Symbol>);
static_assert(std::same_as<Mixed2::Reserved<1>, ReservedKeys<Symbol, 100>>);
static_assert(std::same_as<Mixed2::Reserved<0>, ReservedKeys<Symbol>>);
/// …and only at its own position: level 0 keeps the default.
static_assert(Mixed2::Reserved<0>::epsilon == EPSILON && Mixed2::Reserved<1>::epsilon == 100);
///@}

/// @name Naming one level
///@{
static_assert(D1::key_arity == 1 && D2::key_arity == 2 && D3::key_arity == 3);

/// @c PostAt<0> is the whole chain and @c PostAt<key_arity> is the innermost post. Walking down from
///  a relation has to agree with walking down from its post, or the two spellings mean different
///  things.
static_assert(std::same_as<D3::PostAt<0>, D3::PostType>);
static_assert(std::same_as<D3::PostAt<1>, typename D3::PostType::Nested>);
static_assert(std::same_as<D3::PostAt<3>, StateSet>);
static_assert(std::same_as<D3::TargetSet, StateSet> && std::same_as<D1::TargetSet, StateSet>);
static_assert(std::same_as<D2::PostAt<1>, D1::PostType>); ///< An arity-2 chain nests an arity-1 one.

static_assert(std::same_as<D3::Key<0>, Symbol> && std::same_as<D3::Key<2>, Symbol>);
static_assert(std::same_as<D1::KeyPath, std::tuple<Symbol>>);
static_assert(std::same_as<D3::KeyPath, std::tuple<Symbol, Symbol, Symbol>>);

/// A *post* keeps the singular @c Key: it has exactly one, so there is nothing to disambiguate.
///  That asymmetry with @c Delta is deliberate, not an oversight. @see @ref arity.
static_assert(std::same_as<StatePost::Key, Symbol> && std::same_as<SymbolPost::Key, Symbol>);
///@}

/// @name The contract asks for no key type
///
/// @c mata::AutomatonBase transports keys without inspecting one, so @c DeltaLike requires neither a
///  key nor @c add. Both used to be required, and requiring @c add is what let an arity-2 relation
///  pass the contract and then fail inside the member. See the Plan, §3.7 and Phase 5.
///@{
static_assert(DeltaLike<D1> && DeltaLike<D2> && DeltaLike<D3> && DeltaLike<Mixed2>);

/// @note That the *singular* @c Delta::Key and @c Delta::Reserved are gone is not asserted here:
///  both are alias templates now, so `typename Delta::Key` is a hard error rather than an
///  unsatisfied requirement, and `!requires { ... }` cannot express it (the same GCC-independent
///  limitation @c tests/core/keys.cc hits from the other direction). The indexed spellings asserted
///  above are the positive form of the same statement.
///@}

/// @name Arity-1-shaped members are absent above arity 1, not broken
///
/// Each of these names exactly one key, or hands back a `(source, key, target)` triple with room for
///  exactly one. None can be generalised without changing what it means, so each is constrained
///  instead — and the constraint is what turns a deep instantiation failure into "no such member".
///@{
static_assert(HasAdd<D1> && !HasAdd<D2> && !HasAdd<D3>);
static_assert(HasAddTargets<D1> && !HasAddTargets<D2>);
static_assert(HasRemove<D1> && !HasRemove<D2>);
static_assert(HasContains<D1> && !HasContains<D2>);
static_assert(HasTransitions<D1> && !HasTransitions<D2>);
static_assert(HasTransitionsTo<D1> && !HasTransitionsTo<D2>);
static_assert(HasTransitionsBetween<D1> && !HasTransitionsBetween<D2>);

/// Not in that list: @c get_successors(). Both overloads answer "which states can I reach", which
///  means the same thing at every arity, so both are generic. The return type of the keyed one
///  follows the arity — a reference into the relation where the targets are already contiguous, a
///  fresh set where they have to be gathered — because @c mata::nfa::Nfa::post() passes it straight
///  out as `const StateSet&` and a by-value result there would dangle.
static_assert(HasKeyedSuccessors<D1> && HasKeyedSuccessors<D2> && HasKeyedSuccessors<D3>);
static_assert(std::same_as<D1::Successors, const StateSet&>);
static_assert(std::same_as<D2::Successors, StateSet> && std::same_as<D3::Successors, StateSet>);

/// The generic writer is the one that is there at every arity — and only with a full key path, since
///  @c KeyPath has exactly @c key_arity entries.
static_assert(HasAddTarget<D1> && HasAddTarget<D2> && HasAddTarget<D3>);
/// A partial key path cannot be passed at all: @c KeyPath has exactly @c key_arity entries, which is
///  what the @c same_as assertions on it above pin down. There is no pack to get the length wrong.
///@}

/// @c AutomatonBase still instantiates over both deeper relations after the contract lost @c add:
///  the structural operations reach a successor through the walks and the cursor, never through it.
template class mata::AutomatonBase<D2>;
template class mata::AutomatonBase<D3>;

TEST_CASE("mata::posts::Delta::add_target — writing a key path at any arity") {
	SECTION("at arity 1 it agrees with add(), transition for transition") {
		D1 by_add{};
		D1 by_path{};
		for (const auto& [source, symbol, target] :
		     std::vector<std::tuple<State, Symbol, State>>{{0, 1, 1}, {0, 1, 2}, {0, 4, 3}, {2, 0, 0}, {5, 9, 5}}) {
			by_add.add(source, symbol, target);
			by_path.add_target(source, target, {symbol});
		}
		// Not `==`: an entry's operator== compares only its key, so a post-wise comparison would
		//  pass even with completely different targets. @see mata::posts::posts_equal.
		CHECK(by_add.num_of_states() == by_path.num_of_states());
		CHECK(by_add.num_of_transitions() == by_path.num_of_transitions());
		CHECK(all_targets(by_add) == all_targets(by_path));
		for (State q{0}; q < by_add.num_of_states(); ++q) {
			CHECK(posts::posts_equal(by_add.state_post(q), by_path.state_post(q)));
		}
	}

	SECTION("it presizes, exactly as add() does") {
		D2 delta{};
		REQUIRE(delta.num_of_states() == 0);
		delta.add_target(3, 7, {1, 1}); // Neither state exists yet.
		CHECK(delta.num_of_states() == 8);
		CHECK(delta.uses_state(7));
	}

	SECTION("at arity 2 it creates the levels the path passes through") {
		D2 delta{};
		delta.add_target(0, 10, {1, 1});
		delta.add_target(0, 11, {1, 1}); // Same path, second target.
		delta.add_target(0, 12, {1, 2}); // Same level-0 key, new level-1 key.
		delta.add_target(0, 13, {2, 1}); // New level-0 key.

		const auto& post{delta.state_post(0)};
		REQUIRE(post.size() == 2); // Two level-0 keys.
		CHECK(post.is_sorted());
		REQUIRE(post.front().nested().size() == 2); // Two level-1 keys under key 1.
		CHECK(post.front().nested().front().nested() == StateSet{10, 11});
		CHECK(post.front().nested().back().nested() == StateSet{12});
		CHECK(post.back().nested().front().nested() == StateSet{13});
		CHECK(delta.num_of_transitions() == 4);
	}

	SECTION("at arity 3 as well, and the walk agrees") {
		D3 delta{};
		delta.add_target(0, 5, {1, 2, 3});
		delta.add_target(0, 6, {1, 2, 4});
		delta.add_target(1, 7, {1, 2, 3});
		CHECK(delta.num_of_transitions() == 3);
		CHECK(all_targets(delta) == std::vector<State>{5, 6, 7});

		// The keys come back out in the order they went in, one per level.
		std::vector<std::tuple<Symbol, Symbol, Symbol, State>> moves{};
		delta.for_each_move(0, [&moves](const Symbol a, const Symbol b, const Symbol c, const State t) {
			moves.emplace_back(a, b, c, t);
		});
		CHECK(moves == std::vector<std::tuple<Symbol, Symbol, Symbol, State>>{{1, 2, 3, 5}, {1, 2, 4, 6}});
	}

	SECTION("a deeper relation still reverts, which is what needs a generic write") {
		mata::AutomatonBase<D2> aut{};
		aut.delta.add_target(0, 1, {1, 2});
		aut.delta.add_target(1, 2, {3, 4});
		aut.delta.add_target(3, 3, {5, 6}); // A self-loop that never reaches a final state.
		aut.initial.insert(0);
		aut.final.insert(2);

		CHECK(aut.get_reachable_states() == mata::StateSet{0, 1, 2});
		// @c get_terminating_states() is answered by *reverting* and walking forward from the final
		//  states, so it exercises the whole generic write path: a key path put back in the wrong
		//  order, or a target written under the wrong level, shows up as the wrong answer here.
		//  State 3 is the control — it must not appear.
		CHECK(aut.get_terminating_states() == mata::StateSet{0, 1, 2});

		// Indexed rather than compared whole: @c distances_to_final() returns `num_of_states() + 1`
		//  entries, which is pre-existing (the arity-1 relation does the same) and nothing to do
		//  with the key arity, so pinning the length here would be asserting an unrelated quirk.
		const std::vector<State> to_final{aut.distances_to_final()};
		REQUIRE(to_final.size() > 3);
		CHECK(to_final[0] == 2);
		CHECK(to_final[1] == 1);
		CHECK(to_final[2] == 0);
		CHECK(to_final[3] == mata::Limits::max_state); // The control: 3 reaches no final state.
	}
}

TEST_CASE("mata::posts::Delta::get_successors — the target set, at any arity") {
	SECTION("arity 1 is unchanged") {
		Delta delta{};
		delta.add(0, 1, 1);
		delta.add(0, 2, 3);
		delta.add(0, 2, 1);
		CHECK(delta.get_successors(0) == StateSet{1, 3});
		CHECK(delta.get_successors(1).empty());
	}

	SECTION("arity 2 collects from under every remaining key") {
		D2 delta{};
		delta.add_target(0, 4, {1, 1});
		delta.add_target(0, 2, {1, 5});
		delta.add_target(0, 4, {9, 9}); // Duplicate target down a different path.
		// The successors are a set of *states*, not the inner post that used to be returned: before
		//  Phase 5 this member's return type was `Nested`, which above arity 1 is the post one level
		//  down rather than the targets at the bottom.
		static_assert(std::same_as<decltype(delta.get_successors(0)), StateSet>);
		CHECK(delta.get_successors(0) == StateSet{2, 4});
	}

	SECTION("the keyed form gives targets at arity 2 as well, not the post below") {
		D2 delta{};
		delta.add_target(0, 4, {1, 1});
		delta.add_target(0, 2, {1, 5}); // Same level-0 key, different level-1 key.
		delta.add_target(0, 7, {9, 9}); // Different level-0 key: must not leak in.

		// Everything under key 1, gathered across every level-1 key beneath it.
		CHECK(delta.get_successors(0, 1) == StateSet{2, 4});
		CHECK(delta.get_successors(0, 9) == StateSet{7});
		CHECK(delta.get_successors(0, 3).empty()); // Absent key, not a wrong answer.
		CHECK(delta.get_successors(1, 1).empty()); // Absent state.
	}

	SECTION("at arity 1 the keyed form is still a reference into the relation") {
		Delta delta{};
		delta.add(0, 1, 5);
		delta.add(0, 1, 6);
		// Not merely equal to the stored set — the *same object*, which is what lets
		// mata::nfa::Nfa::post() hand it straight out without a copy.
		CHECK(&delta.get_successors(0, 1) == &delta.state_post(0).find(1)->targets);
		CHECK(delta.get_successors(0, 1) == StateSet{5, 6});
		CHECK(delta.get_successors(0, 2).empty()); // The static empty set, not a dangling reference.
	}
}
