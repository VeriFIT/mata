/** @file
 * @brief What a CSR post layout costs on the write side.
 *
 * The read sweep (`bench-delta-matrix`) shows a compressed-sparse-row layout walking ~2.5x faster
 *  than hand-written loops over today's per-symbol vectors. That is only half an answer: mata's
 *  algorithms *build* relations incrementally, one transition at a time, and CSR is exactly the
 *  layout that makes an insert expensive — a target lands in the middle of the state's single
 *  contiguous block, so the tail shifts and every later symbol offset moves.
 *
 * This measures the cost of construction, per transition inserted, three ways:
 *
 *   delta_incremental   Delta::add(q, symbol, target), one target at a time. What the algorithms do.
 *   csr_incremental     the same, into a CSR post: shift the tail, bump the offsets.
 *   csr_bulk            CSR built from already-grouped input, append only. The best case, and what
 *                       a defragment or a product construction could actually use.
 *
 * Usage: bench-delta-build
 */

#include <sched.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "mata/core/delta.hh"

using mata::Delta;
using mata::State;
using mata::StateSet;
using mata::Symbol;

namespace {

[[gnu::always_inline]] inline void escape(void* p) { asm volatile("" : : "g"(p) : "memory"); }
using Clock = std::chrono::steady_clock;

struct CsrPost {
	std::vector<Symbol> symbols{};
	std::vector<uint32_t> offsets = {0}; ///< always symbols.size() + 1 entries
	std::vector<State> targets{};

	/// Insert one target, keeping symbols sorted and targets grouped and sorted within a symbol.
	void add(const Symbol symbol, const State target) {
		auto sit{std::ranges::lower_bound(symbols, symbol)};
		const size_t i{static_cast<size_t>(sit - symbols.begin())};
		if (sit == symbols.end() || *sit != symbol) {
			symbols.insert(sit, symbol);
			const uint32_t boundary{offsets[i]}; // copy: insert() may reallocate under us
			offsets.insert(offsets.begin() + static_cast<long>(i) + 1, boundary);
		}
		const auto lo{targets.begin() + offsets[i]}, hi{targets.begin() + offsets[i + 1]};
		const auto tit{std::lower_bound(lo, hi, target)};
		if (tit != hi && *tit == target) { return; } // already present
		targets.insert(tit, target);
		for (size_t k{i + 1}; k < offsets.size(); ++k) { ++offsets[k]; }
	}
};

/// The transitions to insert, in the order an algorithm would produce them: by source, then by
///  symbol, then arbitrary targets. Fixed seed.
struct Plan {
	State num_of_states;
	size_t symbols, targets;
	std::vector<std::tuple<State, Symbol, State>> items;
};

Plan make_plan(const State n, const size_t symbols, const size_t targets) {
	std::mt19937_64 rng{0x5eed};
	std::uniform_int_distribution<State> pick{0, n - 1};
	Plan p{n, symbols, targets, {}};
	p.items.reserve(static_cast<size_t>(n) * symbols * targets);
	for (State q{0}; q < n; ++q) {
		for (size_t s{0}; s < symbols; ++s) {
			for (size_t t{0}; t < targets; ++t) {
				p.items.emplace_back(q, static_cast<Symbol>(s * 3 + 1), pick(rng));
			}
		}
	}
	return p;
}

size_t build_delta_incremental(const Plan& p) {
	Delta d{p.num_of_states};
	for (const auto& [q, s, t] : p.items) { d.add(q, s, t); }
	return d.num_of_transitions();
}

size_t build_csr_incremental(const Plan& p) {
	std::vector<CsrPost> csr(p.num_of_states);
	for (const auto& [q, s, t] : p.items) { csr[q].add(s, t); }
	size_t n{0};
	for (const auto& c : csr) { n += c.targets.size(); }
	return n;
}

/// Grouped input, append only: no shifting, no offset fixups.
size_t build_csr_bulk(const Plan& p) {
	std::vector<CsrPost> csr(p.num_of_states);
	std::vector<State> run;
	size_t i{0};
	while (i < p.items.size()) {
		const State q{std::get<0>(p.items[i])};
		const Symbol s{std::get<1>(p.items[i])};
		run.clear();
		while (i < p.items.size() && std::get<0>(p.items[i]) == q && std::get<1>(p.items[i]) == s) {
			run.push_back(std::get<2>(p.items[i]));
			++i;
		}
		std::ranges::sort(run);
		run.erase(std::ranges::unique(run).begin(), run.end());
		CsrPost& c{csr[q]};
		c.symbols.push_back(s);
		c.targets.insert(c.targets.end(), run.begin(), run.end());
		c.offsets.push_back(static_cast<uint32_t>(c.targets.size()));
	}
	size_t n{0};
	for (const auto& c : csr) { n += c.targets.size(); }
	return n;
}

template <typename F> double time_build(F&& run, const size_t transitions) {
	double best{1e300};
	for (int rep{0}; rep < 5; ++rep) {
		const auto start{Clock::now()};
		size_t acc{run()};
		escape(&acc);
		const double ns{
			static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count())
		};
		best = std::min(best, ns);
	}
	return best / static_cast<double>(transitions);
}

} // anonymous namespace.

int main() {
	cpu_set_t affinity;
	CPU_ZERO(&affinity);
	CPU_SET(2, &affinity);
	if (sched_setaffinity(0, sizeof(affinity), &affinity) != 0) {
		std::cerr << "warning: could not pin\n";
	}

	std::cout << "states\tsymbols\ttargets\ttransitions\timpl\tns_per_insert\tvs_delta\n" << std::fixed;
	for (const State n : {size_t{64}, size_t{4096}, size_t{32768}}) {
		for (const size_t sym : {size_t{1}, size_t{2}, size_t{4}, size_t{16}, size_t{64}}) {
			for (const size_t tgt : {size_t{1}, size_t{2}, size_t{8}, size_t{64}}) {
				if (static_cast<size_t>(n) * sym * tgt > 2'000'000) { continue; }
				const Plan p{make_plan(n, sym, tgt)};
				const size_t inserted{p.items.size()};
				// Sanity: all three must agree on how many distinct transitions resulted.
				const size_t a{build_delta_incremental(p)}, b{build_csr_incremental(p)},
					c{build_csr_bulk(p)};
				if (a != b || a != c) {
					std::cerr << "MISMATCH " << n << "/" << sym << "/" << tgt << ": " << a << " " << b << " "
							  << c << "\n";
					return 1;
				}
				const double d{time_build([&] { return build_delta_incremental(p); }, inserted)};
				const double ci{time_build([&] { return build_csr_incremental(p); }, inserted)};
				const double cb{time_build([&] { return build_csr_bulk(p); }, inserted)};
				const auto row = [&](const char* impl, const double v) {
					std::cout << n << "\t" << sym << "\t" << tgt << "\t" << inserted << "\t" << impl << "\t"
							  << std::setprecision(2) << v << "\t" << std::setprecision(3) << v / d << "\n";
				};
				row("delta_incremental", d);
				row("csr_incremental", ci);
				row("csr_bulk", cb);
				std::cout << std::flush;
			}
		}
	}
	return 0;
}
