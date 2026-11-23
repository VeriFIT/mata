// Measurement harness of EXP-4: runs the residual-after reduction once and prints a hash of the result.
//
// Usage: harness file <path.mata>
//        harness tv <num_of_states> <alphabet_size> <transition_density> <final_state_density> <seed>
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace mata::nfa;

int main(int argc, char** argv) {
	const std::string mode{argc > 1 ? argv[1] : ""};
	Nfa nfa;
	if (mode == "file" && argc == 3) {
		nfa = builder::parse_from_mata(std::filesystem::path{argv[2]});
	} else if (mode == "tv" && argc == 7) {
		nfa = builder::create_random_nfa_tabakov_vardi(
			std::stoul(argv[2]), std::stoul(argv[3]), std::stod(argv[4]), std::stod(argv[5]),
			static_cast<unsigned>(std::stoul(argv[6]))
		);
	} else {
		std::fprintf(stderr, "usage: %s file <path.mata> | tv <states> <symbols> <ratio> <density> <seed>\n", argv[0]);
		return 2;
	}

	const auto start{std::chrono::steady_clock::now()};
	const Nfa reduced{
		reduce(nfa, nullptr, ParameterMap{{"algorithm", "residual"}, {"type", "after"}, {"direction", "forward"}})
	};
	const auto end{std::chrono::steady_clock::now()};

	// FNV-1a over the number of states, the initial and final states, and every transition.
	unsigned long long hash{1'469'598'103'934'665'603ULL};
	const auto mix = [&](const unsigned long long value) {
		hash ^= value;
		hash *= 1'099'511'628'211ULL;
	};
	mix(reduced.num_of_states());
	for (const State state : reduced.initial) { mix(state + 1'000'003ULL); }
	for (const State state : reduced.final) { mix(state + 2'000'003ULL); }
	size_t num_of_transitions{0};
	for (const Transition& transition : reduced.delta.transitions()) {
		mix(transition.source);
		mix(transition.symbol);
		mix(transition.target);
		++num_of_transitions;
	}
	std::printf(
		"in=%zu out_states=%zu trans=%zu hash=%016llx ms=%.1f\n", nfa.num_of_states(), reduced.num_of_states(),
		num_of_transitions, hash, std::chrono::duration<double, std::milli>(end - start).count()
	);
}
