#include <algorithm>
#include <cstdint>
#include <iterator>
#include <random>
#include <ranges>
#include <set>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nfa/nfa.hh"

using namespace mata::utils;
using namespace mata::nfa;

TEST_CASE("mata::utils::SparseSet") {
	SparseSet<State> p;

	SECTION("basic functionality: insert, erase, access, constructor, size, get_elements ") {
		std::vector<State> v = {1, 2, 3, 4, 5};
		p.insert(v);
		CHECK(std::vector<State>(p.begin(), p.end()) == v);
		CHECK(p.size() == 5);
		p.erase({2, 4});
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({1, 3, 5}));
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({1, 3, 5}));
		CHECK(p.size() == 3);
		p.insert({1, 3, 5});
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({1, 3, 5}));
		p.erase({1, 2, 3, 4, 5});
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({}));
		for (State q = 0; q < 10; q++) { CHECK(!p[q]); }
		CHECK(p.empty());
	}

	SECTION("iterator ") {
		p.insert({1, 2, 3, 4, 5});
		State i = 1;
		for (auto q : p) {
			CHECK(q == i);
			i++;
		}
		CHECK(i == 6);
	}

	SECTION("accessing stuff outside current domain ") {
		CHECK(!p[100]);
		p.insert(100);
		CHECK(p[100]);
		CHECK(!p[99]);
		CHECK(!p[101]);
		CHECK(p.size() == 1);
	}

	SECTION("complement ") {
		p = {2, 4};
		p.complement(5);
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({0, 1, 3}));
		p.complement(6);
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({2, 4, 5}));

		p = {2, 4, 6, 8};
		p.complement(6);
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({0, 1, 3, 5}));

		p.complement(3);
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({2}));
	}

	SECTION("filter") {
		p = {0, 1, 2, 3, 4, 5, 6};
		mata::BoolVector v = {0, 1, 1};
		auto f = [&v](State x) { return x < v.size() ? v[x] : 0; };
		p.filter(f);
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({1, 2}));
		v = {1, 1, 1, 1, 1};
		p.filter(f);
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({1, 2}));
		v = {0, 0, 0, 0, 0};
		p.filter(f);
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({}));
		p = {0, 1, 2, 3, 4, 5, 6};
		v = {};
		p.filter(f);
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({}));
		p = {};
		v = {0, 1, 1};
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({}));
	}

	SECTION("sort") {
		p = {1, 0, 2, 4, 6, 3, 5};
		p.sort();
		CHECK(std::vector<State>(p.begin(), p.end()) == std::vector<State>{0, 1, 2, 3, 4, 5, 6});
	}

	SECTION("rename") {
		p = {1, 0, 4, 2};
		std::vector<State> r = {3, 4, 5, 6, 1};
		auto f = [&r](State x) { return r[x]; };
		p.rename(f);
		CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({4, 3, 1, 5}));
	}

	SECTION("max") {
		p = {1, 0, 4, 2};
		CHECK(p.max() == 4);
	}

	SECTION("truncate") {
		p = {1, 0, 2, 4, 6, 3, 5};
		p.erase(4);
		p.erase(6);
		p.erase(5);
		p.truncate();
		CHECK(p.domain_size() == 4);
	}
}

TEST_CASE("mata::utils::SparseSet equality compares the contents - issue #769") {
	SparseSet<unsigned long> inserted_in_order{};
	inserted_in_order.insert(5);
	inserted_in_order.insert(3);

	SparseSet<unsigned long> inserted_in_other_order{};
	inserted_in_other_order.insert(3);
	inserted_in_other_order.insert(5);
	CHECK(inserted_in_order == inserted_in_other_order);

	SparseSet<unsigned long> with_bigger_domain{};
	with_bigger_domain.insert(100);
	with_bigger_domain.erase(100);
	with_bigger_domain.insert(3);
	with_bigger_domain.insert(5);
	CHECK(with_bigger_domain == inserted_in_order);

	inserted_in_other_order.insert(7);
	CHECK(inserted_in_order != inserted_in_other_order);
	CHECK(inserted_in_other_order != inserted_in_order);

	// Equal size but different elements: {3,5} vs {3,7}.
	SparseSet<unsigned long> with_other_element{};
	with_other_element.insert(3);
	with_other_element.insert(7);
	CHECK(inserted_in_order != with_other_element);
	CHECK(with_other_element != inserted_in_order);
}

TEST_CASE("mata::utils::SparseSet constructors - issue #769") {
	SECTION("a non-const SparseSet is copied, not re-inserted") {
		SparseSet<unsigned long> original{};
		original.insert(100);
		original.erase(100);
		original.insert(4);
		SparseSet<unsigned long> copy{original};
		CHECK(copy == original);
		CHECK(copy.domain_size() == original.domain_size());
	}

	SECTION("a non-const BoolVector gives the indices of its set bits") {
		mata::BoolVector bool_vector{0, 1, 0, 1};
		const SparseSet<unsigned long> from_bool_vector{bool_vector};
		CHECK(from_bool_vector == SparseSet<unsigned long>{1, 3});
	}

	SECTION("a const and a temporary container") {
		const std::vector<unsigned long> values{1, 2, 3};
		const SparseSet<unsigned long> from_const{values};
		CHECK(from_const == SparseSet<unsigned long>{1, 2, 3});
		const SparseSet<unsigned long> from_temporary{std::vector<unsigned long>{4, 6}};
		CHECK(from_temporary == SparseSet<unsigned long>{4, 6});
	}

	SECTION("a named filter view still constructs") {
		std::vector<unsigned long> values{1, 2, 3, 4, 5};
		auto evens = values | std::views::filter([](const unsigned long v) { return v % 2 == 0; });
		const SparseSet<unsigned long> from_filter{evens};
		CHECK(from_filter == SparseSet<unsigned long>{2, 4});
	}

	SECTION("a built-in array") {
		const unsigned long values[]{2, 4};
		CHECK(SparseSet<unsigned long>{values} == SparseSet<unsigned long>{2, 4});
		SparseSet<unsigned long> from_array{};
		from_array.insert(values);
		CHECK(from_array == SparseSet<unsigned long>{2, 4});
		from_array.erase(values);
		CHECK(from_array.empty());
	}

	SECTION("a range whose sentinel differs from its iterator") {
		const std::vector<unsigned long> values{3, 5, 7};
		const auto counted{std::ranges::subrange(std::counted_iterator(values.begin(), 2), std::default_sentinel)};
		CHECK(SparseSet<unsigned long>{counted} == SparseSet<unsigned long>{3, 5});
	}

	SECTION("elements convertible only from a reference") {
		struct LvalueOnly {
			unsigned long value;
			operator unsigned long() const& { return value; } // NOLINT(google-explicit-constructor)
			operator unsigned long() && = delete;
		};
		const std::vector<LvalueOnly> values{{1}, {6}};
		CHECK(SparseSet<unsigned long>{values} == SparseSet<unsigned long>{1, 6});
	}
}

TEST_CASE("mata::utils::SparseSet<uint8_t> with every value - issue #769") {
	// 256 elements: an index of type uint8_t would wrap from 255 to 0 and never leave the loops.
	SparseSet<uint8_t> full{};
	for (unsigned value{0}; value < 256; ++value) { full.insert(static_cast<uint8_t>(value)); }
	CHECK(full.size() == 256);
	CHECK(full.max() == 255);
	full.sort();
	full.rename([](const uint8_t value) { return value; });
	full.filter([](const uint8_t value) { return value % 2 == 0; });
	CHECK(full.size() == 128);
	CHECK(full.max() == 254);
}

namespace {

/// Draw a value in [0, bound) from the raw generator output, portably across standard libraries.
size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

/// The elements of @p set in ascending order, comparable with @c elements() of a @c std::set.
std::vector<State> sorted_elements(const SparseSet<State>& set) {
	std::vector<State> elements{set.begin(), set.end()};
	std::ranges::sort(elements);
	return elements;
}

std::vector<State> elements(const std::set<State>& set) { return {set.begin(), set.end()}; }

} // namespace

TEST_CASE("mata::utils::SparseSet against std::set (randomized)") {
	constexpr State UNIVERSE{40};
	for (unsigned seed{0}; seed < 300; ++seed) {
		std::mt19937 rng{seed};
		INFO("seed " << seed);
		std::set<State> expected{};
		std::vector<State> initial(draw(rng, 12));
		for (State& state : initial) {
			state = static_cast<State>(draw(rng, UNIVERSE));
			expected.insert(state);
		}
		SparseSet<State> set(initial.begin(), initial.end());
		REQUIRE(sorted_elements(set) == elements(expected));

		for (size_t step{0}; step < 60; ++step) {
			INFO("step " << step);
			const State value{static_cast<State>(draw(rng, UNIVERSE))};
			switch (draw(rng, 11)) {
				case 0:
				case 1:
				case 2: {
					set.insert(value);
					expected.insert(value);
					break;
				}
				case 3:
				case 4: {
					set.erase(value);
					expected.erase(value);
					break;
				}
				case 5: {
					// Complement within a domain that may be smaller or larger than the current one.
					const State domain{static_cast<State>(draw(rng, UNIVERSE + 1))};
					set.complement(domain);
					std::set<State> complement{};
					for (State state{0}; state < domain; ++state) {
						if (!expected.contains(state)) { complement.insert(state); }
					}
					expected = std::move(complement);
					break;
				}
				case 6: {
					const State modulus{static_cast<State>(1 + draw(rng, 4))};
					set.filter([&](const State state) { return state % modulus != 0; });
					std::erase_if(expected, [&](const State state) { return state % modulus == 0; });
					break;
				}
				case 7: {
					// rename() keeps a set only under an injective renaming, so draw a permutation of [0, 61), which
					//  contains every reachable value: an affine map modulo the prime 61.
					constexpr State RENAMING_MODULUS{61};
					const State factor{static_cast<State>(1 + draw(rng, RENAMING_MODULUS - 1))};
					const State offset{static_cast<State>(draw(rng, RENAMING_MODULUS))};
					const auto renaming = [&](const State state) {
						return (state * factor + offset) % RENAMING_MODULUS;
					};
					set.rename(renaming);
					std::set<State> renamed{};
					for (const State state : expected) { renamed.insert(renaming(state)); }
					expected = std::move(renamed);
					break;
				}
				case 8: {
					set.truncate();
					CHECK(set.domain_size() == static_cast<State>(expected.empty() ? 0 : *expected.rbegin() + 1));
					break;
				}
				case 9: {
					// Aliased erase(set): walking this->begin()..end() would read a pop_back-ed dense array.
					set.erase(set);
					expected.clear();
					break;
				}
				default: {
					set.sort();
					CHECK(std::ranges::is_sorted(set));
					break;
				}
			}

			REQUIRE(sorted_elements(set) == elements(expected));
			CHECK(set.size() == expected.size());
			CHECK(set.empty() == expected.empty());
			if (!expected.empty()) { CHECK(set.max() == *expected.rbegin()); }
			const State sweep_bound{std::max<State>(UNIVERSE + 1, static_cast<State>(set.domain_size()))};
			for (State state{0}; state < sweep_bound; ++state) {
				CHECK(set.contains(state) == expected.contains(state));
				CHECK(set[state] == expected.contains(state));
			}
		}

		// Disjointness against a second random set.
		std::vector<State> other_values(draw(rng, 12));
		for (State& state : other_values) { state = static_cast<State>(draw(rng, UNIVERSE)); }
		const SparseSet<State> other(other_values.begin(), other_values.end());
		const bool disjoint{std::ranges::none_of(other_values, [&](const State state) {
			return expected.contains(state);
		})};
		CHECK(are_disjoint(set, other) == disjoint);
		CHECK(are_disjoint(other, set) == disjoint);
		CHECK(set.intersects_with(other_values) == !disjoint);

		// A copy keeps the contents and is independent of the original.
		SparseSet<State> copy{set};
		CHECK(sorted_elements(copy) == elements(expected));
		copy.clear();
		CHECK(copy.empty());
		CHECK(sorted_elements(set) == elements(expected));
	}
}

TEST_CASE("mata::utils::SparseSet::erase(set) clears when the range aliases the set - #224") {
	SparseSet<State> set{std::initializer_list<State>{1, 3, 5}};
	REQUIRE(set.size() == 3);
	set.erase(set);
	CHECK(set.empty());
	CHECK(set.size() == 0);
	CHECK(!set.contains(1));
	// The domain size survives: clear() empties the set, not the reserved domain.
	CHECK(set.domain_size() == 6);

	SparseSet<State> empty{};
	empty.erase(empty);
	CHECK(empty.empty());

	SparseSet<State> one{std::initializer_list<State>{7}};
	one.erase(one);
	CHECK(one.empty());
	CHECK(one.domain_size() == 8);
}
