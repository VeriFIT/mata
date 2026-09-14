/** @file
 * @brief Report the shape of the transition relation across a corpus of automata.
 *
 * Phase 1.5 of the templating plan needs a parameter grid for the delta-access microbenchmark, and
 *  inventing one risks measuring shapes that never occur. This reports the shapes that actually do:
 *  for every automaton given, the number of states, and the distributions of *posts per source*
 *  (how many distinct symbols leave a state) and *targets per post* (how many states one symbol
 *  leads to). Both are collected corpus-wide as histograms, since averaging per-file summaries
 *  would be meaningless.
 *
 * The inputs are `@NFA-bits` files whose symbols are BDD formulas, so they are mintermized on load.
 *  The post shapes only exist after that.
 *
 * Usage: delta-shape-stats <file.mata>...
 */

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "utils/utils.hh"

namespace {

/// Exact counts up to 16, then powers of two, so the tail does not need thousands of buckets.
size_t bucket_of(const size_t n) {
	if (n <= 16) { return n; }
	size_t b{32};
	while (b < n) { b *= 2; }
	return b;
}

using Histogram = std::map<size_t, size_t>;

/// @return The value at @p q of the distribution @p hist (0.0 = min, 0.5 = median, 1.0 = max).
size_t quantile(const Histogram& hist, const double q) {
	const size_t total{std::accumulate(hist.begin(), hist.end(), size_t{0}, [](size_t a, const auto& kv) {
		return a + kv.second;
	})};
	if (total == 0) { return 0; }
	const size_t target{static_cast<size_t>(q * static_cast<double>(total - 1))};
	size_t seen{0};
	for (const auto& [value, count] : hist) {
		seen += count;
		if (seen > target) { return value; }
	}
	return hist.rbegin()->first;
}

double mean_of(const Histogram& hist) {
	size_t total{0}, sum{0};
	for (const auto& [value, count] : hist) {
		total += count;
		sum += value * count;
	}
	return total == 0 ? 0.0 : static_cast<double>(sum) / static_cast<double>(total);
}

void report(const std::string& name, const Histogram& hist) {
	std::cout << name << "\tmin=" << quantile(hist, 0.0) << "\tp50=" << quantile(hist, 0.5)
			  << "\tp90=" << quantile(hist, 0.9) << "\tp99=" << quantile(hist, 0.99)
			  << "\tmax=" << quantile(hist, 1.0) << "\tmean=" << mean_of(hist) << "\n";
	std::cout << name << "-histogram";
	for (const auto& [value, count] : hist) { std::cout << "\t" << value << ":" << count; }
	std::cout << "\n";
}

} // anonymous namespace.

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "usage: " << argv[0] << " <file.mata>...\n";
		return 1;
	}

	// Corpus-wide, bucketed. Sources with no outgoing transitions are counted too: an empty post is
	//  a real shape, and how many of them there are decides whether a sparse outer index would pay.
	Histogram posts_per_source{}, targets_per_post{}, states_per_automaton{};
	size_t files_loaded{0}, files_failed{0};

	for (int i{1}; i < argc; ++i) {
		const std::string filename{argv[i]};
		Nfa aut{};
		mata::OnTheFlyAlphabet alphabet{};
		if (load_automaton(filename, aut, alphabet, true) != 0) {
			std::cerr << "skipped (load failed): " << filename << "\n";
			++files_failed;
			continue;
		}
		++files_loaded;

		const size_t num_of_states{aut.num_of_states()};
		states_per_automaton[bucket_of(num_of_states)] += 1;

		size_t transitions{0}, empty_posts{0};
		for (mata::State source{0}; source < num_of_states; ++source) {
			const StatePost& post{aut.delta.state_post(source)};
			posts_per_source[bucket_of(post.size())] += 1;
			if (post.empty()) { ++empty_posts; }
			for (const SymbolPost& symbol_post : post) {
				targets_per_post[bucket_of(symbol_post.targets.size())] += 1;
				transitions += symbol_post.targets.size();
			}
		}

		std::cout << "file\t" << filename << "\tstates=" << num_of_states << "\ttransitions=" << transitions
				  << "\tempty_posts=" << empty_posts << "\n";
	}

	std::cout << "\n# corpus: " << files_loaded << " loaded, " << files_failed << " failed\n";
	report("states", states_per_automaton);
	report("posts_per_source", posts_per_source);
	report("targets_per_post", targets_per_post);
	return 0;
}
