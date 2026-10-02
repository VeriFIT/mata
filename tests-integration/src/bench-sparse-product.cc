/**
 * Cold-process benchmark of a sparse product.
 *
 * Intersects two chain automata over disjoint alphabets, so the reachable product has a single state while
 * both input automata are large. The running time and the maximum RSS of the whole process therefore measure
 * the fixed cost of setting up the product, not the cost of the product itself. Run one process per
 * measurement, e.g. '/usr/bin/time -v ./bench-sparse-product 5000'.
 */

#include "utils/utils.hh"

namespace {

/// Build a chain 0 -@p symbol-> 1 -@p symbol-> ... -> (@p num_of_states - 1), with the last state final.
Nfa build_chain(const size_t num_of_states, const mata::Symbol symbol) {
	Nfa chain{num_of_states};
	chain.initial.insert(0);
	for (State source{0}; source + 1 < num_of_states; ++source) { chain.delta.add(source, symbol, source + 1); }
	chain.final.insert(num_of_states - 1);
	return chain;
}

} // namespace

int main(int argc, char* argv[]) {
	size_t num_of_states{5'000};
	if (argc > 2) {
		std::cerr << "Usage: " << argv[0] << " [number of states per automaton]\n";
		return EXIT_FAILURE;
	}
	if (argc == 2) {
		num_of_states = std::stoul(argv[1]);
		if (num_of_states == 0) {
			std::cerr << "The number of states must be positive\n";
			return EXIT_FAILURE;
		}
	}

	// Setting precision of the times to fixed points and 4 decimal places
	std::cout << std::fixed << std::setprecision(4);

	const Nfa lhs{build_chain(num_of_states, 0)};
	const Nfa rhs{build_chain(num_of_states, 1)};

	TIME_BEGIN(intersection);
	const Nfa product{mata::nfa::intersection(lhs, rhs)};
	TIME_END(intersection);

	std::cout << "input states: " << num_of_states << "\nproduct states: " << product.num_of_states() << "\n";
	return EXIT_SUCCESS;
}
