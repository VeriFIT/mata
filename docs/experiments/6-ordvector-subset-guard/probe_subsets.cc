#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <span>
#include <string>
#include <vector>

#include "mata/utils/ord-vector.hh"

using mata::utils::OrdVector;

static double elapsed(const std::chrono::steady_clock::time_point start) {
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

int main(const int argc, char** argv) {
	const std::string mode{argc > 1 ? argv[1] : "subset"};
	constexpr int repetitions{2'000};
	std::vector<int> base_values{};
	base_values.reserve(10'000);
	for (int i{0}; i < 10'000; ++i) { base_values.push_back(i); }
	std::vector<int> inserted{};
	inserted.reserve(1'000);
	for (int i{0}; i < 1'000; ++i) { inserted.push_back(mode == "subset" ? 1'000 + i : 10'000 + i * 2); }

	const auto start{std::chrono::steady_clock::now()};
	size_t result{0};
	for (int repetition = 0; repetition < repetitions; ++repetition) {
		OrdVector<int> values{base_values.begin(), base_values.end()};
		result += values.insert(inserted);
		result += values.size();
	}
	std::printf("%s %.3f %zu\n", mode.c_str(), elapsed(start), result);

	// Correctness check against the standard ordered container, including boundary-equality and self-insertion.
	for (unsigned seed{0}; seed < 500; ++seed) {
		std::mt19937 rng{seed};
		std::vector<int> lhs{};
		std::vector<int> rhs{};
		std::ranges::sample(base_values, std::back_inserter(lhs), 32, rng);
		std::ranges::sample(base_values, std::back_inserter(rhs), 8, rng);
		if (mode == "subset") { rhs = lhs; }
		OrdVector<int> merged{lhs.begin(), lhs.end()};
		merged.insert(rhs);
		std::vector<int> expected{lhs};
		expected.insert(expected.end(), rhs.begin(), rhs.end());
		std::ranges::sort(expected);
		expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
		if (!std::ranges::equal(merged, expected)) { return 2; }
		merged.insert(merged);
		if (!std::ranges::equal(merged, expected)) { return 3; }
	}
	return 0;
}
