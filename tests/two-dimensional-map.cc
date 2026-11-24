/* two-dimensional-map.cc -- tests of TwoDimensionalMap
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/utils/two-dimensional-map.hh"
#include "mata/utils/utils.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::TwoDimensionalMap::large_map == false") {
	SECTION("mata::utils::TwoDimensionalMap::insert() && get()") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get(1, 2) == 3);
		CHECK(map.get(4, 5) == 6);
		CHECK(map.get(1, 5) == 7);
		CHECK(map.get(4, 2) == 8);
		CHECK(map.get(0, 0) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(5, 5) == std::numeric_limits<unsigned>::max());
	}

	SECTION("mata::utils::TwoDimensionalMap::get_first_inverted()") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get_first_inverted(3) == 1);
		CHECK(map.get_first_inverted(6) == 4);
		CHECK(map.get_first_inverted(7) == 1);
		CHECK(map.get_first_inverted(8) == 4);
	}

	SECTION("mata::utils::TwoDimensionalMap::get_second_inverted()") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get_second_inverted(3) == 2);
		CHECK(map.get_second_inverted(6) == 5);
		CHECK(map.get_second_inverted(7) == 5);
		CHECK(map.get_second_inverted(8) == 2);
	}

	SECTION("big") {
		TwoDimensionalMap<unsigned> map(1'000, 1'000);
		for (unsigned i = 0; i < 1'000; ++i) {
			for (unsigned j = 0; j < 1'000; ++j) { map.insert(i, j, i + j); }
		}

		for (unsigned i = 0; i < 1'000; ++i) {
			for (unsigned j = 0; j < 1'000; ++j) { CHECK(map.get(i, j) == i + j); }
		}
	}

	SECTION("missing values in an allocated row") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 3);

		// The row of the first key 1 is allocated, the row of the first key 0 is not.
		CHECK(map.get(1, 0) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(1, 9) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(0, 2) == std::numeric_limits<unsigned>::max());
	}

	SECTION("zero is a regular value") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(3, 4, 0);

		CHECK(map.get(3, 4) == 0);
		CHECK(map.get(3, 5) == std::numeric_limits<unsigned>::max());
		CHECK(map.get_first_inverted(0) == 3);
		CHECK(map.get_second_inverted(0) == 4);
	}

	SECTION("inserting a smaller value keeps the larger inverted entries") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 5);
		map.insert(3, 4, 3);

		CHECK(map.get_first_inverted(5) == 1);
		CHECK(map.get_second_inverted(5) == 2);
		CHECK(map.get_first_inverted(3) == 3);
		CHECK(map.get_second_inverted(3) == 4);
		CHECK(map.get(1, 2) == 5);
		CHECK(map.get(3, 4) == 3);
	}

	SECTION("overwriting a value") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(1, 2, 4);

		CHECK(map.get(1, 2) == 4);
		CHECK(map.get_first_inverted(4) == 1);
		CHECK(map.get_second_inverted(4) == 2);
	}

	SECTION("insert_if_absent() keeps the value of an existing pair") {
		TwoDimensionalMap<unsigned> map(10, 10);

		CHECK(map.insert_if_absent(1, 2, 5) == std::pair(5U, true));
		CHECK(map.insert_if_absent(1, 3, 6) == std::pair(6U, true));
		// The pair exists: the new value is refused and the stored one returned.
		CHECK(map.insert_if_absent(1, 2, 7) == std::pair(5U, false));
		CHECK(map.get(1, 2) == 5);
		CHECK(map.get(1, 3) == 6);
		CHECK(map.get(1, 4) == std::numeric_limits<unsigned>::max());
		// Only the inserted values have inverted entries.
		CHECK(map.get_first_inverted(5) == 1);
		CHECK(map.get_first_inverted(6) == 1);
	}
}

TEST_CASE("mata::utils::TwoDimensionalMap::large_map == true") {
	SECTION("mata::utils::TwoDimensionalMap::insert() && get()") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get(1, 2) == 3);
		CHECK(map.get(4, 5) == 6);
		CHECK(map.get(1, 5) == 7);
		CHECK(map.get(4, 2) == 8);
		CHECK(map.get(0, 0) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(5, 5) == std::numeric_limits<unsigned>::max());
	}

	SECTION("mata::utils::TwoDimensionalMap::get_first_inverted()") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get_first_inverted(3) == 1);
		CHECK(map.get_first_inverted(6) == 4);
		CHECK(map.get_first_inverted(7) == 1);
		CHECK(map.get_first_inverted(8) == 4);
	}

	SECTION("mata::utils::TwoDimensionalMap::get_second_inverted()") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get_second_inverted(3) == 2);
		CHECK(map.get_second_inverted(6) == 5);
		CHECK(map.get_second_inverted(7) == 5);
		CHECK(map.get_second_inverted(8) == 2);
	}

	SECTION("big") {
		TwoDimensionalMap<unsigned, true, 1'000> map(1'000, 1'000);
		for (unsigned i = 0; i < 1'000; ++i) {
			for (unsigned j = 0; j < 1'000; ++j) { map.insert(i, j, i + j); }
		}

		for (unsigned i = 0; i < 1'000; ++i) {
			for (unsigned j = 0; j < 1'000; ++j) { CHECK(map.get(i, j) == i + j); }
		}
	}

	SECTION("zero is a regular value") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(3, 4, 0);

		CHECK(map.get(3, 4) == 0);
		CHECK(map.get(3, 5) == std::numeric_limits<unsigned>::max());
		CHECK(map.get_first_inverted(0) == 3);
		CHECK(map.get_second_inverted(0) == 4);
	}

	SECTION("inserting a smaller value keeps the larger inverted entries") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(1, 2, 5);
		map.insert(3, 4, 3);

		CHECK(map.get_first_inverted(5) == 1);
		CHECK(map.get_second_inverted(5) == 2);
		CHECK(map.get_first_inverted(3) == 3);
		CHECK(map.get_second_inverted(3) == 4);
		CHECK(map.get(1, 2) == 5);
		CHECK(map.get(3, 4) == 3);
	}

	SECTION("missing keys") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		CHECK(map.get(0, 0) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(9, 9) == std::numeric_limits<unsigned>::max());

		map.insert(1, 2, 3);
		// The swapped pair and the neighbours of the stored pair in both dimensions are distinct keys.
		CHECK(map.get(2, 1) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(1, 1) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(1, 3) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(0, 2) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(2, 2) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(1, 2) == 3);
	}

	SECTION("overwriting a value") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(1, 2, 4);

		CHECK(map.get(1, 2) == 4);
		CHECK(map.get_first_inverted(4) == 1);
		CHECK(map.get_second_inverted(4) == 2);
	}

	SECTION("insert_if_absent() keeps the value of an existing pair") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);

		CHECK(map.insert_if_absent(1, 2, 5) == std::pair(5U, true));
		CHECK(map.insert_if_absent(1, 3, 6) == std::pair(6U, true));
		// The pair exists: the new value is refused and the stored one returned, also after the table grew.
		CHECK(map.insert_if_absent(1, 2, 7) == std::pair(5U, false));
		for (unsigned i = 0; i < 10; ++i) { map.insert_if_absent(i, i, 100 + i); }
		CHECK(map.insert_if_absent(1, 2, 8) == std::pair(5U, false));
		CHECK(map.get(1, 2) == 5);
		CHECK(map.get(1, 3) == 6);
		CHECK(map.get(9, 9) == 109);
		CHECK(map.get(1, 4) == std::numeric_limits<unsigned>::max());
		CHECK(map.get_first_inverted(5) == 1);
		CHECK(map.get_second_inverted(5) == 2);
	}

	SECTION("non-square dimensions") {
		// Pairs (first, second) and (second, first) must not be mixed up when the dimensions differ.
		TwoDimensionalMap<unsigned, true, 10> wide(3, 1'000);
		TwoDimensionalMap<unsigned, true, 10> tall(1'000, 3);
		for (unsigned i = 0; i < 3; ++i) {
			for (unsigned j = 0; j < 1'000; ++j) {
				wide.insert(i, j, i * 1'000 + j);
				tall.insert(j, i, i * 1'000 + j);
			}
		}

		for (unsigned i = 0; i < 3; ++i) {
			for (unsigned j = 0; j < 1'000; ++j) {
				CHECK(wide.get(i, j) == i * 1'000 + j);
				CHECK(tall.get(j, i) == i * 1'000 + j);
				CHECK(wide.get_first_inverted(i * 1'000 + j) == i);
				CHECK(wide.get_second_inverted(i * 1'000 + j) == j);
				CHECK(tall.get_first_inverted(i * 1'000 + j) == j);
				CHECK(tall.get_second_inverted(i * 1'000 + j) == i);
			}
		}
	}

	SECTION("sparse pairs in huge dimensions") {
		// 2^24 x 2^24 pairs: the packed keys need more than 32 bits, and only a few of them are stored.
		constexpr uint64_t dim_size{uint64_t{1} << 24};
		constexpr uint64_t num_of_pairs{100'000};
		// First keys are even and pairwise distinct, second keys are odd, so no swapped pair is ever stored.
		const auto first_key = [](const uint64_t i) { return (2 * i * 7'919) % dim_size; };
		const auto second_key = [](const uint64_t i) { return (2 * i * 104'729 + 1) % dim_size; };
		TwoDimensionalMap<uint64_t> map(dim_size, dim_size);
		for (uint64_t i = 0; i < num_of_pairs; ++i) { map.insert(first_key(i), second_key(i), i); }

		for (uint64_t i = 0; i < num_of_pairs; ++i) {
			CHECK(map.get(first_key(i), second_key(i)) == i);
			CHECK(map.get_first_inverted(i) == first_key(i));
			CHECK(map.get_second_inverted(i) == second_key(i));
			CHECK(map.get(second_key(i), first_key(i)) == std::numeric_limits<uint64_t>::max());
		}
	}

	SECTION("dimensions at and over the 64-bit limit of the packed keys") {
		using Map = TwoDimensionalMap<uint64_t, false>;
		// (0, 0) and (0, 2) would both pack to zero modulo 2^64.
		CHECK_THROWS_AS(Map(uint64_t{1} << 63, 3), std::length_error);

		// 3 * first_dim_size == 2^64 - 1: the largest packed key is 2^64 - 2, next to the empty-slot marker.
		constexpr uint64_t first_dim_size{std::numeric_limits<uint64_t>::max() / 3};
		Map map(first_dim_size, 3);
		map.insert(0, 0, 1);
		map.insert(0, 2, 2);
		map.insert(first_dim_size - 1, 2, 3);
		map.insert(first_dim_size - 1, 0, 4);
		CHECK(map.get(0, 0) == 1);
		CHECK(map.get(0, 2) == 2);
		CHECK(map.get(first_dim_size - 1, 2) == 3);
		CHECK(map.get(first_dim_size - 1, 0) == 4);
		CHECK(map.get(0, 1) == Map::no_value);
		CHECK(map.get(first_dim_size - 1, 1) == Map::no_value);
	}
}

TEST_CASE("mata::utils::TwoDimensionalMap switches to large mode above MaxMatrixBytes") {
	// 12 cells of 4 Bytes fit into 48 Bytes. A matrix cell cannot hold values above max_value, the large mode stores
	//  any value of T.
	using Map = TwoDimensionalMap<uint64_t, false, 12 * 4>;
	constexpr uint64_t wide_value{uint64_t{1} << 40};
	static_assert(wide_value > Map::max_value);

	const auto check_all_pairs = [](Map& map, const uint64_t first_dim_size, const uint64_t second_dim_size) {
		for (uint64_t i = 0; i < first_dim_size; ++i) {
			for (uint64_t j = 0; j < second_dim_size; ++j) { map.insert(i, j, i * second_dim_size + j); }
		}
		for (uint64_t i = 0; i < first_dim_size; ++i) {
			for (uint64_t j = 0; j < second_dim_size; ++j) { CHECK(map.get(i, j) == i * second_dim_size + j); }
		}
	};

	SECTION("a matrix of exactly MaxMatrixBytes") {
		Map map(3, 4);
		check_all_pairs(map, 3, 4);
	}

	SECTION("one more row than fits into MaxMatrixBytes") {
		Map map(4, 4);
		check_all_pairs(map, 4, 4);
		map.insert(3, 3, wide_value);
		CHECK(map.get(3, 3) == wide_value);
	}

	SECTION("one more column than fits into MaxMatrixBytes") {
		Map map(3, 5);
		check_all_pairs(map, 3, 5);
		map.insert(2, 4, wide_value);
		CHECK(map.get(2, 4) == wide_value);
	}
}

#if UINTPTR_MAX >= UINT64_MAX
TEST_CASE("mata::utils::TwoDimensionalMap sparse rows grow with second keys above 2^32") {
	// The row budget keeps the storage on SparseRows with uint64_t keys: growth, rehashing and missing keys of the
	//  64-bit Fibonacci probing path. Skipped on 32-bit size_t: these dimensions do not fit addressable storage.
	static_assert(sizeof(size_t) >= sizeof(uint64_t));
	using Map = TwoDimensionalMap<uint64_t, false, 512>;
	Map map(20, uint64_t{1} << 63);
	constexpr uint64_t high{(uint64_t{1} << 32) + 7};
	for (uint64_t second = high; second < high + 64; ++second) { map.insert(0, second, second + 1); }
	for (uint64_t first = 0; first < 20; ++first) {
		for (uint64_t second = high; second < high + 64; ++second) {
			map.insert_if_absent(first, second, second + first + 1);
		}
	}
	for (uint64_t first = 0; first < 20; ++first) {
		for (uint64_t second = high; second < high + 64; ++second) {
			const uint64_t expected{second + (first == 0 ? uint64_t{1} : first + 1)};
			CHECK(map.get(first, second) == expected);
			CHECK(map.get(first, second + 1'000) == Map::no_value);
		}
	}
}

TEST_CASE("mata::utils::TwoDimensionalMap sparse rows do not pack their keys") {
	// Three rows fit the row budget, so the map uses SparseRows even though 3 * 2^63 overflows 64 bits: no packed
	//  pair of keys exists, and second keys must stay full-width.
	TwoDimensionalMap<uint64_t, true, 512> map(3, uint64_t{1} << 63);
	constexpr uint64_t second{(uint64_t{1} << 62) + 13};
	map.insert(0, second, 7);
	map.insert(2, second, 8);
	CHECK(map.get(0, second) == 7);
	CHECK(map.get(2, second) == 8);
	CHECK(map.get(1, second) == TwoDimensionalMap<uint64_t, true, 512>::no_value);
	map.insert_if_absent(0, second, 9);
	CHECK(map.get(0, second) == 7);
	CHECK(map.get_first_inverted(7) == 0);
	CHECK(map.get_second_inverted(7) == second);
}
#endif // UINTPTR_MAX >= UINT64_MAX

TEST_CASE("mata::utils::TwoDimensionalMap keeps one hash table per first key") {
	// A matrix would need 20 * 100 cells, but 20 rows of 16 Bytes fit into 512 Bytes, so the map uses sparse rows.
	constexpr uint16_t first_dim_size{20};
	constexpr uint16_t second_dim_size{100};
	constexpr uint16_t last_value{first_dim_size * second_dim_size - 1};
	using Map = TwoDimensionalMap<uint16_t, true, 512>;

	SECTION("growth, collisions, missing keys, and the inverted indices") {
		Map map(first_dim_size, second_dim_size);
		// Only the even second keys of the first row first, so the row grows repeatedly while other rows do not
		// exist yet, and the missing keys of both an allocated and an unallocated row can be checked.
		for (uint16_t second = 0; second < second_dim_size; second += 2) { map.insert(0, second, second); }
		for (uint16_t second = 0; second < second_dim_size; ++second) {
			CHECK(map.get(0, second) == (second % 2 == 0 ? second : Map::no_value));
			CHECK(map.get(1, second) == Map::no_value);
		}

		for (uint16_t first = 0; first < first_dim_size; ++first) {
			for (uint16_t second = 0; second < second_dim_size; ++second) {
				map.insert(first, second, static_cast<uint16_t>(first * second_dim_size + second));
			}
		}
		for (uint16_t first = 0; first < first_dim_size; ++first) {
			for (uint16_t second = 0; second < second_dim_size; ++second) {
				const uint16_t value{static_cast<uint16_t>(first * second_dim_size + second)};
				CHECK(map.get(first, second) == value);
				CHECK(map.get_first_inverted(value) == first);
				CHECK(map.get_second_inverted(value) == second);
			}
		}

		// Overwriting an existing pair moves it to the new value. uint16_t arithmetic promotes to int,
		//  so wrap the value back explicitly before passing it to the typed Map::insert().
		constexpr uint16_t past_last_value{static_cast<uint16_t>(last_value + 1)};
		map.insert(last_value / second_dim_size, second_dim_size - 1, past_last_value);
		CHECK(map.get(last_value / second_dim_size, second_dim_size - 1) == past_last_value);
		CHECK(map.get_first_inverted(past_last_value) == last_value / second_dim_size);
		CHECK(map.get_second_inverted(past_last_value) == second_dim_size - 1);

		// insert_if_absent() refuses the new value of an existing pair and returns the stored one.
		CHECK(map.insert_if_absent(3, 4, 1'234) == std::pair(uint16_t{304}, false));
		CHECK(map.get(3, 4) == 304);
	}

	SECTION("insert_if_absent() inserts a new pair once") {
		Map map(first_dim_size, second_dim_size);
		CHECK(map.insert_if_absent(7, 9, 42) == std::pair(uint16_t{42}, true));
		CHECK(map.get(7, 9) == 42);
		CHECK(map.get_first_inverted(42) == 7);
		CHECK(map.get_second_inverted(42) == 9);
		CHECK(map.insert_if_absent(7, 9, 1'234) == std::pair(uint16_t{42}, false));
		CHECK(map.get(7, 9) == 42);
		CHECK(map.get(7, 8) == Map::no_value);
	}
}
