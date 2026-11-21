#include "mata/nfa/nfa.hh"
#include "mata/nft/nft.hh"
#include <chrono>
#include <cstdio>
#include <string>
using namespace mata;
using namespace mata::nfa;

static double elapsed(std::chrono::steady_clock::time_point start) {
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

int main(const int argc, char** argv) {
	const std::string mode{argc > 1 ? argv[1] : "projection"};
	constexpr State n{1'000};
	auto start{std::chrono::steady_clock::now()};
	size_t result{0};
	if (mode == "projection") {
		for (int round = 0; round < 10; ++round) {
			auto nft{mata::nft::Nft::with_levels(2)};
			for (State q{0}; q < 2 * n; ++q) { nft.add_state_with_level(q < n ? 0 : 1); }
			nft.initial.insert(0);
			nft.final.insert(n - 1);
			for (State q{0}; q < n - 1; ++q) {
				nft.delta.add(q, 0, n + q);
				nft.delta.add(n + q, nfa::EPSILON, q + 1);
			}
			const auto projected{mata::nft::project_out(nft, {1}, mata::nft::JumpMode::RepeatSymbol)};
			result += projected.num_of_states() + projected.delta.num_of_transitions();
		}
	} else {
		for (int round = 0; round < 10; ++round) {
			Nfa nfa{n, {0}, {n - 1}};
			for (State q{0}; q < n - 1; ++q) {
				nfa.delta.add(q, nfa::EPSILON, q + 1);
				if (q % 2 == 0) { nfa.delta.add(q, 0, q + 1); }
			}
			nfa.remove_epsilon();
			result += nfa.num_of_states() + nfa.delta.num_of_transitions();
		}
	}
	std::printf("%s %.3f %zu\n", mode.c_str(), elapsed(start), result);
}
