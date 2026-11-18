// Throwaway microbenchmark: mata::utils::OrdVector (devel) vs std::flat_set vs
// alternative OrdVector algorithms.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <flat_set>
#include <random>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "mata/utils/ord-vector.hh"

using Key = uint32_t;
using OV = mata::utils::OrdVector<Key>;
using FS = std::flat_set<Key>;
using Clock = std::chrono::steady_clock;

static volatile uint64_t sink;
struct Vec {
	std::vector<Key> v;
	Vec(mata::utils::sorted_unique_t, std::vector<Key>&& vec) : v{std::move(vec)} {}
	size_t size() const { return v.size(); }
};

// Range-insert alternatives, applied to OrdVector's underlying vector.
static bool insert_inplace_merge(std::vector<Key>& v, std::span<const Key> keys) {
	if (keys.empty()) { return false; }
	if (v.empty() || v.back() < keys.front()) {
		v.insert(v.end(), keys.begin(), keys.end());
		return true;
	}
	const size_t n{v.size()};
	v.insert(v.end(), keys.begin(), keys.end());
	std::inplace_merge(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(n), v.end());
	v.erase(std::unique(v.begin(), v.end()), v.end());
	return v.size() > n;
}

static bool insert_backmerge(std::vector<Key>& v, std::span<const Key> keys) {
	if (keys.empty()) { return false; }
	const size_t n{v.size()};
	if (n == 0 || v.back() < keys.front()) {
		v.insert(v.end(), keys.begin(), keys.end());
		return true;
	}
	const Key* lhs{v.data()};
	const Key* const lhs_end{lhs + n};
	const Key* rhs{keys.data()};
	const Key* const rhs_end{rhs + keys.size()};
	size_t union_size{n};
	while (rhs != rhs_end) {
		lhs = std::lower_bound(lhs, lhs_end, *rhs);
		if (lhs == lhs_end) {
			union_size += static_cast<size_t>(rhs_end - rhs);
			break;
		}
		if (*lhs != *rhs) { ++union_size; }
		++rhs;
	}
	if (union_size == n) { return false; }
	v.resize(union_size);
	Key* const out{v.data()};
	auto i{static_cast<std::ptrdiff_t>(n) - 1};
	auto j{static_cast<std::ptrdiff_t>(keys.size()) - 1};
	auto w{static_cast<std::ptrdiff_t>(union_size) - 1};
	while (j >= 0) {
		if (i >= 0 && out[i] > keys[static_cast<size_t>(j)]) {
			out[w--] = out[i--];
		} else {
			if (i >= 0 && out[i] == keys[static_cast<size_t>(j)]) { --i; }
			out[w--] = keys[static_cast<size_t>(j--)];
		}
	}
	return true;
}

// Linear two-pointer merge into the tail without the counting pass: grow by
// |keys|, merge back to front,
//  then close the gap at the front with one memmove.
static bool insert_backmerge_shift(std::vector<Key>& v, std::span<const Key> keys) {
	if (keys.empty()) { return false; }
	const size_t n{v.size()};
	if (n == 0 || v.back() < keys.front()) {
		v.insert(v.end(), keys.begin(), keys.end());
		return true;
	}
	v.resize(n + keys.size());
	Key* const out{v.data()};
	auto i{static_cast<std::ptrdiff_t>(n) - 1};
	auto j{static_cast<std::ptrdiff_t>(keys.size()) - 1};
	auto w{static_cast<std::ptrdiff_t>(n + keys.size()) - 1};
	while (j >= 0) {
		if (i >= 0 && out[i] > keys[static_cast<size_t>(j)]) {
			out[w--] = out[i--];
		} else {
			if (i >= 0 && out[i] == keys[static_cast<size_t>(j)]) { --i; }
			out[w--] = keys[static_cast<size_t>(j--)];
		}
	}
	// Elements out[0..i] are untouched and in place; the merged suffix starts at
	// w + 1.
	const auto gap{w - i};
	if (gap > 0) {
		std::move(out + w + 1, out + n + keys.size(), out + i + 1);
		v.resize(n + keys.size() - static_cast<size_t>(gap));
	}
	return v.size() > n;
}

static std::vector<Key> random_sorted(std::mt19937& rng, size_t size, Key universe) {
	std::uniform_int_distribution<Key> dist{0, universe - 1};
	std::vector<Key> keys;
	keys.reserve(size);
	while (keys.size() < size) { keys.push_back(dist(rng)); }
	std::ranges::sort(keys);
	keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
	return keys;
}

template <class Fn> static double time_ns(Fn&& fn, size_t ops) {
	double best{1e300};
	for (int rep = 0; rep < 5; ++rep) {
		const auto start{Clock::now()};
		fn();
		const auto end{Clock::now()};
		best = std::min(best, std::chrono::duration<double, std::nano>(end - start).count() / static_cast<double>(ops));
	}
	return best;
}

static void row(const std::string& name, double ov, double fs, double im, double bm, double bms) {
	std::printf("%-44s %9.2f %9.2f %9.2f %9.2f %9.2f\n", name.c_str(), ov, fs, im, bm, bms);
}

int main() {
	std::mt19937 rng{42};
	std::printf(
		"ns/op (min of 5)                             OrdVector flat_set "
		" inplmerge backmerge backshift\n"
	);

	// 1. Range insert (union) of a sorted span into a set: the Delta/post/union
	// hot shape.
	struct Shape {
		const char* name;
		size_t lhs, rhs;
		Key universe;
	};
	for (const Shape shape :
		 {Shape{"union 4 += 4 (overlap)", 4, 4, 16}, Shape{"union 16 += 16 (overlap)", 16, 16, 64},
		  Shape{"union 64 += 8 (sparse)", 64, 8, 1'024}, Shape{"union 256 += 256", 256, 256, 1'024},
		  Shape{"union 1024 += 16", 1'024, 16, 8'192}, Shape{"union 16 += 1024", 16, 1'024, 8'192},
		  Shape{"union 1000 += subset", 1'000, 100, 1'000}}) {
		constexpr size_t pairs{512};
		std::vector<std::vector<Key>> lhs(pairs), rhs(pairs);
		for (size_t p = 0; p < pairs; ++p) {
			lhs[p] = random_sorted(rng, shape.lhs, shape.universe);
			rhs[p] = random_sorted(rng, shape.rhs, shape.universe);
			if (std::string{shape.name}.find("subset") != std::string::npos) {
				std::vector<Key> sub;
				for (size_t k = 0; k < lhs[p].size(); k += 10) { sub.push_back(lhs[p][k]); }
				rhs[p] = sub;
			}
		}
		const size_t iters{std::max<size_t>(1, 400'000 / (shape.lhs + shape.rhs))};
		auto bench_vec = [&]<class Set>(Set*, auto insert) {
			return time_ns(
				[&] {
					uint64_t acc{0};
					for (size_t it = 0; it < iters; ++it) {
						const size_t p{it % pairs};
						Set set{mata::utils::sorted_unique, std::vector<Key>{lhs[p]}};
						acc += insert(set, std::span<const Key>{rhs[p]});
						acc += set.size();
					}
					sink = acc;
				},
				iters
			);
		};
		const double ov{bench_vec(static_cast<OV*>(nullptr), [](OV& s, std::span<const Key> k) {
			return s.insert(k);
		})};
		const double im{bench_vec(static_cast<Vec*>(nullptr), [](Vec& s, std::span<const Key> k) {
			return insert_inplace_merge(s.v, k);
		})};
		const double bm{bench_vec(static_cast<Vec*>(nullptr), [](Vec& s, std::span<const Key> k) {
			return insert_backmerge(s.v, k);
		})};
		const double bms{bench_vec(static_cast<Vec*>(nullptr), [](Vec& s, std::span<const Key> k) {
			return insert_backmerge_shift(s.v, k);
		})};
		const double fs{time_ns(
			[&] {
				uint64_t acc{0};
				for (size_t it = 0; it < iters; ++it) {
					const size_t p{it % pairs};
					FS set{std::sorted_unique, std::vector<Key>{lhs[p]}};
					const size_t before{set.size()};
					set.insert(std::sorted_unique, rhs[p].begin(), rhs[p].end());
					acc += set.size() > before;
					acc += set.size();
				}
				sink = acc;
			},
			iters
		)};
		row(shape.name, ov, fs, im, bm, bms);
	}

	// 2. Building a set by single-key inserts.
	for (const auto [name, size, ordered] :
		 {std::tuple{"build 16 random", 16, false}, std::tuple{"build 256 random", 256, false},
		  std::tuple{"build 4096 random", 4'096, false}, std::tuple{"build 256 ascending", 256, true},
		  std::tuple{"build 4096 ascending", 4'096, true}}) {
		std::vector<Key> keys(static_cast<size_t>(size));
		for (size_t k = 0; k < keys.size(); ++k) { keys[k] = static_cast<Key>(k * 3); }
		if (!ordered) { std::ranges::shuffle(keys, rng); }
		const size_t iters{std::max<size_t>(1, 2'000'000 / keys.size())};
		const double ov{time_ns(
			[&] {
				uint64_t acc{0};
				for (size_t it = 0; it < iters; it += keys.size()) {
					OV set{};
					for (const Key k : keys) { acc += set.insert(k).second; }
				}
				sink = acc;
			},
			iters
		)};
		const double fs{time_ns(
			[&] {
				uint64_t acc{0};
				for (size_t it = 0; it < iters; it += keys.size()) {
					FS set{};
					for (const Key k : keys) { acc += set.insert(k).second; }
				}
				sink = acc;
			},
			iters
		)};
		const double fp{time_ns(
			[&] {
				uint64_t acc{0};
				for (size_t it = 0; it < iters; it += keys.size()) {
					std::vector<Key> set{};
					for (const Key k : keys) {
						if (set.empty() || set.back() < k) {
							set.push_back(k);
							++acc;
							continue;
						}
						auto pos{std::lower_bound(set.begin(), set.end(), k)};
						if (pos == set.end() || *pos != k) {
							set.insert(pos, k);
							++acc;
						}
					}
				}
				sink = acc;
			},
			iters
		)};
		row(std::string{name} + " (3rd col: append fast path)", ov, fs, fp, 0, 0);
	}

	// 3. Lookups.
	for (const size_t size : {8UL, 64UL, 1'024UL, 65'536UL}) {
		const std::vector<Key> keys{random_sorted(rng, size, static_cast<Key>(size * 4))};
		const OV ov_set{mata::utils::sorted_unique, std::vector<Key>{keys}};
		const FS fs_set{std::sorted_unique, std::vector<Key>{keys}};
		std::vector<Key> queries(4'096);
		std::uniform_int_distribution<Key> dist{0, static_cast<Key>(size * 4)};
		for (Key& q : queries) { q = dist(rng); }
		constexpr size_t iters{1 << 20};
		const double ov{time_ns(
			[&] {
				uint64_t acc{0};
				for (size_t it = 0; it < iters; ++it) { acc += ov_set.contains(queries[it & 4'095]); }
				sink = acc;
			},
			iters
		)};
		const double fs{time_ns(
			[&] {
				uint64_t acc{0};
				for (size_t it = 0; it < iters; ++it) { acc += fs_set.contains(queries[it & 4'095]); }
				sink = acc;
			},
			iters
		)};
		const double sb{time_ns(
			[&] {
				uint64_t acc{0};
				const auto& v{ov_set.to_vector()};
				for (size_t it = 0; it < iters; ++it) {
					const Key q{queries[it & 4'095]};
					auto length{static_cast<std::ptrdiff_t>(v.size())};
					const Key* first{v.data()};
					while (length > 0) {
						const auto rem{length % 2};
						length /= 2;
						first += (first[length] < q) ? length + rem : 0;
					}
					acc += first != v.data() + v.size() && *first == q;
				}
				sink = acc;
			},
			iters
		)};
		row("contains, size " + std::to_string(size) + " (3rd col: branchless)", ov, fs, sb, 0, 0);
	}
	return 0;
}
