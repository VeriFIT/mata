#include <algorithm>
#include <random>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/utils/ord-vector.hh"
#include "mata/utils/synchronized-iterator.hh"
#include "mata/utils/utils.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::SynchronizedIterator") {
	SECTION("synchronized_universal_iterator, basic functionality") {
		SynchronizedUniversalIterator<OrdVector<int>::const_iterator> iu;

		// Basic functionality, position[0] gets emptied first
		OrdVector<int> v1{1, 2, 4};
		OrdVector<int> v2{0, 1, 3, 5};
		OrdVector<int> v3{0, 1, 2, 4};

		push_back(iu, v1);
		push_back(iu, v2);
		push_back(iu, v3);

		REQUIRE(iu.advance());
		auto current = iu.get_current();
		REQUIRE(current.size() == 3);
		REQUIRE(*current[0] == 1);
		REQUIRE(*current[1] == 1);
		REQUIRE(*current[2] == 1);
		REQUIRE(!iu.advance());

		iu.reset();
		REQUIRE(iu.get_current().empty());

		// Basic functionality, position[0] does not get emptied first
		v1 = {1, 2, 3, 4, 5};
		v2 = {0, 1, 3};
		v3 = {1, 2, 3};

		push_back(iu, v1);
		push_back(iu, v2);
		push_back(iu, v3);

		REQUIRE(iu.advance());
		current = iu.get_current();
		REQUIRE(current.size() == 3);
		REQUIRE(*current[0] == 1);
		REQUIRE(*current[1] == 1);
		REQUIRE(*current[2] == 1);
		REQUIRE(iu.advance());
		current = iu.get_current();
		REQUIRE(current.size() == 3);
		REQUIRE(*current[0] == 3);
		REQUIRE(*current[1] == 3);
		REQUIRE(*current[2] == 3);
		REQUIRE(!iu.advance());
	}

	SECTION("synchronized_universal_iterator, corner cases") {
		SynchronizedUniversalIterator<OrdVector<int>::const_iterator> iu;

#ifdef NDEBUG
		// Zero inputs are not a legal state (the release fallback returns false); debug builds assert.
		REQUIRE(!iu.advance());
		REQUIRE(!iu.advance());
		const auto& no_inputs = iu.get_current();
		REQUIRE(no_inputs.empty());
#endif // NDEBUG

		OrdVector<int> v1{};
		OrdVector<int> v2{1};
		OrdVector<int> v3{};

		push_back(iu, v1);
		push_back(iu, v2);
		push_back(iu, v3);

		// Some inputs are empty: synchronization is impossible.
		REQUIRE(!iu.advance());

		// Empty after reset
		iu.reset();
		const auto& after_empty_input = iu.get_current();
		REQUIRE(after_empty_input.empty());

		// Only empty vectors
		push_back(iu, v1);

		REQUIRE(!iu.advance());

		push_back(iu, v3);
		iu.reset();
		REQUIRE(iu.get_current().empty());

		// Insert the same vector twice
		OrdVector<int> v4{1, 2};
		OrdVector<int> v5{2};

		push_back(iu, v4);
		push_back(iu, v4);
		push_back(iu, v5);

		REQUIRE(iu.advance());
		const auto& current = iu.get_current();
		REQUIRE(*current[0] == 2);
		REQUIRE(*current[1] == 2);
		REQUIRE(*current[2] == 2);
		REQUIRE(!iu.advance());
	}

	SECTION("synchronized_universal_iterator, three inputs where input 0 moves past input 1 - issue #769") {
		// All three inputs live in one buffer with input 0 first, so the outcome does not depend on where separate
		//  containers happen to be allocated. Only 9 is common to all three.
		const std::vector<int> buffer{1, 2, 5, 9, /* input 1 */ 2, 9, /* input 2 */ 5, 9};
		SynchronizedUniversalIterator<std::vector<int>::const_iterator> sync_iterator;
		sync_iterator.push_back(buffer.begin(), buffer.begin() + 4);
		sync_iterator.push_back(buffer.begin() + 4, buffer.begin() + 6);
		sync_iterator.push_back(buffer.begin() + 6, buffer.end());

		REQUIRE(sync_iterator.advance());
		const auto& current = sync_iterator.get_current();
		REQUIRE(current.size() == 3);
		CHECK(*current[0] == 9);
		CHECK(*current[1] == 9);
		CHECK(*current[2] == 9);
		CHECK(!sync_iterator.advance());
	}

	SECTION("SynchronizedExistentialIterator, basic functionality") {
		SynchronizedExistentialIterator<OrdVector<int>::const_iterator> ie;

		// Basic functionality
		OrdVector<int> v1{1, 2};
		OrdVector<int> v2{0, 3};
		OrdVector<int> v3{0, 1, 2, 3};

		push_back(ie, v1);
		push_back(ie, v2);
		push_back(ie, v3);

		int i = 0;
		while (ie.advance()) {
			auto current = ie.get_current();
			REQUIRE(current.size() == 2);
			REQUIRE(*current[0] == i);
			REQUIRE(*current[1] == i);
			i++;
		};
		REQUIRE(i == 4);
	}

	SECTION("SynchronizedExistentialIterator, corner cases") {
		SynchronizedExistentialIterator<OrdVector<int>::const_iterator> ie;

		// Empty iterator
		REQUIRE(!ie.advance());
		REQUIRE(!ie.advance());
		auto current = ie.get_current();
		REQUIRE(current.empty());

		// Empty vectors
		OrdVector<int> v1{};
		OrdVector<int> v2{1};
		OrdVector<int> v3{};

		push_back(ie, v1);
		push_back(ie, v2);
		push_back(ie, v3);

		REQUIRE(ie.advance());
		current = ie.get_current();
		REQUIRE(*current[0] == 1);
		REQUIRE(current.size() == 1);
		REQUIRE(!ie.advance());

		// Empty after reset
		ie.reset();
		REQUIRE(!ie.advance());
		current = ie.get_current();
		REQUIRE(current.empty());
		REQUIRE(!ie.advance());
		current = ie.get_current();
		REQUIRE(current.empty());

		// Only empty vectors
		push_back(ie, v1);

		REQUIRE(!ie.advance());

		push_back(ie, v3);
		ie.reset();

		REQUIRE(!ie.advance());

		// Insert the same vector twice
		OrdVector<int> v4{1, 2};
		OrdVector<int> v5{2};

		push_back(ie, v4);
		push_back(ie, v5);
		push_back(ie, v4);

		REQUIRE(ie.advance());
		current = ie.get_current();
		REQUIRE(current.size() == 2);
		REQUIRE(*current[0] == 1);
		REQUIRE(*current[1] == 1);
		REQUIRE(ie.advance());
		current = ie.get_current();
		REQUIRE(current.size() == 3);
		REQUIRE(*current[0] == 2);
		REQUIRE(*current[1] == 2);
		REQUIRE(*current[2] == 2);
		REQUIRE(!ie.advance());
	}
}

#ifdef NDEBUG
TEST_CASE("mata::utils::SynchronizedUniversalIterator over zero inputs in release - issue #797") {
	SynchronizedUniversalIterator<OrdVector<int>::const_iterator> sync_iterator;
	// Zero inputs are a precondition violation asserted in debug builds; release keeps returning false.
	REQUIRE(!sync_iterator.advance());
	REQUIRE(sync_iterator.get_current().empty());
}
#endif // NDEBUG

namespace {

/// Draw a value in [0, bound) from the raw generator output, portably across standard libraries.
size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

/// A sorted vector with unique random values in [0, @p universe).
std::vector<int> random_sorted_unique(std::mt19937& rng, const size_t universe) {
	std::vector<bool> present(universe, false);
	for (size_t i{0}; i < 1 + draw(rng, universe); ++i) { present[draw(rng, universe)] = true; }
	std::vector<int> values{};
	for (size_t i{0}; i < universe; ++i) {
		if (present[i]) { values.push_back(static_cast<int>(i)); }
	}
	return values;
}

} // namespace

TEST_CASE("mata::utils::SynchronizedUniversalIterator against a brute-force intersection (randomized)") {
	for (unsigned seed{0}; seed < 300; ++seed) {
		std::mt19937 rng{seed};
		INFO("seed " << seed);
		const size_t num_inputs{2 + draw(rng, 4)};
		const size_t universe{6 + draw(rng, 20)};
		std::vector<std::vector<int>> inputs(num_inputs);
		std::vector<int> intersection{};
		for (size_t input{0}; input < num_inputs; ++input) {
			inputs[input] = random_sorted_unique(rng, universe);
			if (input == 0) {
				intersection = inputs[0];
			} else {
				std::vector<int> smaller{};
				std::ranges::set_intersection(intersection, inputs[input], std::back_inserter(smaller));
				intersection = std::move(smaller);
			}
		}

		SynchronizedUniversalIterator<std::vector<int>::const_iterator> sync_iterator;
		for (const std::vector<int>& input : inputs) { push_back(sync_iterator, input); }

		std::vector<int> synchronizations{};
		while (sync_iterator.advance()) {
			const auto& current{sync_iterator.get_current()};
			REQUIRE(current.size() == num_inputs);
			const int value{*current[0]};
			for (const auto& position : current) { REQUIRE(*position == value); }
			synchronizations.push_back(value);
			REQUIRE(synchronizations.size() <= intersection.size());
		}
		CHECK(synchronizations == intersection);
		// A reset followed by the same inputs must repeat the run.
		sync_iterator.reset();
		for (const std::vector<int>& input : inputs) { push_back(sync_iterator, input); }
		std::vector<int> repeat{};
		while (sync_iterator.advance()) {
			repeat.push_back(*sync_iterator.get_current()[0]);
			REQUIRE(repeat.size() <= intersection.size());
		}
		CHECK(repeat == intersection);
	}
}

TEST_CASE("mata::utils::SynchronizedExistentialIterator against a brute-force merge (randomized)") {
	for (unsigned seed{0}; seed < 300; ++seed) {
		std::mt19937 rng{seed};
		INFO("seed " << seed);
		const size_t num_inputs{1 + draw(rng, 5)};
		const size_t universe{6 + draw(rng, 20)};
		std::vector<std::vector<int>> inputs(num_inputs);
		std::vector<int> merged{};
		for (size_t input{0}; input < num_inputs; ++input) {
			inputs[input] = random_sorted_unique(rng, universe);
			std::vector<int> next{};
			std::ranges::set_union(merged, inputs[input], std::back_inserter(next));
			merged = std::move(next);
		}
		// Skip the run when every input is empty: the existential iterator has no minimum then.
		if (merged.empty()) { continue; }

		SynchronizedExistentialIterator<std::vector<int>::const_iterator> sync_iterator;
		for (const std::vector<int>& input : inputs) { push_back(sync_iterator, input); }

		std::vector<int> minima{};
		while (sync_iterator.advance()) {
			const auto& synchronized{sync_iterator.get_current()};
			REQUIRE(!synchronized.empty());
			std::vector<int> current{};
			for (const auto& position : synchronized) { current.push_back(*position); }
			std::ranges::sort(current);
			// One position per input whose minimum is hit; a brute-force count of the value in the inputs.
			size_t expected_count{0};
			for (const std::vector<int>& input : inputs) {
				if (std::ranges::binary_search(input, current.front())) { ++expected_count; }
			}
			CHECK(current.size() == expected_count);
			for (const int value : current) { CHECK(value == current.front()); }
			minima.push_back(current.front());
			REQUIRE(minima.size() <= merged.size());
		}
		CHECK(minima == merged);

		sync_iterator.reset();
		for (const std::vector<int>& input : inputs) { push_back(sync_iterator, input); }
		std::vector<int> repeat{};
		while (sync_iterator.advance()) {
			repeat.push_back(*sync_iterator.get_current()[0]);
			REQUIRE(repeat.size() <= merged.size());
		}
		CHECK(repeat == merged);
	}
}
