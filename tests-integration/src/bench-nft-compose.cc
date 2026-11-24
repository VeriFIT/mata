/**
 * Benchmark of NFT composition and of noodlify_for_transducer() on seeded, in-memory generated transducers.
 *
 * Composes two random jump-free two-level NFTs on the levels (1, 0) with the default compose() (CompositionMode::Auto
 *  under JumpMode::RepeatSymbol), with JumpMode::NoJump, and with CompositionMode::General, for three kinds of
 *  EPSILON: none, fast EPSILON transitions (from level 0 to level 0), and EPSILON on ordinary transitions. Then
 *  noodlifies a chain of random NFAs through a reluctant replace-all transducer. Each line is `<name>: <seconds>`
 *  followed by the size of the result. The optional argument is the number of states per level of a composed NFT.
 */

#include "utils/utils.hh"

#include "mata/applications/strings.hh"
#include "mata/nft/nft.hh"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

using mata::Symbol;
using mata::nft::Nft;

namespace {

/// Raw std::mt19937 output, so a seed gives the same automata under every standard library.
size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

enum class EpsilonKind : std::uint8_t { None, Fast, Ordinary };

/// A random jump-free NFT with two levels and @p num_of_states_per_level states on each, over the symbols
///  {0, ..., num_of_symbols - 1}; each (state, symbol) gets up to two targets on the next level.
Nft random_nft(
	std::mt19937& rng, const size_t num_of_states_per_level, const size_t num_of_symbols, const EpsilonKind epsilon
) {
	constexpr size_t num_of_levels{2};
	Nft nft{Nft::with_levels(num_of_levels)};
	for (size_t level{0}; level < num_of_levels; ++level) {
		for (size_t index{0}; index < num_of_states_per_level; ++index) {
			nft.add_state_with_level(static_cast<mata::nft::Level>(level));
		}
	}
	// The states of level l are l * num_of_states_per_level + index.
	const auto state_on{[&](const size_t level, const size_t index) {
		return static_cast<State>(level * num_of_states_per_level + index);
	}};
	nft.initial.insert(state_on(0, 0));
	for (size_t index{0}; index < num_of_states_per_level; ++index) {
		if (draw(rng, 3) == 0) { nft.final.insert(state_on(0, index)); }
	}
	for (size_t level{0}; level < num_of_levels; ++level) {
		const size_t next_level{(level + 1) % num_of_levels};
		for (size_t index{0}; index < num_of_states_per_level; ++index) {
			const State source{state_on(level, index)};
			for (Symbol symbol{0}; symbol < num_of_symbols; ++symbol) {
				for (size_t transition{0}, transitions{draw(rng, 3)}; transition < transitions; ++transition) {
					nft.delta.add(source, symbol, state_on(next_level, draw(rng, num_of_states_per_level)));
				}
			}
			if (draw(rng, 10) != 0) { continue; }
			if (epsilon == EpsilonKind::Ordinary) {
				nft.delta.add(source, mata::nft::EPSILON, state_on(next_level, draw(rng, num_of_states_per_level)));
			} else if (epsilon == EpsilonKind::Fast && level == 0) {
				nft.delta.add(source, mata::nft::EPSILON, state_on(0, draw(rng, num_of_states_per_level)));
			}
		}
	}
	return nft;
}

/// A random NFA over @p symbols with @p num_of_states states, its initial state 0 and its last state final.
Nfa random_nfa(std::mt19937& rng, const size_t num_of_states, const std::vector<Symbol>& symbols) {
	Nfa nfa{num_of_states, {0}, {static_cast<State>(num_of_states - 1)}};
	for (State state{0}; state < num_of_states; ++state) {
		for (const Symbol symbol : symbols) {
			for (size_t transition{0}, transitions{draw(rng, 3)}; transition < transitions; ++transition) {
				nfa.delta.add(state, symbol, static_cast<State>(draw(rng, num_of_states)));
			}
		}
	}
	return nfa;
}

/// Runs @p operation, then prints `<name>: <seconds>` and the number of states of its result.
template <typename Operation> void time_operation(const std::string& name, Operation&& operation) {
	const auto start{std::chrono::steady_clock::now()};
	const auto result{operation()};
	const std::chrono::duration<double> elapsed{std::chrono::steady_clock::now() - start};
	std::cout << name << ": " << elapsed.count() << "\n" << name << "-size: " << result << "\n" << std::flush;
}

} // namespace

int main(int argc, char* argv[]) {
	size_t num_of_states_per_level{200};
	if (argc > 2) {
		std::cerr << "Usage: " << argv[0] << " [number of states per level]\n";
		return EXIT_FAILURE;
	}
	if (argc == 2) {
		num_of_states_per_level = std::stoul(argv[1]);
		if (num_of_states_per_level == 0) {
			std::cerr << "The number of states per level must be positive\n";
			return EXIT_FAILURE;
		}
	}

	// Setting precision of the times to fixed points and 4 decimal places
	std::cout << std::fixed << std::setprecision(4);

	using mata::nft::CompositionMode;
	using mata::nft::JumpMode;
	constexpr size_t num_of_symbols{4};
	for (const auto& [epsilon, epsilon_name] :
		 {std::pair{EpsilonKind::None, "plain"}, std::pair{EpsilonKind::Fast, "fast-epsilon"},
		  std::pair{EpsilonKind::Ordinary, "epsilon"}}) {
		std::mt19937 lhs_rng{753};
		std::mt19937 rhs_rng{7'530};
		const Nft lhs{random_nft(lhs_rng, num_of_states_per_level, num_of_symbols, epsilon)};
		const Nft rhs{random_nft(rhs_rng, num_of_states_per_level, num_of_symbols, epsilon)};
		const std::string prefix{std::string{"compose-"} + epsilon_name};
		time_operation(prefix + "-auto", [&] { return mata::nft::compose(lhs, rhs).num_of_states(); });
		time_operation(prefix + "-nojump", [&] {
			return mata::nft::compose(lhs, rhs, 1, 0, true, JumpMode::NoJump).num_of_states();
		});
		time_operation(prefix + "-general", [&] {
			return mata::nft::compose(lhs, rhs, 1, 0, true, JumpMode::RepeatSymbol, CompositionMode::General)
				.num_of_states();
		});
	}

	const std::vector<Symbol> symbols{'a', 'b', 'c', 'd'};
	const mata::EnumAlphabet alphabet{symbols.begin(), symbols.end()};
	const auto replace{std::make_shared<Nft>(mata::applications::strings::replace::replace_reluctant_literal(
		{'a', 'b'}, {'c'}, &alphabet, mata::applications::strings::replace::ReplaceMode::All
	))};
	// Two random input automata and sigma star as the only output automaton: its noodles split nothing but the input.
	std::mt19937 nfa_rng{75'300};
	std::vector<std::shared_ptr<Nfa>> input_automata{};
	for (size_t index{0}; index < 2; ++index) {
		input_automata.push_back(std::make_shared<Nfa>(random_nfa(nfa_rng, num_of_states_per_level / 4 + 1, symbols)));
	}
	Nfa sigma_star{1, {0}, {0}};
	for (const Symbol symbol : symbols) { sigma_star.delta.add(0, symbol, 0); }
	const std::vector<std::shared_ptr<Nfa>> output_automata{std::make_shared<Nfa>(std::move(sigma_star))};
	time_operation("noodlify-for-transducer", [&] {
		return mata::applications::strings::seg_nfa::noodlify_for_transducer(replace, input_automata, output_automata)
			.size();
	});
	return EXIT_SUCCESS;
}
