// Times the operations that drive SynchronizedExistentialIterator::advance().
// Usage:
//   probe_c det <file.mata>                 determinize(); prints states;transitions;median_ms
//   probe_c incl <lhs.mata> <rhs.mata>      antichain inclusion both ways; prints r1;ms1;r2;ms2
//   probe_c univ <file.mata>                is_universal_antichains over the used symbols; prints result;median_ms
//   probe_c micro <k> <alphabet> <symbols_per_state> <seed>
//       one macrostate of k states, each with symbols_per_state distinct symbols out of alphabet (targets: 1 each);
//       prints distinct_symbols;us_per_macrostate (push_back + advance to the end)
#include "utils/utils.hh"

#include <algorithm>
#include <random>

using mata::Symbol;

namespace {
double time_ms(const auto& fn, size_t min_reps = 3, double min_total_ms = 100.0) {
	std::vector<double> samples;
	double total{0};
	while (samples.size() < min_reps || (total < min_total_ms && samples.size() < 100'000)) {
		const auto start{std::chrono::steady_clock::now()};
		fn();
		const auto end{std::chrono::steady_clock::now()};
		samples.push_back(std::chrono::duration<double, std::milli>(end - start).count());
		total += samples.back();
	}
	std::ranges::sort(samples);
	return samples[samples.size() / 2];
}

bool load(const std::vector<std::string>& files, std::vector<Nfa>& auts, mata::OnTheFlyAlphabet& alphabet) {
	std::vector<std::string> names{files};
	return load_automata(names, auts, alphabet, true) == EXIT_SUCCESS;
}
} // namespace

int main(int argc, char* argv[]) {
	if (argc < 3) { return 1; }
	const std::string mode{argv[1]};
	std::cout << std::fixed << std::setprecision(4);
	if (mode == "micro") {
		if (argc != 6) { return 1; }
		const size_t k{std::stoul(argv[2])};
		const size_t alphabet_size{std::stoul(argv[3])};
		const size_t per_state{std::stoul(argv[4])};
		if (per_state > alphabet_size) { return 1; }
		std::mt19937 rng{static_cast<unsigned>(std::stoul(argv[5]))};
		Delta delta{};
		std::vector<Symbol> symbols(alphabet_size);
		for (size_t i{0}; i < alphabet_size; ++i) { symbols[i] = static_cast<Symbol>(i); }
		for (State q{0}; q < k; ++q) {
			std::ranges::shuffle(symbols, rng);
			std::vector<Symbol> chosen(symbols.begin(), symbols.begin() + static_cast<long>(per_state));
			std::ranges::sort(chosen);
			for (const Symbol a : chosen) { delta.add(q, a, static_cast<State>((q + a) % k)); }
		}
		SynchronizedExistentialSymbolPostIterator it{};
		size_t distinct{0};
		size_t sink{0};
		const auto pass{[&] {
			it.reset();
			for (State q{0}; q < k; ++q) { mata::utils::push_back(it, delta[q]); }
			size_t count{0};
			while (it.advance()) {
				++count;
				sink += it.get_current().size();
			}
			distinct = count;
		}};
		const double ms{time_ms(pass, 1'000, 300.0)};
		std::cout << distinct << ";" << ms * 1000.0 << "\n";
		return sink == 0 ? 1 : 0;
	}
	if (mode == "incl") {
		if (argc != 4) { return 1; }
		std::vector<Nfa> auts;
		mata::OnTheFlyAlphabet alphabet;
		if (!load({argv[2], argv[3]}, auts, alphabet)) { return 2; }
		bool r1{false};
		bool r2{false};
		const ParameterMap params{{"algorithm", "antichains"}};
		const double ms1{time_ms([&] { r1 = is_included(auts[0], auts[1], &alphabet, params); })};
		const double ms2{time_ms([&] { r2 = is_included(auts[1], auts[0], &alphabet, params); })};
		std::cout << r1 << ";" << ms1 << ";" << r2 << ";" << ms2 << "\n";
		return 0;
	}
	std::vector<Nfa> auts;
	mata::OnTheFlyAlphabet otf;
	if (!load({argv[2]}, auts, otf)) { return 2; }
	const Nfa& aut{auts[0]};
	if (mode == "det") {
		Nfa result;
		const double ms{time_ms([&] { result = determinize(aut); })};
		std::cout << result.num_of_states() << ";" << result.delta.num_of_transitions() << ";" << ms << "\n";
		return 0;
	}
	if (mode == "univ") {
		const mata::EnumAlphabet alphabet{aut.delta.get_used_symbols()};
		bool result{false};
		const double ms{time_ms([&] { result = algorithms::is_universal_antichains(aut, alphabet, nullptr); })};
		std::cout << result << ";" << ms << "\n";
		return 0;
	}
	return 1;
}
