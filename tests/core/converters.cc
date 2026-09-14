/** @file
 * @brief The NFT→NFA converters, including the branch that no shipping build compiles.
 *
 * `Nft::to_nfa()` picks between an O(1) hand-over of the relation and a transcription, with
 *  `if constexpr` on whether `mata::nft::Delta` and `mata::nfa::Delta` are the same type. They are,
 *  today — which means **the rebuild branch is discarded and never compiled**. `if constexpr` does
 *  not instantiate the branch it does not take, so that code could be arbitrarily broken and every
 *  build, every test and every gate would stay green.
 *
 * That is the same shape as the two bugs this refactor already found by accident: the epsilon
 *  defaults that only ever resolved to core's constant, and the O(1) `back()` shortcut in
 *  `epsilon_symbol_posts` that is only correct while epsilon is the greatest key. Both were
 *  invisible until a relation disagreed.
 *
 * So the tests below force the branch by naming a *different* relation type explicitly —
 *  `to_nfa<Other>()` — and then check the transcription against the hand-over it replaces. Without
 *  this file, T6.5 would be a feature that compiles for the first time on the day somebody needs it.
 */

#include <catch2/catch_test_macros.hpp>

#include <concepts>

#include "mata/nfa/nfa.hh"
#include "mata/nft/nft.hh"

using namespace mata;

namespace {

/// A relation that is a *distinct type* from @c mata::nfa::Delta while agreeing with it on
///  everything a transcription needs: same key type, same arity, same state, same target. Only the
///  reserved tail differs, which is enough to make it a different type and is exactly the kind of
///  divergence §3.8 says a module may want (an NFT reserving `DONT_CARE`).
using OtherDelta = posts::RelationOf<ReservedKeys<Symbol, EPSILON, EPSILON - 2>, StateSet>;

static_assert(!std::same_as<OtherDelta, nfa::Delta>, "or it would not force the rebuild branch");
static_assert(OtherDelta::key_arity == nfa::Delta::key_arity);
static_assert(std::same_as<OtherDelta::State, nfa::Delta::State>);
static_assert(std::same_as<OtherDelta::Target, nfa::Delta::Target>);

/// Probes for whether the O(1) members are *available*, which is the guarantee they carry.
template <typename D> concept MovableTo = requires(nft::Nft a) { a.template to_nfa_move<D>(); };
template <typename D> concept CopyableTo = requires(const nft::Nft a) { a.template to_nfa_copy<D>(); };

nft::Nft sample() {
	nft::Nft aut{};
	aut.delta.add(0, 1, 1);
	aut.delta.add(0, 2, 2);
	aut.delta.add(1, 3, 2);
	aut.delta.add(2, EPSILON, 0); // a reserved key, to be sure the tail is carried across
	aut.initial.insert(0);
	aut.final.insert(2);
	return aut;
}

} // namespace.

/// @name The O(1) guarantee is in the type system
///
/// @c to_nfa_move and @c to_nfa_copy exist only while the hand-over is actually available. The day
///  the two relations diverge they *disappear*, and their call sites in `nft/inclusion.cc` and
///  `nft/nft.cc` stop compiling — which is the point, because the alternative is those hot paths
///  silently becoming a rebuild.
///@{
static_assert(MovableTo<nft::Delta> && CopyableTo<nft::Delta>); ///< available today
static_assert(!MovableTo<OtherDelta> && !CopyableTo<OtherDelta>); ///< and gone the moment it is not
///@}

TEST_CASE("mata::nft::Nft::to_nfa — moves when it can, rebuilds when it cannot") {
	SECTION("the two branches produce the same automaton") {
		// The O(1) branch: nft::Delta is nfa::Delta, so the relation is handed over.
		nft::Nft a{sample()};
		const nfa::Nfa moved{a.to_nfa()};

		// The rebuild branch, forced by naming a relation type that is not nfa::Delta. This is the
		// only thing in the build that compiles it at all.
		nft::Nft b{sample()};
		const nfa::Nfa rebuilt{b.to_nfa<OtherDelta>()};

		CHECK(moved.is_identical(rebuilt));
		CHECK(rebuilt.num_of_states() == moved.num_of_states());
		CHECK(rebuilt.delta.num_of_transitions() == 4);
		CHECK(rebuilt.initial == moved.initial);
		CHECK(rebuilt.final == moved.final);
		// The reserved key survives transcription like any other.
		CHECK(rebuilt.delta.contains(2, EPSILON, 0));
	}

	SECTION("both branches leave the NFT empty, so the postcondition does not depend on the branch") {
		nft::Nft a{sample()};
		(void)a.to_nfa();
		CHECK(a.delta.empty());

		nft::Nft b{sample()};
		(void)b.to_nfa<OtherDelta>();
		CHECK(b.delta.empty()); // the rebuild reads rather than moves, so it must clear explicitly
	}

	SECTION("an empty transducer converts to an empty automaton, either way") {
		nft::Nft a{};
		nft::Nft b{};
		const nfa::Nfa moved{a.to_nfa()};
		const nfa::Nfa rebuilt{b.to_nfa<OtherDelta>()};
		CHECK(moved.delta.empty());
		CHECK(rebuilt.delta.empty());
		CHECK(moved.is_identical(rebuilt));
	}
}
