/** @file
 * @brief The NFT2NFA converters, including the branch that no shipping build compiles.
 */

#include <catch2/catch_test_macros.hpp>

#include <concepts>

#include "mata/nfa/nfa.hh"
#include "mata/nft/nft.hh"

using namespace mata;

namespace {

// Same key, arity, state and target as nfa::Delta, but a different type (another reserved tail).
using OtherDelta = posts::RelationOf<ReservedKeys<Symbol, EPSILON, EPSILON - 2>, StateSet>;

static_assert(!std::same_as<OtherDelta, nfa::Delta>, "or it would not force the rebuild branch");
static_assert(OtherDelta::key_arity == nfa::Delta::key_arity);
static_assert(std::same_as<OtherDelta::State, nfa::Delta::State>);
static_assert(std::same_as<OtherDelta::Target, nfa::Delta::Target>);

template <typename D>
concept MovableTo = requires(nft::Nft a) { a.template to_nfa_move<D>(); };
template <typename D>
concept CopyableTo = requires(const nft::Nft a) { a.template to_nfa_copy<D>(); };

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

// to_nfa_move and to_nfa_copy exist only while nft::Delta is nfa::Delta.
static_assert(MovableTo<nft::Delta> && CopyableTo<nft::Delta>);
static_assert(!MovableTo<OtherDelta> && !CopyableTo<OtherDelta>);

TEST_CASE("mata::nft::Nft::to_nfa - moves when it can, rebuilds when it cannot") {
	SECTION("the two branches produce the same automaton") {
		// The O(1) branch: nft::Delta is nfa::Delta, so the relation is handed over.
		nft::Nft a{sample()};
		const nfa::Nfa moved{a.to_nfa()};

		// The rebuild branch, forced by a relation type that is not nfa::Delta.
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
		(void) a.to_nfa();
		CHECK(a.delta.empty());

		nft::Nft b{sample()};
		(void) b.to_nfa<OtherDelta>();
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
