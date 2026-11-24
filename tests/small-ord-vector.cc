/* small-ord-vector.cc -- tests of SmallOrdVector
 */

#include <cstdint>
#include <random>
#include <set>
#include <vector>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mata/nfa/delta.hh"
#include "mata/utils/small-ord-vector.hh"

using mata::utils::OrdVector;
using mata::utils::SmallOrdVector;

namespace {

template <class Set> std::vector<typename Set::value_type> elements(const Set& set) { return {set.begin(), set.end()}; }

} // namespace

TEST_CASE("mata::utils::SmallOrdVector stores a single target inline") {
	using Set = SmallOrdVector<std::uint64_t>;
	STATIC_CHECK(Set::inline_capacity == 1);
	STATIC_CHECK(sizeof(Set) == 16);
	STATIC_CHECK(SmallOrdVector<std::uint32_t>::inline_capacity == 2);
	// `symbol` fills the tail padding of the inline target set.
	STATIC_CHECK(sizeof(mata::nfa::SymbolPost) == 16);

	Set set{};
	CHECK(set.empty());
	CHECK(set.is_inline());
	CHECK(set.insert(7).second);
	CHECK_FALSE(set.insert(7).second);
	CHECK(set.is_inline());
	CHECK(set.size() == 1);
	CHECK(set.front() == 7);
	CHECK(set.contains(7));
	CHECK_FALSE(set.contains(3));

	// A second target spills to the heap, keeping the order.
	CHECK(set.insert(3).second);
	CHECK_FALSE(set.is_inline());
	CHECK(elements(set) == std::vector<std::uint64_t>{3, 7});

	// Erasing keeps the heap block, a copy is made inline.
	CHECK(set.erase(3) == 1);
	CHECK(set.erase(3) == 0);
	CHECK_FALSE(set.is_inline());
	const Set copy{set};
	CHECK(copy.is_inline());
	CHECK(copy == set);

	set.clear();
	CHECK(set.empty());
	CHECK(set.capacity() >= 2);
}

TEST_CASE("mata::utils::SmallOrdVector erases the key zero given as a literal") {
	mata::nfa::SmallStateSet targets{0, 2, 5};
	CHECK(targets.erase(0) == 1);
	CHECK(targets.erase(0) == 0);
	CHECK(elements(targets) == std::vector<mata::nfa::State>{2, 5});
	// The iterator overload still accepts both iterator types.
	CHECK(*targets.erase(targets.begin()) == 5);
	const auto after_last{targets.erase(targets.cbegin())};
	CHECK(after_last == targets.end());
	CHECK(targets.empty());
}

TEST_CASE("mata::utils::SmallOrdVector copy, move and assignment") {
	using Set = SmallOrdVector<std::uint64_t>;
	Set big{9, 1, 5, 3, 1};
	CHECK(elements(big) == std::vector<std::uint64_t>{1, 3, 5, 9});
	CHECK(big.capacity() == 5); // Reserved for the input, before deduplication.

	Set copy{big};
	CHECK(copy == big);
	CHECK(copy.capacity() == 4);

	Set moved{std::move(copy)};
	CHECK(moved == big);
	CHECK(copy.empty()); // NOLINT(bugprone-use-after-move): a moved-from set is empty.
	CHECK(copy.is_inline());

	Set single{4};
	single = big;
	CHECK(single == big);
	single = Set{2};
	CHECK(elements(single) == std::vector<std::uint64_t>{2});
	single = std::move(moved);
	CHECK(single == big);

	Set& self{single};
	single = self;
	CHECK(single == big);
	single = std::move(self);
	CHECK(single == big);

	single = OrdVector<std::uint64_t>{8, 6};
	CHECK(single == OrdVector<std::uint64_t>{6, 8});
	CHECK(OrdVector<std::uint64_t>{6, 8} == single);
	CHECK(Set{std::span<const std::uint64_t>{big}} == big);
}

TEST_CASE("mata::utils::SmallOrdVector inserts sorted ranges") {
	using Set = SmallOrdVector<std::uint64_t>;
	Set set{};
	CHECK_FALSE(set.insert(std::span<const std::uint64_t>{}));
	CHECK(set.insert(OrdVector<std::uint64_t>{4}));
	CHECK(set.is_inline());
	CHECK(set.insert(OrdVector<std::uint64_t>{5, 6})); // Appended.
	CHECK(set.insert(OrdVector<std::uint64_t>{1, 5, 8}));
	CHECK(elements(set) == std::vector<std::uint64_t>{1, 4, 5, 6, 8});
	CHECK_FALSE(set.insert(OrdVector<std::uint64_t>{1, 8}));
	CHECK_FALSE(set.insert(set));
	CHECK(elements(set) == std::vector<std::uint64_t>{1, 4, 5, 6, 8});
}

TEMPLATE_TEST_CASE(
	"mata::utils::SmallOrdVector behaves like std::set",
	"",
	SmallOrdVector<std::uint64_t>,
	(SmallOrdVector<std::uint64_t, 2>),
	SmallOrdVector<std::uint32_t>,
	(SmallOrdVector<std::uint32_t, 4>)
) {
	using Key = TestType::value_type;
	std::mt19937 rng{804};
	for (int round{0}; round < 200; ++round) {
		TestType set{};
		std::set<Key> expected{};
		const Key universe{static_cast<Key>(1 + rng() % 24)};
		for (int op{0}; op < 64; ++op) {
			const Key key{static_cast<Key>(rng() % universe)};
			switch (rng() % 8) {
				case 0:
				case 1:
				case 2:
					CHECK(set.insert(key).second == expected.insert(key).second);
					break;
				case 3:
					CHECK(set.erase(key) == expected.erase(key));
					break;
				case 4: {
					std::set<Key> other{};
					const auto count{rng() % 6};
					for (unsigned i{0}; i < count; ++i) { other.insert(static_cast<Key>(rng() % universe)); }
					const OrdVector<Key> sorted{other};
					const bool grows{!std::ranges::includes(expected, other)};
					CHECK(set.insert(sorted) == grows);
					expected.insert(other.begin(), other.end());
					break;
				}
				case 5: {
					const TestType copy{set};
					set = copy;
					CHECK(copy == set);
					break;
				}
				case 6: {
					TestType moved{std::move(set)};
					set = std::move(moved);
					break;
				}
				default: {
					set.erase_if([&](const Key k) { return k % 3 == key % 3; });
					std::erase_if(expected, [&](const Key k) { return k % 3 == key % 3; });
					break;
				}
			}
			REQUIRE(elements(set) == std::vector<Key>(expected.begin(), expected.end()));
			REQUIRE(set.contains(key) == expected.contains(key));
		}
		TestType unordered{};
		for (const Key key : std::vector<Key>(expected.rbegin(), expected.rend())) { unordered.push_back(key); }
		unordered.push_back(universe);
		unordered.push_back(universe);
		unordered.sort_and_rmdupl();
		expected.insert(universe);
		CHECK(elements(unordered) == std::vector<Key>(expected.begin(), expected.end()));
	}
}

TEST_CASE("mata::nfa::SymbolPost structured bindings keep the symbol first") {
	mata::nfa::SymbolPost symbol_post{'a', mata::nfa::StateSet{2, 1}};
	auto& [symbol, targets]{symbol_post};
	CHECK(symbol == 'a');
	CHECK(targets == mata::nfa::StateSet{1, 2});
	targets.insert(3);
	CHECK(symbol_post.num_of_targets() == 3);
	const auto [symbol_copy, targets_copy]{symbol_post};
	CHECK(symbol_copy == 'a');
	CHECK(targets_copy == symbol_post.targets);
}
