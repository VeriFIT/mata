/** @file
 * @brief Microbenchmark of the five ways the library reads a transition relation.
 *
 * The Phase 1.5 baseline of the templating plan: `key_arity` 1 (today's delta, structure depth 2)
 *  must not regress when the posts are templated in Phase 2 and the walks generalised in Phase 3.
 *  The end-to-end `bench-*` targets are far too coarse to see an access-pattern change, so this
 *  measures the accessors directly, over synthetic relations of a controlled shape.
 *
 * The grid is not invented: it is centred on `delta-shape-stats` run over
 *  `tests-integration/automata` (66 automata, 18 731 states, 65 321 transitions), which found
 *
 *   - 57.5% of states have exactly **2** posts, 91.4% have <= 4, the maximum is 32;
 *   - 81.0% of posts have exactly **1** target, 95.6% have <= 2, with a thin tail to 128;
 *   - 3.49 transitions per state, and 6.3% of states have no outgoing transitions at all;
 *   - automata are small: median 128 states, maximum 4096 -- all of it cache-resident.
 *
 * So the shapes that matter are tiny, and the fixed cost of *setting up* a walk is amortised over
 *  about three targets. Larger sizes are included as stress points anyway: that corpus is the
 *  committed sample, chosen to be small enough to live in the repository, and the external
 *  nfa-bench corpus is not necessarily like it.
 *
 * Build: Release, and with profiling off, or the numbers mean nothing --
 *   cmake -B build-rel -S . -DCMAKE_BUILD_TYPE=Release -DNO_PROFILING=ON
 *
 * Usage: bench-delta-access [--quick] [--realistic-only]
 * Output: TSV on stdout, so two runs can be diffed mechanically.
 */

#include <sched.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "mata/core/delta.hh"

using mata::Delta;
using mata::State;
using mata::StatePost;
using mata::Symbol;
using mata::SymbolPost;

namespace {

/// Keep the optimiser from deleting a walk whose result is otherwise unused.
[[gnu::always_inline]] inline void escape(void* p) { asm volatile("" : : "g"(p) : "memory"); }

// --- the shapes -------------------------------------------------------------------------------

struct Shape {
	std::string name;
	size_t num_of_states;
	size_t posts_per_source; ///< 0 = draw from the measured distribution.
	size_t targets_per_post; ///< 0 = draw from the measured distribution.
};

/// The measured distributions, as (value, weight) taken from `delta-shape-stats` over
///  `tests-integration/automata`. Used by the "realistic" shape, which is the one that gates.
const std::vector<std::pair<size_t, size_t>> kPostsPerSource{
	{0, 1179}, {1, 1625}, {2, 10772}, {3, 1491},  {4, 2046}, {5, 555}, {6, 186}, {7, 113},
	{8, 323},  {9, 148},  {10, 86},   {11, 64},   {12, 29},  {15, 15}, {16, 57}, {32, 42}
};
const std::vector<std::pair<size_t, size_t>> kTargetsPerPost{
	{1, 39028}, {2, 7034}, {3, 1566}, {4, 258}, {5, 115}, {7, 24}, {8, 24},
	{9, 12},    {10, 12},  {11, 24},  {12, 2},  {16, 3},  {32, 1}, {64, 48}, {128, 36}
};

std::discrete_distribution<size_t> distribution_of(const std::vector<std::pair<size_t, size_t>>& hist) {
	std::vector<double> weights;
	weights.reserve(hist.size());
	for (const auto& [value, weight] : hist) { weights.push_back(static_cast<double>(weight)); }
	return std::discrete_distribution<size_t>(weights.begin(), weights.end());
}

/**
 * @brief Build a relation of the requested shape.
 *
 * Targets are spread over the whole state space rather than kept near the source, so that the walk
 *  does not accidentally measure a best case that real automata do not have. The seed is fixed, so
 *  two runs build byte-identical relations.
 */
Delta build(const Shape& shape) {
	std::mt19937_64 rng{0x5eed};
	auto posts_dist{distribution_of(kPostsPerSource)};
	auto targets_dist{distribution_of(kTargetsPerPost)};
	std::uniform_int_distribution<State> state_dist{0, static_cast<State>(shape.num_of_states - 1)};

	Delta delta{shape.num_of_states};
	for (State source{0}; source < shape.num_of_states; ++source) {
		const size_t posts{
			shape.posts_per_source != 0 ? shape.posts_per_source : kPostsPerSource[posts_dist(rng)].first
		};
		for (size_t p{0}; p < posts; ++p) {
			// Symbols strided, not contiguous: a real alphabet after mintermization is not dense.
			const Symbol symbol{static_cast<Symbol>(p * 3 + 1)};
			const size_t targets{
				shape.targets_per_post != 0 ? shape.targets_per_post : kTargetsPerPost[targets_dist(rng)].first
			};
			for (size_t t{0}; t < targets; ++t) { delta.add(source, symbol, state_dist(rng)); }
		}
	}
	return delta;
}

// --- the access patterns ----------------------------------------------------------------------

/**
 * @brief The hand-written baseline: nested loops over the concrete structure, targets only.
 *
 * What a user writes when they reach past the accessors and walk @c StatePost and @c SymbolPost
 *  themselves. Paired with @c sweep_successors -- same order, same accumulation -- so the ratio
 *  between the two is what @c for_each_successor costs someone who could have written this.
 */
size_t manual_successors(const Delta& delta, const std::vector<State>& order) {
	size_t acc{0};
	for (const State source : order) {
		for (const SymbolPost& symbol_post : delta.state_post(source)) {
			for (const State target : symbol_post.targets) { acc += target; }
		}
	}
	return acc;
}

/// The hand-written baseline carrying the key too. Paired with @c sweep_moves.
size_t manual_moves(const Delta& delta, const std::vector<State>& order) {
	size_t acc{0};
	for (const State source : order) {
		for (const SymbolPost& symbol_post : delta.state_post(source)) {
			for (const State target : symbol_post.targets) { acc += target + symbol_post.symbol; }
		}
	}
	return acc;
}

/// Sweep every state through @c for_each_successor -- the plain "visit all targets" walk.
size_t sweep_successors(const Delta& delta, const std::vector<State>& order) {
	size_t acc{0};
	for (const State source : order) {
		delta.for_each_successor(source, [&](const State target) { acc += target; });
	}
	return acc;
}

/// Sweep every state through @c for_each_move -- the same walk, but carrying the key.
size_t sweep_moves(const Delta& delta, const std::vector<State>& order) {
	size_t acc{0};
	for (const State source : order) {
		delta.for_each_move(source, [&](const Symbol symbol, const State target) { acc += target + symbol; });
	}
	return acc;
}

/**
 * @brief Sweep every state through @c successor_cursor.
 *
 * The one that matters most. Tarjan's SCC walk uses it, it is the only accessor that has to store
 *  and resume a position, and it is what T3.2 rewrites into a `std::array` of iterator pairs. With
 *  ~3.5 targets per state it is also where a fixed per-state setup cost hurts most.
 */
size_t sweep_cursor(const Delta& delta, const std::vector<State>& order) {
	size_t acc{0};
	for (const State source : order) {
		const mata::SuccessorCursor cursor{delta.successor_cursor(source)};
		for (auto it{cursor.begin()}; it != std::default_sentinel; ++it) { acc += *it; }
	}
	return acc;
}

/// Fetch one post at random and walk it. Isolates the outer indexed lookup plus one post.
size_t random_post(const Delta& delta, const std::vector<State>& order) {
	size_t acc{0};
	for (const State source : order) {
		const StatePost& post{delta.state_post(source)};
		for (const SymbolPost& symbol_post : post) {
			acc += symbol_post.symbol;
			for (const State target : symbol_post.targets) { acc += target; }
		}
	}
	return acc;
}

/// Point membership: the binary search over keys, then over targets. No iteration.
size_t point_lookup(const Delta& delta, const std::vector<State>& order) {
	size_t acc{0};
	for (const State source : order) {
		// Symbol 4 is the second post when posts are strided by 3; a hit for most shapes.
		acc += delta.contains(source, 4, order[(source + 1) % order.size()]) ? size_t{1} : size_t{0};
	}
	return acc;
}

// --- timing -----------------------------------------------------------------------------------

using Clock = std::chrono::steady_clock; ///< Not system_clock: that is wall time and not monotonic.

struct Timing {
	double ns_per_state;
	double ns_per_transition;
	double spread; ///< (max - min) / min over the repetitions, as a sanity check on the noise.
};

template <typename F>
Timing measure(F&& walk, const Delta& delta, const std::vector<State>& order, const size_t transitions) {
	// Warm the caches and let the branch predictor settle before anything is recorded.
	for (int i{0}; i < 3; ++i) {
		size_t acc{walk(delta, order)};
		escape(&acc);
	}

	// The iteration count is derived from the shape, NOT from a calibration loop.
	//
	// An auto-scaled count is not reproducible: two runs of the same binary can settle on different
	//  counts, and then they are not measuring the same work. That is what put `point_lookup`'s
	//  cross-run noise at 100% -- one run took twice as long as the other on the same build.
	//  A fixed work target gives the same count every time, by construction.
	constexpr size_t kWorkTarget{20'000'000};
	const size_t unit{std::max(transitions, order.size())};
	const size_t iterations{std::max(size_t{1}, kWorkTarget / std::max(unit, size_t{1}))};

	double best{1e300}, worst{0.0};
	for (int rep{0}; rep < 11; ++rep) {
		const auto start{Clock::now()};
		for (size_t i{0}; i < iterations; ++i) {
			size_t acc{walk(delta, order)};
			escape(&acc);
		}
		const double ns{
			static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count())
			/ static_cast<double>(iterations)
		};
		best = std::min(best, ns);
		worst = std::max(worst, ns);
	}
	const double states{static_cast<double>(order.size())};
	return {
		best / states,
		transitions == 0 ? 0.0 : best / static_cast<double>(transitions),
		(worst - best) / best
	};
}

} // anonymous namespace.

int main(int argc, char* argv[]) {
	// Pin to a single core. Migration mid-measurement moves the numbers by more than any change
	//  this benchmark exists to detect, and it is not reproducible between runs.
	cpu_set_t affinity;
	CPU_ZERO(&affinity);
	CPU_SET(2, &affinity);
	if (sched_setaffinity(0, sizeof(affinity), &affinity) != 0) {
		std::cerr << "warning: could not pin to a core; the numbers will be noisier\n";
	}

	bool quick{false}, realistic_only{false};
	for (int i{1}; i < argc; ++i) {
		const std::string arg{argv[i]};
		if (arg == "--quick") { quick = true; }
		else if (arg == "--realistic-only") { realistic_only = true; }
		else {
			std::cerr << "usage: " << argv[0] << " [--quick] [--realistic-only]\n";
			return 1;
		}
	}

	// 128 is the corpus median, 4096 its maximum; the rest are stress points beyond the sample.
	const std::vector<size_t> sizes{quick ? std::vector<size_t>{128, 4096}
	                                      : std::vector<size_t>{128, 4096, 65536, 262144}};

	std::vector<Shape> shapes;
	for (const size_t n : sizes) {
		// The shape that gates: posts and targets drawn from the measured distributions.
		shapes.push_back({"realistic", n, 0, 0});
		if (realistic_only) { continue; }
		// Uniform cells, to separate the dimensions. Values chosen off the measured percentiles:
		//  2 posts is the mode (57.5%), 4 is the 91st percentile, 32 the corpus maximum, 256 stress.
		for (const size_t posts : {size_t{1}, size_t{2}, size_t{4}, size_t{32}, size_t{256}}) {
			for (const size_t targets : {size_t{1}, size_t{2}, size_t{128}}) {
				if (quick && (posts > 4 || targets > 2)) { continue; }
				// Skip cells whose relation would not fit comfortably in memory.
				if (n * posts * targets > 40'000'000) { continue; }
				shapes.push_back({"uniform", n, posts, targets});
			}
		}
	}

	std::cout << "shape\tstates\tposts\ttargets\ttransitions\ttrans_per_state\tpattern"
				 "\tns_per_state\tns_per_transition\tspread\n"
			  << std::fixed;

	for (const Shape& shape : shapes) {
		const Delta delta{build(shape)};
		const size_t transitions{delta.num_of_transitions()};

		// Sequential order for the sweeps; a fixed shuffle for the random-access patterns, so the
		//  outer index is actually exercised rather than prefetched.
		std::vector<State> sequential(shape.num_of_states);
		for (State q{0}; q < shape.num_of_states; ++q) { sequential[q] = q; }
		std::vector<State> shuffled{sequential};
		std::mt19937_64 rng{0xf00d};
		std::ranges::shuffle(shuffled, rng);

		const struct {
			const char* name;
			size_t (*walk)(const Delta&, const std::vector<State>&);
			const std::vector<State>* order;
		} patterns[]{
			{"manual_successors", manual_successors, &sequential},
			{"manual_moves", manual_moves, &sequential},
			{"sweep_successors", sweep_successors, &sequential},
			{"sweep_moves", sweep_moves, &sequential},
			{"sweep_cursor", sweep_cursor, &sequential},
			{"random_post", random_post, &shuffled},
			{"point_lookup", point_lookup, &shuffled},
		};

		for (const auto& pattern : patterns) {
			const Timing t{measure(pattern.walk, delta, *pattern.order, transitions)};
			std::cout << shape.name << "\t" << shape.num_of_states << "\t" << shape.posts_per_source << "\t"
					  << shape.targets_per_post << "\t" << transitions << "\t" << std::setprecision(2)
					  << static_cast<double>(transitions) / static_cast<double>(shape.num_of_states) << "\t"
					  << pattern.name << "\t" << std::setprecision(3) << t.ns_per_state << "\t"
					  << t.ns_per_transition << "\t" << std::setprecision(3) << t.spread << "\n"
					  << std::flush;
		}
	}
	return 0;
}
