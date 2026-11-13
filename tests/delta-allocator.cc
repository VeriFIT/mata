/* delta-allocator.cc -- tests of the allocator of the heap blocks of Delta
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "mata/nfa/nfa.hh"
#include "mata/utils/delta-allocator.hh"

using mata::utils::delta_allocate;
using mata::utils::delta_allocator_name;
using mata::utils::delta_deallocate;

TEST_CASE("mata::utils::DeltaAllocator follows MATA_DELTA_ALLOCATOR", "[delta-allocator]") {
	const std::string_view name{delta_allocator_name()};
	const bool known{name == "std" || name == "pool"};
	CHECK(known);
	// NOLINTNEXTLINE(concurrency-mt-unsafe): the tests never modify the environment.
	if (const char* const requested{std::getenv("MATA_DELTA_ALLOCATOR")}; requested != nullptr && *requested != '\0') {
		CHECK(name == requested);
	}
}

TEST_CASE("mata::utils::DeltaAllocator returns aligned, disjoint, writable blocks", "[delta-allocator]") {
	// Sizes around the boundaries of the pool's size classes, its small-block limit and its chunk.
	const std::vector<std::size_t> sizes{
		1, 15, 16, 17, 32, 48, 4'095, 4'096, 4'097, 65'536, (std::size_t{1} << 20) + 1
	};
	std::vector<std::pair<std::byte*, std::size_t>> blocks{};
	for (int round{0}; round < 64; ++round) {
		for (const std::size_t size : sizes) {
			auto* const block{static_cast<std::byte*>(delta_allocate(size))};
			REQUIRE(block != nullptr);
			CHECK(reinterpret_cast<std::uintptr_t>(block) % 16 == 0);
			std::memset(block, static_cast<int>(blocks.size() & 0xFF), size);
			blocks.emplace_back(block, size);
		}
	}
	std::vector<std::pair<std::byte*, std::size_t>> sorted{blocks};
	std::ranges::sort(sorted);
	for (std::size_t i{1}; i < sorted.size(); ++i) {
		const bool disjoint{sorted[i - 1].first + sorted[i - 1].second <= sorted[i].first};
		CHECK(disjoint);
	}
	for (std::size_t i{0}; i < blocks.size(); ++i) {
		const auto [block, size]{blocks[i]};
		const auto expected{static_cast<std::byte>(i & 0xFF)};
		CHECK(std::all_of(block, block + size, [expected](const std::byte b) { return b == expected; }));
		delta_deallocate(block, size);
	}
}

TEST_CASE("mata::utils::DeltaAllocator accepts blocks released on another thread", "[delta-allocator]") {
	std::vector<std::pair<void*, std::size_t>> blocks{};
	std::thread producer{[&blocks] {
		for (std::size_t size{1}; size <= 8'192; size += 37) { blocks.emplace_back(delta_allocate(size), size); }
	}};
	producer.join();
	for (const auto [block, size] : blocks) { delta_deallocate(block, size); }

	// The released blocks are reusable: automata built afterwards on this thread stay correct.
	mata::nfa::Nfa nfa{3, {0}, {2}};
	for (mata::Symbol symbol{0}; symbol < 64; ++symbol) {
		nfa.delta.add(0, symbol, 1);
		nfa.delta.add(0, symbol, 2);
		nfa.delta.add(1, symbol, 2);
	}
	CHECK(nfa.delta.num_of_transitions() == 192);
	CHECK(nfa.is_in_lang({0}));
	CHECK(nfa.is_in_lang({63, 63}));
}
