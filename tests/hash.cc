/** @file hash.cc
 *  @brief Quality tests of the hash routines shared by the unordered containers of mata.
 *
 * The keys mata hashes are small, dense and sequential (state identifiers), which is exactly the input a weak
 *  combine spreads badly: #797 measured 65,185 distinct values for the 10^6 pairs below. Collisions are not a
 *  correctness problem, but libstdc++ reduces the hash modulo the bucket count, so every colliding key walks the
 *  same chain on each lookup. These tests pin the distinctness of the hashes on such key families.
 */

#include "mata/nfa/nfa.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/utils.hh"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace mata::utils;

/**
 * The smallest number of distinct hashes a family of @p count keys may produce.
 *
 * A 64-bit @c size_t holds these families with room to spare, so every key must hash to its own value. A 32-bit
 *  @c size_t cannot: the birthday bound makes about `count^2 / 2^33` collisions certain whatever the mixing is, so
 *  allow ten times that many and keep testing the mixing rather than the hash width.
 */
constexpr size_t min_distinct_hashes(const size_t count) {
	if constexpr (sizeof(size_t) >= 8) {
		return count;
	} else {
		const uint64_t pairs{static_cast<uint64_t>(count) * (count - 1) / 2};
		const uint64_t expected_collisions{pairs >> 32};
		return count - std::min<size_t>(count, static_cast<size_t>(10 * expected_collisions));
	}
}

TEST_CASE("mata::utils::hash_combine() mixes the running value") {
	// The pairs (1, 0) and (0, 63) collided under the pre-#797 combine, which added the shifted running value to
	//  the hash of the new element instead of mixing it.
	CHECK(hash_combine(hash_combine(HASH_SEED, 1u), 0u) != hash_combine(hash_combine(HASH_SEED, 0u), 63u));

	// Equal inputs must still hash equally, and the order of the combined elements must matter.
	CHECK(hash_combine(HASH_SEED, 7u) == hash_combine(HASH_SEED, 7u));
	CHECK(hash_combine(hash_combine(HASH_SEED, 1u), 2u) != hash_combine(hash_combine(HASH_SEED, 2u), 1u));
}

TEST_CASE("mata::utils::PairHash distinctness on sequential pairs") {
	constexpr unsigned bound{1'000};
	const PairHash<unsigned, unsigned> hasher{};
	std::unordered_set<size_t> hashes;
	hashes.reserve(static_cast<size_t>(bound) * bound);
	for (unsigned first{0}; first < bound; ++first) {
		for (unsigned second{0}; second < bound; ++second) { hashes.insert(hasher({first, second})); }
	}
	CHECK(hashes.size() >= min_distinct_hashes(static_cast<size_t>(bound) * bound));
}

TEST_CASE("mata::utils::hash_range() distinctness on two-element state sets") {
	constexpr mata::nfa::State bound{1'000};
	std::unordered_set<size_t> hashes;
	for (mata::nfa::State first{0}; first < bound; ++first) {
		for (mata::nfa::State second{first + 1}; second < bound; ++second) {
			const OrdVector<mata::nfa::State> set{first, second};
			hashes.insert(hash_range(set.begin(), set.end()));
		}
	}
	CHECK(hashes.size() >= min_distinct_hashes(static_cast<size_t>(bound) * (bound - 1) / 2));
}

TEST_CASE("mata::utils::hash_range() distinguishes a one-element range from its element") {
	const std::vector<unsigned> single{42};
	CHECK(hash_range(single.begin(), single.end()) != std::hash<unsigned>{}(42));

	// An empty range must not hash to the same value as a range holding a zero.
	const std::vector<unsigned> zero{0};
	CHECK(hash_range(single.begin(), single.begin()) != hash_range(zero.begin(), zero.end()));
}

TEST_CASE("std::hash<mata::nfa::Transition> distinctness on a dense transition grid") {
	constexpr mata::nfa::State bound{100};
	const std::hash<mata::nfa::Transition> hasher{};
	std::unordered_set<size_t> hashes;
	for (mata::nfa::State source{0}; source < bound; ++source) {
		for (mata::Symbol symbol{0}; symbol < bound; ++symbol) {
			for (mata::nfa::State target{0}; target < bound; ++target) {
				hashes.insert(hasher(mata::nfa::Transition{source, symbol, target}));
			}
		}
	}
	CHECK(hashes.size() >= min_distinct_hashes(static_cast<size_t>(bound) * bound * bound));

	// The three components must not be interchangeable.
	CHECK(hasher({1, 2, 3}) != hasher({3, 2, 1}));
	CHECK(hasher({1, 2, 3}) == hasher({1, 2, 3}));
}
