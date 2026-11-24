#include "mata/nfa/nfa.hh"
#include "mata/nfa/plumbing.hh"
#include "mata/nft/nft.hh"
#include "mata/nft/plumbing.hh"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {
bool tracking{false};
std::uint64_t allocations{0};
std::uint64_t allocated_bytes{0};
} // namespace
void* operator new(const std::size_t size) {
	if (tracking) {
		++allocations;
		allocated_bytes += size;
	}
	if (void* memory{std::malloc(size)}; memory != nullptr) { return memory; }
	throw std::bad_alloc{};
}
void* operator new[](const std::size_t size) {
	if (tracking) {
		++allocations;
		allocated_bytes += size;
	}
	if (void* memory{std::malloc(size)}; memory != nullptr) { return memory; }
	throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
using mata::nfa::Nfa;
using mata::nfa::State;
using mata::nfa::StateRenaming;
using mata::nft::Nft;

Nfa make_nfa() {
	constexpr std::size_t components{2};
	constexpr std::size_t states_per_component{50};
	Nfa aut{components * states_per_component, {0}, {45, 95}};
	for (std::size_t component{0}; component < components; ++component) {
		for (State offset{0}; offset < states_per_component; ++offset) {
			const State source{static_cast<State>(component * states_per_component + offset)};
			const State next{
				static_cast<State>(component * states_per_component + (offset + 1) % states_per_component)
			};
			const State previous{
				static_cast<State>(component * states_per_component + (offset + 49) % states_per_component)
			};
			aut.delta.add(source, 0, next);
			aut.delta.add(source, 1, previous);
		}
	}
	return aut;
}

Nft make_nft(const Nfa& nfa) {
	Nft aut{Nft::with_levels(1, nfa.num_of_states(), nfa.initial, nfa.final)};
	aut.delta = nfa.delta;
	return aut;
}

void print_renaming(const StateRenaming& renaming) {
	for (State state{0}; state < 100; ++state) {
		if (const auto it{renaming.find(state)}; it != renaming.end()) {
			std::cout << state << ">" << it->second << " ";
		}
	}
}

struct Measurement {
	std::uint64_t allocations;
	std::uint64_t bytes;
};

template <typename Result, typename Reduce>
Measurement measure(Result& result, StateRenaming& renaming, const Reduce& reduce) {
	allocations = 0;
	allocated_bytes = 0;
	tracking = true;
	result = reduce();
	tracking = false;
	return {allocations, allocated_bytes};
}

bool verify_and_report() {
	const Nfa source{make_nfa()};
	const Nft nft_source{make_nft(source)};
	Nfa nfa_result{};
	Nft nft_result{};
	StateRenaming nfa_map{{999, 999}, {998, 998}};
	StateRenaming nft_map{{999, 999}, {998, 998}};
	const Measurement before_nfa{measure(nfa_result, nfa_map, [&] { return mata::nfa::reduce(source, &nfa_map); })};
	const Measurement before_nft{measure(nft_result, nft_map, [&] { return mata::nft::reduce(nft_source, &nft_map); })};
	const bool valid{
		nfa_map.size() == 100 && nft_map.size() == 100 && nfa_map.find(999) == nfa_map.end() &&
		nft_map.find(999) == nft_map.end() && nfa_map == nft_map && nfa_result.delta == nft_result.delta &&
		nfa_result.initial == nft_result.initial && nfa_result.final == nft_result.final && !nfa_result.is_lang_empty()
	};
	if (!valid) { return false; }
	std::cout << "nfa_states=" << nfa_result.num_of_states() << " nfa_count=" << nfa_map.size()
			  << " nft_count=" << nft_map.size() << " nfa_alloc=" << before_nfa.allocations
			  << " nfa_bytes=" << before_nfa.bytes << " nft_alloc=" << before_nft.allocations
			  << " nft_bytes=" << before_nft.bytes << "\n";
	std::cout << "map=";
	print_renaming(nfa_map);
	std::cout << "\n";
	return true;
}

void benchmark_nft() {
	const Nft source{make_nft(make_nfa())};
	constexpr int iterations{2'000};
	std::size_t checksum{0};
	const auto start{std::chrono::steady_clock::now()};
	for (int iteration{0}; iteration < iterations; ++iteration) {
		StateRenaming renaming{};
		const Nft result{reduce(source, &renaming)};
		checksum += result.num_of_states() + renaming.at(99);
	}
	const auto elapsed{std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start)};
	std::cout << "nft_reduce ns=" << elapsed.count() << " checksum=" << checksum << "\n";
}
} // namespace

int main(const int argc, char** argv) {
	if (argc == 2 && std::string_view{argv[1]} == "bench") {
		benchmark_nft();
		return 0;
	}
	return verify_and_report() ? 0 : 1;
}
