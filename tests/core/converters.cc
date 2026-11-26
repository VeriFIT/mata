/** @file
 * @brief The NFT2NFA converters.
 */

#include <catch2/catch_test_macros.hpp>

#include "mata/nfa/nfa.hh"
#include "mata/nft/nft.hh"

using namespace mata;

namespace {

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

TEST_CASE("mata::nft::Nft::to_nfa_copy and to_nfa_move") {
	SECTION("copy and move produce the same automaton") {
		const nft::Nft a{sample()};
		const nfa::Nfa copied{a.to_nfa_copy()};
		CHECK(a.delta.num_of_transitions() == 4); // the source is untouched

		nft::Nft b{sample()};
		const nfa::Nfa moved{b.to_nfa_move()};

		CHECK(copied.is_identical(moved));
		CHECK(moved.delta.num_of_transitions() == 4);
		CHECK(moved.initial == copied.initial);
		CHECK(moved.final == copied.final);
		CHECK(moved.delta.contains(2, EPSILON, 0));
	}

	SECTION("an empty transducer converts to an empty automaton") {
		const nft::Nft a{};
		nft::Nft b{};
		const nfa::Nfa copied{a.to_nfa_copy()};
		const nfa::Nfa moved{b.to_nfa_move()};
		CHECK(copied.delta.empty());
		CHECK(moved.delta.empty());
		CHECK(copied.is_identical(moved));
	}
}
