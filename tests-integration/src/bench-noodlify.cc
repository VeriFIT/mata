/**
 * Benchmark: noodlification of string equations (issue #207)
 *
 * Times `noodlify_for_equation()` (both sides given as sequences of automata, "reduce" set to "forward", as
 *  Z3-Noodler calls it) end to end, from the unification of the side automata to the last noodle.
 *
 * Usage:
 *   bench-noodlify [SEED]
 *     The random 3+2 equation of issue #207: x1 x2 x3 = y1 y2 with Tabakov-Vardi automata over two letters, 20
 *     states for every x and 40 and 20 states for y1 and y2, generated from SEED (default 0).
 *   bench-noodlify LHS.mata... -- RHS.mata...
 *     The equation whose left side is the concatenation of the automata in the files before `--` and whose right
 *     side is the concatenation of the automata in the files after it. All files share one alphabet.
 *
 * Prints the time on stdout and the number of noodles on stderr, so that only the time is a pycobench metric.
 *
 * Inputs: inputs/bench-noodlify-random.input (jobs/bench-noodlify-random.yaml) and
 *  inputs/bench-triple-noodlify-z3-noodler.input (jobs/bench-noodlify-equation.yaml).
 */

#include "utils/utils.hh"

#include "mata/applications/strings.hh"
#include "mata/nfa/builder.hh"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace mata::applications::strings;

namespace {

constexpr size_t alphabet_size{2};
constexpr double transitions_per_state_and_symbol{1.5};
constexpr double final_state_density{0.3};

std::vector<std::shared_ptr<Nfa>> random_side(const std::vector<size_t>& num_of_states, unsigned& seed) {
	std::vector<std::shared_ptr<Nfa>> side;
	for (const size_t states : num_of_states) {
		side.push_back(
			std::make_shared<Nfa>(builder::create_random_nfa_tabakov_vardi(
				states, alphabet_size, transitions_per_state_and_symbol, final_state_density, seed++
			))
		);
	}
	return side;
}

std::vector<std::shared_ptr<Nfa>> to_shared(std::vector<Nfa>&& automata) {
	std::vector<std::shared_ptr<Nfa>> shared;
	shared.reserve(automata.size());
	for (Nfa& aut : automata) { shared.push_back(std::make_shared<Nfa>(std::move(aut))); }
	return shared;
}

int usage(const char* program) {
	std::cerr << "Usage: " << program << " [SEED]\n       " << program << " LHS.mata... -- RHS.mata...\n";
	return EXIT_FAILURE;
}

} // namespace

int main(int argc, char* argv[]) {
	const std::vector<std::string> args(argv + 1, argv + argc);
	std::vector<std::shared_ptr<Nfa>> lhs;
	std::vector<std::shared_ptr<Nfa>> rhs;

	if (const auto separator{std::ranges::find(args, "--")}; separator != args.end()) {
		std::vector<std::string> lhs_files(args.begin(), separator);
		std::vector<std::string> rhs_files(separator + 1, args.end());
		if (lhs_files.empty() || rhs_files.empty()) { return usage(argv[0]); }
		// Load both sides together: separate mintermizations could give one number to different predicates.
		std::vector<std::string> equation_files{lhs_files};
		equation_files.insert(equation_files.end(), rhs_files.begin(), rhs_files.end());
		mata::OnTheFlyAlphabet alphabet;
		std::vector<Nfa> lhs_automata;
		std::vector<Nfa> rhs_automata;
		if (load_automata(lhs_files, lhs_automata, alphabet) != EXIT_SUCCESS ||
			load_automata(rhs_files, rhs_automata, alphabet) != EXIT_SUCCESS) {
			return EXIT_FAILURE;
		}
		lhs = to_shared(std::move(lhs_automata));
		rhs = to_shared(std::move(rhs_automata));
	} else if (args.size() <= 1) {
		unsigned seed{0};
		if (!args.empty()) {
			try {
				seed = static_cast<unsigned>(std::stoul(args[0]));
			} catch (const std::exception&) { return usage(argv[0]); }
		}
		lhs = random_side({20, 20, 20}, seed);
		rhs = random_side({40, 20}, seed);
	} else {
		return usage(argv[0]);
	}

	std::cout << std::fixed << std::setprecision(4);
	TIME_BEGIN(noodlify_for_equation);
	const std::vector<seg_nfa::NoodleWithEpsilonsCounter> noodles{
		seg_nfa::noodlify_for_equation(lhs, rhs, false, {{"reduce", "forward"}})
	};
	TIME_END(noodlify_for_equation);
	std::cerr << "noodles: " << noodles.size() << "\n";

	return EXIT_SUCCESS;
}
