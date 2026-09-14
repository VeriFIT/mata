/** @file
 * @brief Does a depth-generic walk cost anything against hand-written nested loops?
 *
 * A throwaway prototype, deliberately outside the library: it settles which access implementation to
 *  build in Phase 2/3 *before* the posts are templated, because that choice is hard to reverse once
 *  260-odd call sites depend on it. Nothing here includes @c mata::Delta; the post chain is rebuilt
 *  from scratch so an arbitrary @c key_arity can be instantiated today, which the real relation
 *  cannot (`DeltaLike` requires `key_arity == 1`).
 *
 * The baseline is what a user can write by hand: explicit nested `for` loops over the concrete
 *  structure, one loop per key. Everything else is measured as a ratio against that, because the
 *  question is not "how fast is the library" but "what does the abstraction cost someone who could
 *  have written the loops themselves".
 *
 * Four implementations:
 *
 *   manual      explicit nested loops, one per key arity. What the user writes.
 *   recursive   `if constexpr` recursion over the nesting, callback inlined by type. T3.1.
 *   cursor      resumable, stores its position and can be suspended mid-walk. T3.2, and the
 *               shape Tarjan's SCC walk needs.
 *   erased      the recursive walk through a `std::function`, to price type erasure. Included
 *               because it is the tempting shortcut for keeping the walk out of headers.
 *
 * Build: Release, profiling off.
 *   cmake -B build-rel -S . -DCMAKE_BUILD_TYPE=Release -DNO_PROFILING=ON
 * Usage: bench-depth-prototype
 * Output: TSV on stdout.
 */

#include <sched.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using State = unsigned long;
using Symbol = unsigned long;

[[gnu::always_inline]] inline void escape(void* p) { asm volatile("" : : "g"(p) : "memory"); }

// --- the post chain, rebuilt so any arity can be instantiated ------------------------------------

/// The innermost post: the targets reached once every key has been supplied.
struct Targets {
	static constexpr size_t arity{0};
	std::vector<State> targets{};
};

/// One post: an ordered map from a key to the post nested under it. Sorted vector, as `StatePost` is.
template <typename Nested> struct Post {
	static constexpr size_t arity{Nested::arity + 1};
	struct Entry {
		Symbol key{};
		Nested nested{};
	};
	std::vector<Entry> entries{};
};

template <size_t N> struct ChainOf {
	using type = Post<typename ChainOf<N - 1>::type>;
};
template <> struct ChainOf<0> {
	using type = Targets;
};
/// The relation at key arity @p N: indexed by source state, then @p N keys, then targets.
template <size_t N> using Relation = std::vector<typename ChainOf<N>::type>;

// --- building ------------------------------------------------------------------------------------

template <typename P>
void fill(P& post, const size_t keys, const size_t targets, const State num_of_states, std::mt19937_64& rng) {
	if constexpr (P::arity == 0) {
		std::uniform_int_distribution<State> pick{0, num_of_states - 1};
		post.targets.reserve(targets);
		for (size_t i{0}; i < targets; ++i) { post.targets.push_back(pick(rng)); }
		std::ranges::sort(post.targets);
		post.targets.erase(std::ranges::unique(post.targets).begin(), post.targets.end());
	} else {
		post.entries.reserve(keys);
		for (size_t k{0}; k < keys; ++k) {
			// Strided, not contiguous: a mintermized alphabet is not dense.
			post.entries.push_back({static_cast<Symbol>(k * 3 + 1), {}});
			fill(post.entries.back().nested, keys, targets, num_of_states, rng);
		}
	}
}

template <size_t N> Relation<N> build(const State num_of_states, const size_t keys, const size_t targets) {
	std::mt19937_64 rng{0x5eed};
	Relation<N> relation(num_of_states);
	for (State q{0}; q < num_of_states; ++q) { fill(relation[q], keys, targets, num_of_states, rng); }
	return relation;
}

template <typename P> size_t count_targets(const P& post) {
	if constexpr (P::arity == 0) {
		return post.targets.size();
	} else {
		size_t n{0};
		for (const auto& e : post.entries) { n += count_targets(e.nested); }
		return n;
	}
}

// --- 1. manual: explicit nested loops, one per key -----------------------------------------------
//
// Written out rather than generated, because the point of comparison is the code a user would
//  actually write against the concrete structure. The tedium at arity 6 is itself a result.

size_t manual(const Relation<1>& r) {
	size_t acc{0};
	for (const auto& p : r)
		for (const auto& a : p.entries)
			for (const State t : a.nested.targets) acc += t;
	return acc;
}
size_t manual(const Relation<2>& r) {
	size_t acc{0};
	for (const auto& p : r)
		for (const auto& a : p.entries)
			for (const auto& b : a.nested.entries)
				for (const State t : b.nested.targets) acc += t;
	return acc;
}
size_t manual(const Relation<3>& r) {
	size_t acc{0};
	for (const auto& p : r)
		for (const auto& a : p.entries)
			for (const auto& b : a.nested.entries)
				for (const auto& c : b.nested.entries)
					for (const State t : c.nested.targets) acc += t;
	return acc;
}
size_t manual(const Relation<4>& r) {
	size_t acc{0};
	for (const auto& p : r)
		for (const auto& a : p.entries)
			for (const auto& b : a.nested.entries)
				for (const auto& c : b.nested.entries)
					for (const auto& d : c.nested.entries)
						for (const State t : d.nested.targets) acc += t;
	return acc;
}
size_t manual(const Relation<5>& r) {
	size_t acc{0};
	for (const auto& p : r)
		for (const auto& a : p.entries)
			for (const auto& b : a.nested.entries)
				for (const auto& c : b.nested.entries)
					for (const auto& d : c.nested.entries)
						for (const auto& e : d.nested.entries)
							for (const State t : e.nested.targets) acc += t;
	return acc;
}
size_t manual(const Relation<6>& r) {
	size_t acc{0};
	for (const auto& p : r)
		for (const auto& a : p.entries)
			for (const auto& b : a.nested.entries)
				for (const auto& c : b.nested.entries)
					for (const auto& d : c.nested.entries)
						for (const auto& e : d.nested.entries)
							for (const auto& f : e.nested.entries)
								for (const State t : f.nested.targets) acc += t;
	return acc;
}

// --- 2. recursive: if constexpr over the nesting, callback inlined by type -----------------------

template <typename P, typename F> [[gnu::always_inline]] inline void walk(const P& post, F&& fn) {
	if constexpr (P::arity == 0) {
		for (const State t : post.targets) { fn(t); }
	} else {
		for (const auto& e : post.entries) { walk(e.nested, fn); }
	}
}

template <size_t N> size_t recursive(const Relation<N>& r) {
	size_t acc{0};
	for (const auto& p : r) { walk(p, [&](const State t) { acc += t; }); }
	return acc;
}

// --- 3. cursor: resumable, stores its own position ----------------------------------------------
//
// NOTE for the design decision: the plan's T3.2 proposes
//   std::array<std::pair<It, It>, key_arity>
// which cannot be written. Every level of the nesting is a *different type*, so its iterator is a
//  different type too -- `Post<Post<Targets>>::Entry` and `Post<Targets>::Entry` share nothing. A
//  homogeneous array is only possible if every post is forced to the same container, which throws
//  away the reason for nesting in the first place (S3.1: each post free to be a hash post, a bitmap
//  post). So this is the heterogeneous form: one cursor struct per level, nested by type.

template <typename P> struct Cursor;

template <> struct Cursor<Targets> {
	const State* it{nullptr};
	const State* end{nullptr};
	void init(const Targets& t) {
		it = t.targets.data();
		end = it + t.targets.size();
	}
	bool done() const { return it == end; }
	State get() const { return *it; }
	void next() { ++it; }
};

template <typename Nested> struct Cursor<Post<Nested>> {
	using Entries = std::vector<typename Post<Nested>::Entry>;
	typename Entries::const_iterator it{}, end{};
	Cursor<Nested> inner{};

	void init(const Post<Nested>& p) {
		it = p.entries.begin();
		end = p.entries.end();
		seek();
	}
	/// Restore the invariant "positioned on a target, or exhausted".
	void seek() {
		while (it != end) {
			inner.init(it->nested);
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

template <size_t N> size_t cursor(const Relation<N>& r) {
	size_t acc{0};
	for (const auto& p : r) {
		Cursor<typename ChainOf<N>::type> c{};
		c.init(p);
		for (; !c.done(); c.next()) { acc += c.get(); }
	}
	return acc;
}

// --- 3b. range cursor: resumable, but yields the whole contiguous run of targets ---------------
//
// Same nesting as the cursor above; the difference is what a dereference returns. Instead of one
//  target and a test-and-carry per target, it descends to a leaf and hands back that leaf's span,
//  leaving the inner loop to the caller. Resumable at leaf granularity rather than target.

template <typename P> struct RangeCursor;

template <> struct RangeCursor<Targets> {
	const Targets* leaf{nullptr};
	bool spent{true};
	void init(const Targets& t) {
		leaf = &t;
		spent = t.targets.empty();
	}
	bool done() const { return spent; }
	std::span<const State> get() const { return {leaf->targets.data(), leaf->targets.size()}; }
	void next() { spent = true; }
};

template <typename Nested> struct RangeCursor<Post<Nested>> {
	using Entries = std::vector<typename Post<Nested>::Entry>;
	typename Entries::const_iterator it{}, end{};
	RangeCursor<Nested> inner{};

	void init(const Post<Nested>& p) {
		it = p.entries.begin();
		end = p.entries.end();
		seek();
	}
	void seek() {
		while (it != end) {
			inner.init(it->nested);
			if (!inner.done()) { return; }
			++it;
		}
	}
	bool done() const { return it == end; }
	std::span<const State> get() const { return inner.get(); }
	void next() {
		inner.next();
		if (inner.done()) {
			++it;
			seek();
		}
	}
};

template <size_t N> size_t range_cursor(const Relation<N>& r) {
	size_t acc{0};
	for (const auto& p : r) {
		RangeCursor<typename ChainOf<N>::type> c{};
		c.init(p);
		for (; !c.done(); c.next()) {
			for (const State t : c.get()) { acc += t; }
		}
	}
	return acc;
}

// --- 3c. flat cursor: hand-written explicit carry, one specialisation per arity -----------------
//
// The hypothesis: today's shipping cursor is flat -- it holds both levels' positions as named members
//  and carries between them explicitly -- and it beats the composed form by ~3.6% at arity 1. If that
//  gap comes from delegation the compiler does not fully flatten, then hand-writing the flat carry for
//  arity 2 and 3 as well should keep the win across the whole supported range (structure depth <= 4).
//
// This is what `std::array<std::pair<It, It>, key_arity>` was reaching for. It cannot be an array,
//  because each level's iterator is a different type, but it can be a struct with distinct members.
//  Writing them by hand is the price, and the arity-3 body below is the evidence of what that price is.

template <size_t N> struct FlatCursor;

template <> struct FlatCursor<1> {
	using L0 = ChainOf<1>::type; // Post<Targets>
	std::vector<L0::Entry>::const_iterator a{}, a_end{};
	const State* t{nullptr};
	const State* t_end{nullptr};

	bool load_from_a() {
		for (; a != a_end; ++a) {
			const std::vector<State>& v{a->nested.targets};
			if (!v.empty()) {
				t = v.data();
				t_end = t + v.size();
				return true;
			}
		}
		return false;
	}
	void init(const L0& root) {
		a = root.entries.begin();
		a_end = root.entries.end();
		if (!load_from_a()) { t = t_end = nullptr; }
	}
	bool done() const { return t == t_end; }
	State get() const { return *t; }
	void next() {
		if (++t != t_end) { return; }
		++a;
		if (!load_from_a()) { t = t_end = nullptr; }
	}
};

template <> struct FlatCursor<2> {
	using L0 = ChainOf<2>::type;   // Post<Post<Targets>>
	using L1 = ChainOf<1>::type;   // Post<Targets>
	std::vector<L0::Entry>::const_iterator a{}, a_end{};
	std::vector<L1::Entry>::const_iterator b{}, b_end{};
	const State* t{nullptr};
	const State* t_end{nullptr};

	bool scan_b() {
		for (; b != b_end; ++b) {
			const std::vector<State>& v{b->nested.targets};
			if (!v.empty()) {
				t = v.data();
				t_end = t + v.size();
				return true;
			}
		}
		return false;
	}
	bool scan_a() {
		for (; a != a_end; ++a) {
			b = a->nested.entries.begin();
			b_end = a->nested.entries.end();
			if (scan_b()) { return true; }
		}
		return false;
	}
	void init(const L0& root) {
		a = root.entries.begin();
		a_end = root.entries.end();
		if (!scan_a()) { t = t_end = nullptr; }
	}
	bool done() const { return t == t_end; }
	State get() const { return *t; }
	void next() {
		if (++t != t_end) { return; }
		++b;
		if (scan_b()) { return; }
		++a;
		if (!scan_a()) { t = t_end = nullptr; }
	}
};

template <> struct FlatCursor<3> {
	using L0 = ChainOf<3>::type;   // Post<Post<Post<Targets>>>
	using L1 = ChainOf<2>::type;
	using L2 = ChainOf<1>::type;
	std::vector<L0::Entry>::const_iterator a{}, a_end{};
	std::vector<L1::Entry>::const_iterator b{}, b_end{};
	std::vector<L2::Entry>::const_iterator c{}, c_end{};
	const State* t{nullptr};
	const State* t_end{nullptr};

	bool scan_c() {
		for (; c != c_end; ++c) {
			const std::vector<State>& v{c->nested.targets};
			if (!v.empty()) {
				t = v.data();
				t_end = t + v.size();
				return true;
			}
		}
		return false;
	}
	bool scan_b() {
		for (; b != b_end; ++b) {
			c = b->nested.entries.begin();
			c_end = b->nested.entries.end();
			if (scan_c()) { return true; }
		}
		return false;
	}
	bool scan_a() {
		for (; a != a_end; ++a) {
			b = a->nested.entries.begin();
			b_end = a->nested.entries.end();
			if (scan_b()) { return true; }
		}
		return false;
	}
	void init(const L0& root) {
		a = root.entries.begin();
		a_end = root.entries.end();
		if (!scan_a()) { t = t_end = nullptr; }
	}
	bool done() const { return t == t_end; }
	State get() const { return *t; }
	void next() {
		if (++t != t_end) { return; }
		++c;
		if (scan_c()) { return; }
		++b;
		if (scan_b()) { return; }
		++a;
		if (!scan_a()) { t = t_end = nullptr; }
	}
};

template <size_t N> size_t flat_cursor(const Relation<N>& r) {
	size_t acc{0};
	for (const auto& p : r) {
		FlatCursor<N> c{};
		c.init(p);
		for (; !c.done(); c.next()) { acc += c.get(); }
	}
	return acc;
}

// --- 4. erased: the recursive walk behind a std::function ---------------------------------------

template <typename P> void walk_erased(const P& post, const std::function<void(State)>& fn) {
	if constexpr (P::arity == 0) {
		for (const State t : post.targets) { fn(t); }
	} else {
		for (const auto& e : post.entries) { walk_erased(e.nested, fn); }
	}
}

template <size_t N> size_t erased(const Relation<N>& r) {
	size_t acc{0};
	const std::function<void(State)> fn{[&](const State t) { acc += t; }};
	for (const auto& p : r) { walk_erased(p, fn); }
	return acc;
}

// --- timing --------------------------------------------------------------------------------------

using Clock = std::chrono::steady_clock;

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
 * Measuring one to completion and then the next makes the ratios hostage to what else the machine is
 *  doing. Observed: on a loaded desktop, sequential measurement moved `recursive` from 1.00x to
 *  1.14x against the same baseline, while round-robin held it at 1.00x. The iteration count is
 *  deterministic so two runs measure the same work.
 */
template <typename Tuple, size_t... Is>
auto measure_all(Tuple&& fs, const size_t transitions, std::index_sequence<Is...>) {
	const size_t iterations{std::max(size_t{1}, size_t{20'000'000} / std::max(transitions, size_t{1}))};
	std::array<double, sizeof...(Is)> best{};
	best.fill(1e300);
	((void) burst(std::get<Is>(fs), std::min<size_t>(iterations, 3)), ...);
	for (int r{0}; r < 11; ++r) {
		((best[Is] = std::min(best[Is], burst(std::get<Is>(fs), iterations))), ...);
	}
	for (double& b : best) { b /= static_cast<double>(transitions); }
	return best;
}

/// Hold total work roughly constant across arities, so the per-transition costs are comparable.
State states_for(const size_t transitions_per_state) {
	constexpr size_t kTotal{400'000};
	return static_cast<State>(std::clamp<size_t>(kTotal / std::max(transitions_per_state, size_t{1}), 64, 120'000));
}

size_t pow_of(const size_t base, const size_t exp) {
	size_t v{1};
	for (size_t i{0}; i < exp; ++i) { v *= base; }
	return v;
}

template <size_t N> void run_arity(const size_t keys, const size_t targets) {
	const State n{states_for(pow_of(keys, N) * targets)};
	const Relation<N> r{build<N>(n, keys, targets)};
	size_t transitions{0};
	for (const auto& p : r) { transitions += count_targets(p); }

	// Every implementation must agree, or the comparison is meaningless.
	const size_t expect{recursive<N>(r)};
	bool ok_cursor{cursor<N>(r) == expect && range_cursor<N>(r) == expect};
	if constexpr (N <= 3) { ok_cursor = ok_cursor && flat_cursor<N>(r) == expect; }
	const bool ok_erased{erased<N>(r) == expect};
	bool ok_manual{true};
	if constexpr (N <= 6) { ok_manual = manual(r) == expect; }
	if (!ok_cursor || !ok_erased || !ok_manual) {
		std::cerr << "MISMATCH at arity " << N << "\n";
		return;
	}

	// Interleaved, so a busy moment on the machine lands on every implementation rather than one.
	double t_manual{0.0}, t_rec{0.0}, t_cur{0.0}, t_rng{0.0}, t_era{0.0}, t_flat{0.0};
	if constexpr (N <= 3) {
		auto fs{std::make_tuple([&] { return manual(r); }, [&] { return recursive<N>(r); },
								[&] { return cursor<N>(r); }, [&] { return range_cursor<N>(r); },
								[&] { return erased<N>(r); }, [&] { return flat_cursor<N>(r); })};
		const auto t{measure_all(fs, transitions, std::make_index_sequence<6>{})};
		t_manual = t[0];
		t_rec = t[1];
		t_cur = t[2];
		t_rng = t[3];
		t_era = t[4];
		t_flat = t[5];
	} else if constexpr (N <= 6) {
		auto fs{std::make_tuple([&] { return manual(r); }, [&] { return recursive<N>(r); },
								[&] { return cursor<N>(r); }, [&] { return range_cursor<N>(r); },
								[&] { return erased<N>(r); })};
		const auto t{measure_all(fs, transitions, std::make_index_sequence<5>{})};
		t_manual = t[0];
		t_rec = t[1];
		t_cur = t[2];
		t_rng = t[3];
		t_era = t[4];
	} else {
		auto fs{std::make_tuple([&] { return recursive<N>(r); }, [&] { return cursor<N>(r); },
								[&] { return range_cursor<N>(r); }, [&] { return erased<N>(r); })};
		const auto t{measure_all(fs, transitions, std::make_index_sequence<4>{})};
		t_rec = t[0];
		t_cur = t[1];
		t_rng = t[2];
		t_era = t[3];
	}

	const auto row = [&](const char* impl, const double ns) {
		std::cout << N << "\t" << (N + 1) << "\t" << keys << "\t" << targets << "\t" << n << "\t" << transitions
				  << "\t" << impl << "\t" << std::fixed << std::setprecision(4) << ns << "\t";
		if (t_manual > 0.0) {
			std::cout << std::setprecision(3) << ns / t_manual;
		} else {
			std::cout << "-";
		}
		std::cout << "\n" << std::flush;
	};
	if (t_manual > 0.0) { row("manual", t_manual); }
	row("recursive", t_rec);
	row("cursor", t_cur);
	row("range_cursor", t_rng);
	if (t_flat > 0.0) { row("flat_cursor", t_flat); }
	row("erased", t_era);
}

template <size_t... Ns> void run_all(const size_t keys, const size_t targets, std::index_sequence<Ns...>) {
	(run_arity<Ns + 1>(keys, targets), ...);
}

} // anonymous namespace.

int main() {
	std::cerr << "sizeof cursors at arity 1/2/3 -- composed: " << sizeof(Cursor<ChainOf<1>::type>) << "/"
			  << sizeof(Cursor<ChainOf<2>::type>) << "/" << sizeof(Cursor<ChainOf<3>::type>)
			  << "   flat: " << sizeof(FlatCursor<1>) << "/" << sizeof(FlatCursor<2>) << "/"
			  << sizeof(FlatCursor<3>) << "\n";

	cpu_set_t affinity;
	CPU_ZERO(&affinity);
	CPU_SET(2, &affinity);
	if (sched_setaffinity(0, sizeof(affinity), &affinity) != 0) {
		std::cerr << "warning: could not pin to a core\n";
	}

	std::cout << "arity\tdepth\tkeys\ttargets\tstates\ttransitions\timpl\tns_per_transition\tvs_manual\n";
	// Two fanouts: the corpus-like one (2 keys, 1 target) and a denser one.
	for (const auto& [keys, targets] : {std::pair<size_t, size_t>{2, 1}, std::pair<size_t, size_t>{4, 2}}) {
		run_all(keys, targets, std::make_index_sequence<9>{}); // arity 1..9, i.e. structure depth 2..10
	}
	return 0;
}
