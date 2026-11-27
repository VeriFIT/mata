/** @file
 * @brief Tests for ranked tree simulations and simulation-based reduction.
 */

#include "mata/ta/simulation.hh"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace mata::ta;

namespace {

std::vector<bool> reference_evaluate(const Ta& aut, const Tree& tree) {
	std::vector<std::vector<bool>> children;
	children.reserve(tree.children.size());
	for (const Tree& child : tree.children) { children.push_back(reference_evaluate(aut, child)); }

	std::vector<bool> roots(aut.num_of_states(), false);
	for (const auto& [target, rules] : aut.delta) {
		for (const Transition& rule : rules) {
			if (rule.symbol != tree.symbol || rule.children.size() != tree.children.size()) { continue; }
			bool matches{true};
			for (size_t i{0}; i < rule.children.size(); ++i) {
				if (!children[i][rule.children[i]]) {
					matches = false;
					break;
				}
			}
			if (matches) { roots[target] = true; }
		}
	}
	return roots;
}

bool reference_accepts(const Ta& aut, const Tree& tree) {
	const std::vector<bool> roots{reference_evaluate(aut, tree)};
	for (const State state : aut.final) {
		if (roots[state]) { return true; }
	}
	return false;
}

void check_language(const Ta& lhs, const Ta& rhs, const std::vector<Tree>& trees) {
	for (size_t i{0}; i < trees.size(); ++i) {
		CAPTURE(i, trees[i].symbol);
		CHECK(reference_accepts(lhs, trees[i]) == reference_accepts(rhs, trees[i]));
	}
}

Tree unary(const Symbol symbol, const Tree& child) { return Tree{symbol, {child}}; }

} // namespace

TEST_CASE("mata::ta downward simulation covers nullary and ranked rules") {
	Ta aut{4, RankedAlphabet{{'a', 0}, {'b', 0}, {'f', 1}}};
	aut.add_transition('a', 0);
	aut.add_transition('b', 1);
	aut.add_transition('f', {0}, 2);
	aut.add_transition('f', {1}, 3);

	const auto relation{compute_simulation(aut, SimulationDirection::Downward)};
	CHECK(relation.get(0, 0));
	CHECK_FALSE(relation.get(0, 1));
	CHECK_FALSE(relation.get(1, 0));
	CHECK_FALSE(relation.get(2, 3));
	CHECK_FALSE(relation.get(3, 2));
	CHECK(relation.get(2, 2));
}

TEST_CASE("mata::ta upward simulation is distinct and preserves finality") {
	SECTION("different nullary rules do not distinguish context-free states") {
		Ta aut{2, RankedAlphabet{{'a', 0}, {'b', 0}}};
		aut.add_transition('a', 0);
		aut.add_transition('b', 1);
		const auto downward{compute_simulation(aut, SimulationDirection::Downward)};
		const auto upward{compute_simulation(aut, SimulationDirection::Upward)};
		CHECK_FALSE(downward.get(0, 1));
		CHECK(upward.get(0, 1));
		CHECK(upward.get(1, 0));
	}

	SECTION("a final state needs a final upward simulator") {
		Ta aut{2, RankedAlphabet{{'a', 0}}};
		aut.add_transition('a', 0);
		aut.add_transition('a', 1);
		aut.final = {0};
		const auto downward{compute_simulation(aut, SimulationDirection::Downward)};
		const auto upward{compute_simulation(aut, SimulationDirection::Upward)};
		CHECK(downward.get(0, 1));
		CHECK(downward.get(1, 0));
		CHECK_FALSE(upward.get(0, 1));
		CHECK(upward.get(1, 0));
	}
}

TEST_CASE("mata::ta simulations respect repeated children and one-hole positions") {
	SECTION("downward coverage compares every child occurrence") {
		Ta aut{4, RankedAlphabet{{'a', 0}, {'b', 0}, {'f', 2}}};
		aut.add_transition('a', 0);
		aut.add_transition('b', 1);
		aut.add_transition('f', {0, 0}, 2);
		aut.add_transition('f', {1, 0}, 3);
		auto relation{compute_simulation(aut, SimulationDirection::Downward)};
		CHECK_FALSE(relation.get(2, 3));

		aut.add_transition('f', {0, 0}, 3);
		relation = compute_simulation(aut, SimulationDirection::Downward);
		CHECK(relation.get(2, 3));
	}

	SECTION("upward coverage does not identify sibling hole positions") {
		Ta aut{5, RankedAlphabet{{'a', 0}, {'f', 2}}};
		aut.add_transition('a', 0);
		aut.add_transition('a', 1);
		aut.add_transition('a', 2);
		aut.add_transition('f', {0, 2}, 3);
		aut.add_transition('f', {2, 1}, 4);
		auto relation{compute_simulation(aut, SimulationDirection::Upward)};
		CHECK_FALSE(relation.get(0, 1));

		aut.add_transition('f', {1, 2}, 4);
		relation = compute_simulation(aut, SimulationDirection::Upward);
		CHECK(relation.get(0, 1));
	}
}

TEST_CASE("mata::ta downward reduction quotients mutual classes and root finality") {
	Ta aut{2, RankedAlphabet{{'a', 0}, {'b', 0}}};
	aut.add_transition('a', 0);
	aut.add_transition('a', 1);
	aut.final = {0};
	const std::vector<Tree> language_probe{Tree{'a'}, Tree{'b'}, unary('a', Tree{'a'})};

	const Ta reduced{reduce_by_simulation(aut, SimulationDirection::Downward)};
	CHECK(reduced.num_of_states() == 1);
	check_language(aut, reduced, language_probe);
}

TEST_CASE("mata::ta rejects standalone upward reduction") {
	Ta aut{2, RankedAlphabet{{'a', 0}}};
	aut.add_transition('a', 0);
	aut.add_transition('a', 1);
	CHECK_THROWS_AS(reduce_by_simulation(aut, SimulationDirection::Upward), std::invalid_argument);
}

TEST_CASE("mata::ta composed reduction combines directions in a protocol automaton") {
	// The q/r branches occur in both protocol contexts. Downward simulation is strictly one-way q -> r, while their
	// composed relation is mutual.
	Ta aut{6, RankedAlphabet{{'a', 0}, {'b', 0}, {'f', 1}, {'g', 1}}};
	aut.add_transition('a', 0);
	aut.add_transition('a', 1);
	aut.add_transition('b', 1);
	aut.add_transition('f', {0}, 2);
	aut.add_transition('f', {1}, 3);
	aut.add_transition('g', {0}, 4);
	aut.add_transition('g', {1}, 5);
	aut.final = {2, 3, 4, 5};

	const Tree a{'a'};
	const Tree b{'b'};
	const std::vector<Tree> language_probe{
		a, b, unary('f', a), unary('f', b), unary('g', a), unary('g', b), unary('f', unary('f', a))
	};
	const Ta downward{reduce_by_simulation(aut)};
	const Ta composed{reduce_by_composed_simulation(aut)};
	CHECK(downward.num_of_states() == 6);
	CHECK(composed.num_of_states() == 2);
	check_language(aut, downward, language_probe);
	check_language(aut, composed, language_probe);
}
