#include "mata/ta/ta.hh"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <unordered_set>
#include <vector>

using namespace mata::ta;

namespace {

std::vector<Tree> language_probe() {
	return {
		Tree{0},
		Tree{1},
		Tree{2, {Tree{0}, Tree{1}}},
		Tree{2, {Tree{1}, Tree{0}}},
		Tree{2, {Tree{0}, Tree{0}}},
		Tree{3, {Tree{2, {Tree{0}, Tree{1}}}, Tree{0}}},
	};
}

void check_same_language(const Ta& lhs, const Ta& rhs, const std::vector<Tree>& trees) {
	for (const Tree& tree : trees) { CHECK(lhs.accepts(tree) == rhs.accepts(tree)); }
}

} // namespace

TEST_CASE("mata::ta::Ta nullary rules seed evaluation and final roots accept") {
	Ta aut{};
	const State accepting{aut.add_state()};
	const State other{aut.add_state()};
	aut.final.insert(accepting);
	aut.add_transition(0, accepting);
	aut.add_transition(0, other);

	CHECK(aut.evaluate(Tree{0}) == StateSet{accepting, other});
	CHECK(aut.evaluate(Tree{1}).empty());
	CHECK(aut.accepts(Tree{0}));
	CHECK_FALSE(aut.accepts(Tree{1}));

	Ta empty{};
	Tree witness{99};
	CHECK(aut.is_lang_empty(&witness) == false);
	CHECK(witness == Tree{0});
	CHECK(empty.is_lang_empty());
	CHECK(empty.accepts(Tree{0}) == false);
}

TEST_CASE("mata::ta::Ta ordered hyperedges preserve arity, child order, and repetition") {
	Ta aut{};
	const State a{aut.add_state()};
	const State b{aut.add_state()};
	const State root{aut.add_state()};
	const State pair{aut.add_state()};
	aut.final = {root, pair};
	aut.add_transition(0, a);
	aut.add_transition(1, b);
	aut.add_transition(2, {a, b, a}, root);
	aut.add_transition(3, {root, root}, pair);

	const Tree accepted{2, {Tree{0}, Tree{1}, Tree{0}}};
	CHECK(aut.accepts(accepted));
	CHECK_FALSE(aut.accepts(Tree{2, {Tree{1}, Tree{0}, Tree{0}}}));
	CHECK_FALSE(aut.accepts(Tree{2, {Tree{0}, Tree{1}}}));
	CHECK_FALSE(aut.accepts(Tree{2, {Tree{0}, Tree{1}, Tree{0}, Tree{0}}}));
	CHECK(aut.accepts(Tree{3, {accepted, accepted}}));
	CHECK_FALSE(aut.accepts(Tree{3, {accepted, Tree{0}}}));
}

TEST_CASE("mata::ta::Delta low-level edits reject conflicting ranks atomically") {
	RankedAlphabet alphabet{{0, 1}};
	CHECK_FALSE(alphabet.add(0, 1));
	CHECK_THROWS_AS(alphabet.add(0, 2), std::invalid_argument);
	CHECK(alphabet.arity(0) == 1);
	CHECK_FALSE(alphabet.arity(1).has_value());

	Delta delta{alphabet};
	REQUIRE(delta.add(0, {0}, 1));
	CHECK_FALSE(delta.add(0, {0}, 1));
	CHECK_THROWS_AS(delta.add(0, {0, 0}, 1), std::invalid_argument);
	CHECK(delta.size() == 1);
	CHECK(delta.contains(Transition{0, {0}, 1}));
	CHECK_THROWS_AS(delta.add(1, Limits::max_state), std::overflow_error);
}

TEST_CASE("mata::ta::Ta empty witness stays unreachable without nullary bootstrap") {
	Ta aut{};
	aut.add_state(0);
	aut.add_state(1);
	aut.final.insert(1);
	aut.add_transition(0, {0}, 0);
	aut.add_transition(1, {0}, 1);

	CHECK(aut.evaluate(Tree{0}).empty());
	CHECK(aut.is_lang_empty());
	CHECK_FALSE(aut.accepts(Tree{1, {Tree{1, {Tree{0}}}}}));
}

TEST_CASE("mata::ta::Ta witness is accepted and uses observed ranks") {
	Ta aut{};
	aut.add_state(0);
	aut.add_state(1);
	aut.add_state(2);
	aut.final.insert(2);
	aut.add_transition(0, 0);
	aut.add_transition(0, 1);
	aut.add_transition(1, {0, 1}, 2);

	Tree witness{99};
	REQUIRE_FALSE(aut.is_lang_empty(&witness));
	CHECK(aut.accepts(witness));
	REQUIRE(witness.symbol == 1);
	REQUIRE(witness.children.size() == 2);
	CHECK(witness.children[0] == Tree{0});
	CHECK(witness.children[1] == Tree{0});
}

TEST_CASE("mata::ta::Ta trim removes disconnected productive and unproductive states") {
	Ta aut{};
	const State a{aut.add_state()};
	const State b{aut.add_state()};
	const State accepting{aut.add_state()};
	const State disconnected{aut.add_state()};
	const State unproductive{aut.add_state()};
	aut.final.insert(accepting);
	aut.add_transition(0, a);
	aut.add_transition(1, b);
	aut.add_transition(2, {a, b}, accepting);
	aut.add_transition(3, disconnected);
	aut.add_transition(4, {unproductive}, unproductive);
	aut.add_transition(5, {unproductive}, accepting);

	const Tree accepted{2, {Tree{0}, Tree{1}}};
	StateRenaming renaming{};
	aut.trim(&renaming);
	CHECK(aut.accepts(accepted));
	CHECK_FALSE(aut.accepts(Tree{2, {Tree{0}, Tree{0}}}));
	CHECK_FALSE(aut.accepts(Tree{3}));
	CHECK(aut.num_of_states() == 3);
	CHECK(renaming.size() == 3);
	CHECK(renaming.contains(accepting));
	CHECK_FALSE(renaming.contains(disconnected));
	CHECK_FALSE(renaming.contains(unproductive));
	std::unordered_set<State> renamed_states{};
	for (const auto& [old_state, new_state] : renaming) { renamed_states.insert(new_state); }
	CHECK(renamed_states == std::unordered_set<State>{0, 1, 2});
	aut.validate();
}

TEST_CASE("mata::ta::Ta trim handles automata without useful states") {
	Ta aut{4};
	aut.final.insert(3);
	StateRenaming renaming{{0, 7}};
	aut.trim(&renaming);
	CHECK(aut.num_of_states() == 0);
	CHECK(aut.final.empty());
	CHECK(aut.delta.empty());
	CHECK(renaming.empty());
	CHECK(aut.is_lang_empty());
}

TEST_CASE("mata::ta::Ta determinization recognizes duplicate nullary and ordered rule keys") {
	Ta aut{};
	aut.add_state(0);
	aut.add_state(1);
	aut.add_state(2);
	aut.add_state(3);
	aut.add_state(4);
	aut.final.insert(4);
	aut.add_transition(0, 0);
	aut.add_transition(0, 1);
	CHECK_FALSE(aut.is_deterministic());
	aut.add_transition(1, {0, 0}, 2);
	aut.add_transition(1, {0, 0}, 3);
	aut.add_transition(2, {2, 3}, 4);

	Tree tree{2, {Tree{1, {Tree{0}, Tree{0}}}, Tree{1, {Tree{0}, Tree{0}}}}};
	REQUIRE(aut.accepts(tree));
	std::unordered_map<StateSet, State> subset_map{};
	const Ta deterministic{aut.determinize(&subset_map)};
	CHECK(deterministic.is_deterministic());
	CHECK(deterministic.accepts(tree));
	CHECK_FALSE(deterministic.accepts(Tree{2, {Tree{1, {Tree{0}, Tree{0}}}, Tree{0}}}));
	std::unordered_set<State> mapped_states{};
	for (const auto& [macrostate, state] : subset_map) {
		CHECK_FALSE(macrostate.empty());
		mapped_states.insert(state);
	}
	CHECK(mapped_states == std::unordered_set<State>{0, 1, 2});
	CHECK(subset_map.contains(StateSet{0, 1}));
	CHECK(subset_map.contains(StateSet{2, 3}));
}

TEST_CASE("mata::ta::Ta subset construction combines independent child macrostate tuples") {
	Ta aut{};
	aut.add_state(0);
	aut.add_state(1);
	aut.add_state(2);
	aut.add_state(3);
	aut.final.insert(3);
	aut.add_transition(0, 0);
	aut.add_transition(1, {0}, 1);
	aut.add_transition(2, {0}, 2);
	aut.add_transition(3, {1, 2}, 3);

	const Tree accepted{3, {Tree{1, {Tree{0}}}, Tree{2, {Tree{0}}}}};
	REQUIRE(aut.accepts(accepted));
	const Ta deterministic{aut.determinize()};
	CHECK(deterministic.is_deterministic());
	CHECK(deterministic.accepts(accepted));
	CHECK_FALSE(deterministic.accepts(Tree{3, {Tree{2, {Tree{0}}}, Tree{2, {Tree{0}}}}}));
	CHECK_FALSE(deterministic.accepts(Tree{3, {Tree{1, {Tree{0}}}, Tree{1, {Tree{0}}}}}));
}

TEST_CASE("mata::ta::Ta determinize preserves a deterministic ordered language") {
	Ta aut{};
	aut.add_state(0);
	aut.add_state(1);
	aut.add_state(2);
	aut.final.insert(2);
	aut.add_transition(0, 0);
	aut.add_transition(1, 1);
	aut.add_transition(2, {0, 1}, 2);
	REQUIRE(aut.is_deterministic());

	const Ta deterministic{aut.determinize()};
	CHECK(deterministic.is_deterministic());
	check_same_language(aut, deterministic, language_probe());
	const Ta deterministic_again{deterministic.determinize()};
	CHECK(deterministic_again.is_deterministic());
	check_same_language(deterministic, deterministic_again, language_probe());
}

TEST_CASE("mata::ta::Ta determinize on empty/bootstrap-only automata") {
	Ta empty{2};
	empty.final.insert(0);
	CHECK(empty.determinize().is_lang_empty());

	Ta leaves{};
	leaves.add_state(0);
	leaves.add_state(1);
	leaves.final.insert(1);
	leaves.add_transition(0, 0);
	leaves.add_transition(0, 1);
	const Ta deterministic{leaves.determinize()};
	CHECK(deterministic.is_deterministic());
	CHECK(deterministic.accepts(Tree{0}));
	CHECK_FALSE(deterministic.accepts(Tree{1}));
}

TEST_CASE("mata::ta token-passing initial trees preserve exactly one token") {
	Ta aut{};
	const State idle{aut.add_state()};
	const State marked{aut.add_state()};
	aut.final.insert(marked);
	aut.add_transition(0, idle); // nil
	aut.add_transition(1, marked); // token leaf
	aut.add_transition(2, {idle, idle}, idle);
	aut.add_transition(2, {marked, idle}, marked);
	aut.add_transition(2, {idle, marked}, marked);
	const auto nil{[] { return Tree{0}; }};
	const auto token{[] { return Tree{1}; }};
	const auto node{[](Tree left, Tree right) { return Tree{2, {std::move(left), std::move(right)}}; }};

	CHECK(aut.accepts(node(token(), nil())));
	CHECK(aut.accepts(node(node(nil(), token()), node(nil(), nil()))));
	CHECK(aut.accepts(node(node(nil(), nil()), node(token(), nil()))));
	CHECK_FALSE(aut.accepts(node(nil(), node(nil(), nil()))));
	CHECK_FALSE(aut.accepts(node(token(), token())));
	CHECK_FALSE(aut.accepts(node(node(token(), nil()), node(nil(), token()))));
	CHECK(aut.is_deterministic());

	const Ta deterministic{aut.determinize()};
	CHECK(deterministic.is_deterministic());
	CHECK(deterministic.accepts(node(node(token(), nil()), nil())));
	CHECK_FALSE(deterministic.accepts(node(token(), token())));
}
