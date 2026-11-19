// Times algorithms::is_universal_antichains (and checks it against the naive algorithm when asked).
// Usage:
//   probe_univ files <check:0|1> <list-file>       one .mata path per line; alphabet = symbols used by the automaton
//   probe_univ files-holds <check> <list-file>      same, but on union_nondet(aut, complement(aut)), which is universal
//   probe_univ random <check> <count> <seed>        Tabakov-Vardi NFAs, n = 40..100, |Sigma| = 2, ratio 1.25..3
// Output: one line per instance: name;result;median_ms;reps;cex_len
#include "utils/utils.hh"

#include "mata/nfa/builder.hh"

#include <algorithm>
#include <random>

using mata::Symbol;

namespace {
double time_ms(const auto& fn, size_t& reps) {
	std::vector<double> samples;
	double total{0};
	while (samples.size() < 3 || (total < 50.0 && samples.size() < 200)) {
		const auto start{std::chrono::steady_clock::now()};
		fn();
		const auto end{std::chrono::steady_clock::now()};
		samples.push_back(std::chrono::duration<double, std::milli>(end - start).count());
		total += samples.back();
	}
	reps = samples.size();
	std::ranges::sort(samples);
	return samples[samples.size() / 2];
}

void run(const std::string& name, const Nfa& aut, const mata::Alphabet& alphabet, const bool check) {
	Run cex{};
	const bool result{algorithms::is_universal_antichains(aut, alphabet, &cex)};
	if (check) {
		const bool naive{algorithms::is_universal_naive(aut, alphabet, nullptr)};
		if (naive != result) {
			std::cerr << "MISMATCH " << name << "\n";
			std::exit(2);
		}
		if (!result && aut.is_in_lang(cex.word)) {
			std::cerr << "BAD CEX " << name << "\n";
			std::exit(3);
		}
	}
	size_t reps{0};
	const double ms{time_ms([&] { (void) algorithms::is_universal_antichains(aut, alphabet, nullptr); }, reps)};
	std::cout << name << ";" << result << ";" << ms << ";" << reps << ";" << (result ? 0 : cex.word.size()) << "\n";
}
} // namespace

int main(int argc, char* argv[]) {
	if (argc < 4) { return 1; }
	const std::string mode{argv[1]};
	const bool check{std::string{argv[2]} == "1"};
	std::cout << std::fixed << std::setprecision(4);
	if (mode == "random") {
		const size_t count{std::stoul(argv[3])};
		std::mt19937 rng{static_cast<unsigned>(std::stoul(argv[4]))};
		for (size_t i{0}; i < count; ++i) {
			const size_t n{std::uniform_int_distribution<size_t>{40, 100}(rng)};
			const double ratio{std::uniform_real_distribution<double>{1.25, 3.0}(rng)};
			const unsigned seed{static_cast<unsigned>(rng())};
			const Nfa aut{builder::create_random_nfa_tabakov_vardi(n, 2, ratio, 0.5, seed)};
			const mata::EnumAlphabet alphabet{mata::utils::OrdVector<Symbol>{0, 1}};
			run("random-" + std::to_string(i) + "-n" + std::to_string(n), aut, alphabet, check);
		}
		return 0;
	}
	std::ifstream list{argv[3]};
	for (std::string path; std::getline(list, path);) {
		if (path.empty()) { continue; }
		Nfa aut;
		mata::OnTheFlyAlphabet otf;
		if (load_automaton(path, aut, otf, true) != EXIT_SUCCESS) {
			std::cout << path << ";ERR;0;0;0\n";
			continue;
		}
		const mata::EnumAlphabet alphabet{aut.delta.get_used_symbols()};
		if (mode == "files-holds") { aut = union_nondet(aut, complement(aut, alphabet)); }
		run(path, aut, alphabet, check);
	}
	return 0;
}
