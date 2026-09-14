/** @file
 * @brief The full shape matrix: every generic accessor against hand-written loops, cell by cell.
 *
 * Structure depth 2 (`key_arity` 1) on the real @c mata::Delta. The baseline in every cell is what a
 *  user can write instead — explicit nested loops over @c StatePost and @c SymbolPost — so each ratio
 *  answers "what does the accessor cost someone who could have written the loops themselves".
 *
 * Grid: 6 state counts x 15 symbols-per-state x 15 targets-per-symbol. Cells whose relation would be
 *  too large are skipped, which is why the matrices thin out towards the bottom right.
 *
 * Build: Release with profiling off, or the numbers mean nothing.
 *   cmake -B build-rel -S . -DCMAKE_BUILD_TYPE=Release -DNO_PROFILING=ON
 * Usage: bench-delta-matrix [--quick]
 */

#include <sched.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <random>
#include <cstdint>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "mata/relation.hh"

using mata::Delta;
using mata::State;
using mata::StatePost;
using mata::StateSet;
using mata::Symbol;

namespace {

[[gnu::always_inline]] inline void escape(void* p) { asm volatile("" : : "g"(p) : "memory"); }

/// Built with the batch @c add(), one call per post: adding target by target is a sorted insert each
///  time and dominates the run at these sizes.
Delta build(const State num_of_states, const size_t symbols, const size_t targets) {
	std::mt19937_64 rng{0x5eed};
	std::uniform_int_distribution<State> pick{0, num_of_states - 1};
	Delta delta{num_of_states};
	std::vector<State> scratch;
	for (State source{0}; source < num_of_states; ++source) {
		for (size_t s{0}; s < symbols; ++s) {
			scratch.clear();
			scratch.reserve(targets);
			for (size_t t{0}; t < targets; ++t) { scratch.push_back(pick(rng)); }
			std::ranges::sort(scratch);
			scratch.erase(std::ranges::unique(scratch).begin(), scratch.end());
			// Strided: a mintermized alphabet is not dense.
			delta.add(source, static_cast<Symbol>(s * 3 + 1), StateSet{scratch});
		}
	}
	return delta;
}

// --- the hand-written baselines ------------------------------------------------------------------

size_t manual_successors(const Delta& delta, const State n) {
	size_t acc{0};
	for (State q{0}; q < n; ++q) {
		for (const mata::SymbolPost& sp : delta.state_post(q)) {
			for (const State t : sp.targets) { acc += t; }
		}
	}
	return acc;
}

size_t manual_moves(const Delta& delta, const State n) {
	size_t acc{0};
	for (State q{0}; q < n; ++q) {
		for (const mata::SymbolPost& sp : delta.state_post(q)) {
			for (const State t : sp.targets) { acc += t + sp.symbol; }
		}
	}
	return acc;
}

// --- the generic accessors -----------------------------------------------------------------------

size_t gen_successors(const Delta& delta, const State n) {
	size_t acc{0};
	for (State q{0}; q < n; ++q) { delta.for_each_successor(q, [&](const State t) { acc += t; }); }
	return acc;
}

size_t gen_moves(const Delta& delta, const State n) {
	size_t acc{0};
	for (State q{0}; q < n; ++q) {
		delta.for_each_move(q, [&](const Symbol s, const State t) { acc += t + s; });
	}
	return acc;
}

size_t gen_cursor(const Delta& delta, const State n) {
	size_t acc{0};
	for (State q{0}; q < n; ++q) {
		const mata::SuccessorCursor c{delta.successor_cursor(q)};
		for (auto it{c.begin()}; it != std::default_sentinel; ++it) { acc += *it; }
	}
	return acc;
}

/**
 * @brief The proposed fix: a cursor that yields the contiguous target range, not one target.
 *
 * The measured cost of @c mata::SuccessorCursor is in its @c operator++, which tests and carries once
 *  per target while a hand-written inner loop is a flat scan the compiler can unroll. This keeps the
 *  only property Tarjan actually needs -- the position can be stored and resumed -- but hands the
 *  inner loop back to the caller, and therefore to the optimiser.
 *
 * Resumable at post granularity rather than target granularity. That is a real difference: a walk
 *  suspended mid-post has to remember where in the span it was, so Tarjan would carry one extra
 *  index. Whether that is acceptable is a design question; this measures whether it is worth asking.
 */
class RangeCursor {
  public:
	class const_iterator {
	  public:
		StatePost::const_iterator post_it{}, post_end{};

		/// Skip empty posts, so a dereference is always a non-empty span.
		void seek() {
			while (post_it != post_end && post_it->targets.empty()) { ++post_it; }
		}
		std::span<const State> operator*() const { return post_it->target_span(); }
		const_iterator& operator++() {
			++post_it;
			seek();
			return *this;
		}
		bool operator==(std::default_sentinel_t) const { return post_it == post_end; }
	};

	explicit RangeCursor(const StatePost& post) : post_{&post} {}

	const_iterator begin() const {
		const_iterator it;
		it.post_it = post_->begin();
		it.post_end = post_->end();
		it.seek();
		return it;
	}
	std::default_sentinel_t end() const { return std::default_sentinel; }

  private:
	const StatePost* post_;
};

size_t gen_range_cursor(const Delta& delta, const State n) {
	size_t acc{0};
	for (State q{0}; q < n; ++q) {
		const RangeCursor c{delta.state_post(q)};
		for (auto it{c.begin()}; it != std::default_sentinel; ++it) {
			for (const State t : *it) { acc += t; }
		}
	}
	return acc;
}

// --- timing --------------------------------------------------------------------------------------

using Clock = std::chrono::steady_clock;

/**
 * @brief A cursor *composed* out of per-post cursors, one struct per level.
 *
 * Tests the claim at `core/delta.hh:334`, which says of the depth-aware @c mata::SuccessorCursor:
 *  "Composing a cursor out of per-post cursors is measurably slower, and @c Delta is the one place
 *  entitled to know its own representation." That claim is load-bearing — it is the stated reason the
 *  cursor stays depth-specialised rather than generic — so it is worth re-testing directly.
 *
 * The shipping cursor is one flat struct holding both levels' positions at once. This is the same
 *  traversal expressed as a cursor per level, each owning only its own position and delegating
 *  downwards. Identical semantics, identical yield order, one target at a time.
 */
class ComposedCursor {
  public:
	/// The innermost level: the targets of one SymbolPost.
	class TargetCursor {
	  public:
		const State* it{nullptr};
		const State* end{nullptr};
		void init(const mata::SymbolPost& sp) {
			const std::span<const State> t{sp.target_span()};
			it = t.data();
			end = it + t.size();
		}
		bool done() const { return it == end; }
		State get() const { return *it; }
		void next() { ++it; }
	};

	/// One level up: the symbol posts of one state, delegating to TargetCursor.
	class PostCursor {
	  public:
		StatePost::const_iterator it{}, end{};
		TargetCursor inner{};
		void init(const StatePost& post) {
			it = post.begin();
			end = post.end();
			seek();
		}
		void seek() {
			while (it != end) {
				inner.init(*it);
				if (!inner.done()) { return; }
				++it;
			}
		}
		bool done() const { return it == end; }
		State get() const { return inner.get(); }
		void next() {
			inner.next();
			if (inner.done()) {
				++it;
				seek();
			}
		}
	};
};

size_t gen_composed_cursor(const Delta& delta, const State n) {
	size_t acc{0};
	for (State q{0}; q < n; ++q) {
		ComposedCursor::PostCursor c{};
		c.init(delta.state_post(q));
		for (; !c.done(); c.next()) { acc += c.get(); }
	}
	return acc;
}

/**
 * @brief A cursor that stores *indices* rather than iterators.
 *
 * The plan's T3.2 wanted `std::array<std::pair<It, It>, key_arity>`, which cannot be written because
 *  each nesting level has a different iterator type. Indices do not have that problem — a position is
 *  a @c uint32_t at every level — so they *can* live in a homogeneous array, and they are far smaller:
 *  at structure depth 10 an index cursor is 36 bytes against 160 for nested iterators, and Tarjan
 *  allocates one per state.
 *
 * The cost is that a dereference has to re-derive the location instead of holding a pointer to it.
 *  At depth 2 that is two random accesses; deeper it is a descent per dereference. This measures
 *  whether the compactness is affordable.
 */
class IndexCursor {
  public:
	const StatePost* post_{nullptr};
	uint32_t symbol_at{0}, target_at{0};

	void init(const StatePost& post) {
		post_ = &post;
		symbol_at = 0;
		target_at = 0;
		seek();
	}
	/// Advance past symbol posts whose targets are exhausted (or empty).
	void seek() {
		while (symbol_at < post_->size() && target_at >= (post_->begin() + symbol_at)->targets.size()) {
			++symbol_at;
			target_at = 0;
		}
	}
	bool done() const { return symbol_at >= post_->size(); }
	State get() const { return *((post_->begin() + symbol_at)->targets.begin() + target_at); }
	void next() {
		++target_at;
		seek();
	}
};

size_t gen_index_cursor(const Delta& delta, const State n) {
	size_t acc{0};
	for (State q{0}; q < n; ++q) {
		IndexCursor c{};
		c.init(delta.state_post(q));
		for (; !c.done(); c.next()) { acc += c.get(); }
	}
	return acc;
}

/**
 * @brief A compressed-sparse-row mirror of one state's post.
 *
 * The hypothesis this tests: the cursor's cost may not be in its interface but in the *storage*.
 *  Today every @c SymbolPost owns its own @c StateSet, so a walk over a state hops between
 *  separately allocated runs and the cursor has to notice each boundary. Lay all of a state's
 *  targets end to end, with an offset per symbol, and
 *
 *   - the successor walk is one flat contiguous loop over the whole state, no boundaries at all;
 *   - the cursor collapses to a single pointer, and @c operator++ is @c ++p;
 *   - the state costs three allocations instead of one per symbol.
 *
 * This changes the representation, not just the accessor, so it is not a drop-in — it is evidence
 *  about what Phase 2 should pick when it fixes the post types. Symbols stay sorted, so lookup is
 *  still a binary search followed by an offset pair.
 */
struct CsrPost {
	std::vector<Symbol> symbols{};
	std::vector<uint32_t> offsets{}; ///< size symbols.size() + 1
	std::vector<State> targets{};    ///< all targets of the state, grouped by symbol
};

std::vector<CsrPost> to_csr(const Delta& delta, const State n) {
	std::vector<CsrPost> csr(n);
	for (State q{0}; q < n; ++q) {
		CsrPost& c{csr[q]};
		c.offsets.push_back(0);
		for (const mata::SymbolPost& sp : delta.state_post(q)) {
			c.symbols.push_back(sp.symbol);
			for (const State t : sp.targets) { c.targets.push_back(t); }
			c.offsets.push_back(static_cast<uint32_t>(c.targets.size()));
		}
	}
	return csr;
}

/// One flat loop per state. There are no post boundaries to cross.
size_t csr_successors(const std::vector<CsrPost>& csr) {
	size_t acc{0};
	for (const CsrPost& c : csr) {
		for (const State t : c.targets) { acc += t; }
	}
	return acc;
}

size_t csr_moves(const std::vector<CsrPost>& csr) {
	size_t acc{0};
	for (const CsrPost& c : csr) {
		for (size_t i{0}; i < c.symbols.size(); ++i) {
			const Symbol sym{c.symbols[i]};
			for (uint32_t j{c.offsets[i]}; j < c.offsets[i + 1]; ++j) { acc += c.targets[j] + sym; }
		}
	}
	return acc;
}

/// Resumable in one pointer, which is strictly less state than the current cursor carries.
size_t csr_cursor(const std::vector<CsrPost>& csr) {
	size_t acc{0};
	for (const CsrPost& c : csr) {
		const State* p{c.targets.data()};
		const State* const e{p + c.targets.size()};
		for (; p != e; ++p) { acc += *p; }
	}
	return acc;
}

/// One timed burst of @p run. Templated on the callable, so nothing is type-erased into the loop.
template <typename F> double burst(F&& run, const size_t iterations) {
	const auto start{Clock::now()};
	for (size_t i{0}; i < iterations; ++i) {
		size_t acc{run()};
		escape(&acc);
	}
	return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count())
		 / static_cast<double>(iterations);
}

/**
 * @brief Time every implementation, interleaving the repetitions.
 *
 * Measuring one implementation to completion and then the next makes the ratios hostage to whatever
 *  else the machine is doing: a busy second penalises whichever implementation was being measured in
 *  it. Observed directly -- on a loaded desktop, sequential measurement moved `recursive` from 1.00x
 *  to 1.14x against the same baseline. Round-robin means a slow moment lands on all of them.
 *
 * The iteration count is deterministic, so two runs measure the same work, and capped so the tiny
 *  cells do not dominate the sweep.
 */
template <typename Tuple, size_t... Is>
auto measure_all(Tuple&& fs, const size_t transitions, std::index_sequence<Is...>) {
	const size_t iterations{std::clamp<size_t>(2'000'000 / std::max(transitions, size_t{1}), 1, 300'000)};
	std::array<double, sizeof...(Is)> best{};
	best.fill(1e300);
	// Warm the caches and settle the predictors before anything is recorded.
	((void) burst(std::get<Is>(fs), std::min<size_t>(iterations, 2)), ...);
	for (int r{0}; r < 7; ++r) {
		((best[Is] = std::min(best[Is], burst(std::get<Is>(fs), iterations))), ...);
	}
	for (double& b : best) { b /= static_cast<double>(transitions); }
	return best;
}

} // anonymous namespace.

int main(int argc, char* argv[]) {
	const bool quick{argc > 1 && std::string{argv[1]} == "--quick"};

	cpu_set_t affinity;
	CPU_ZERO(&affinity);
	CPU_SET(2, &affinity);
	if (sched_setaffinity(0, sizeof(affinity), &affinity) != 0) {
		std::cerr << "warning: could not pin to a core\n";
	}

	const std::vector<State> SIZES{quick ? std::vector<State>{4, 256} : std::vector<State>{4, 16, 64, 256, 4096, 32768}};
	const std::vector<size_t> AXIS{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 32, 64, 128, 256};

	// Sizes matter as much as speed for the cursors: Tarjan keeps one per state.
	std::cerr << "sizeof: SuccessorCursor::const_iterator=" << sizeof(mata::SuccessorCursor::const_iterator)
			  << " ComposedCursor::PostCursor=" << sizeof(ComposedCursor::PostCursor)
			  << " IndexCursor=" << sizeof(IndexCursor) << "\n";

	std::cout << "states\tsymbols\ttargets\ttransitions\timpl\tns_per_transition\tratio\n" << std::fixed;

	size_t done{0}, skipped{0};
	for (const State n : SIZES) {
		for (const size_t sym : AXIS) {
			for (const size_t tgt : AXIS) {
				// Two caps: total transitions, and the number of target vectors the relation allocates.
				if (static_cast<size_t>(n) * sym * tgt > 8'000'000 || static_cast<size_t>(n) * sym > 2'000'000) {
					++skipped;
					continue;
				}
				const Delta delta{build(n, sym, tgt)};
				const size_t transitions{delta.num_of_transitions()};
				if (transitions == 0) { continue; }

				const std::vector<CsrPost> csr{to_csr(delta, n)};
				const size_t want_s{manual_successors(delta, n)};
				const size_t want_m{manual_moves(delta, n)};
				if (gen_successors(delta, n) != want_s || gen_cursor(delta, n) != want_s
					|| gen_range_cursor(delta, n) != want_s || gen_moves(delta, n) != want_m
					|| csr_successors(csr) != want_s || csr_cursor(csr) != want_s
					|| csr_moves(csr) != want_m || gen_composed_cursor(delta, n) != want_s
					|| gen_index_cursor(delta, n) != want_s) {
					std::cerr << "MISMATCH at " << n << "/" << sym << "/" << tgt << "\n";
					return 1;
				}

				auto fs{std::make_tuple(
					[&] { return manual_successors(delta, n); }, [&] { return manual_moves(delta, n); },
					[&] { return gen_successors(delta, n); }, [&] { return gen_moves(delta, n); },
					[&] { return gen_cursor(delta, n); }, [&] { return gen_range_cursor(delta, n); },
					[&] { return csr_successors(csr); }, [&] { return csr_moves(csr); },
					[&] { return csr_cursor(csr); }, [&] { return gen_composed_cursor(delta, n); },
					[&] { return gen_index_cursor(delta, n); }
				)};
				const auto t{measure_all(fs, transitions, std::make_index_sequence<11>{})};
				const double base_s{t[0]}, base_m{t[1]}, t_fs{t[2]}, t_fm{t[3]}, t_cu{t[4]}, t_rc{t[5]},
					t_cs{t[6]}, t_cm{t[7]}, t_cc{t[8]}, t_co{t[9]}, t_ix{t[10]};

				const auto row = [&](const char* impl, const double ns, const double base) {
					std::cout << n << "\t" << sym << "\t" << tgt << "\t" << transitions << "\t" << impl << "\t"
							  << std::setprecision(5) << ns << "\t" << std::setprecision(4)
							  << (base > 0 ? ns / base : 0.0) << "\n";
				};
				row("manual_successors", base_s, base_s);
				row("manual_moves", base_m, base_m);
				row("for_each_successor", t_fs, base_s);
				row("for_each_move", t_fm, base_m);
				row("successor_cursor", t_cu, base_s);
				row("range_cursor", t_rc, base_s);
				row("csr_successors", t_cs, base_s);
				row("csr_moves", t_cm, base_m);
				row("csr_cursor", t_cc, base_s);
				row("composed_cursor", t_co, base_s);
				row("index_cursor", t_ix, base_s);
				std::cout << std::flush;
				++done;
			}
		}
		std::cerr << "states=" << n << " done, cells so far " << done << " (skipped " << skipped << ")\n";
	}
	std::cerr << "total cells " << done << ", skipped " << skipped << "\n";
	return 0;
}
