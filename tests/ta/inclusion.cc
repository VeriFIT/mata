#include "mata/ta/inclusion.hh"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

using namespace mata::ta;

namespace {

void check_counterexample(const Ta& smaller, const Ta& bigger, const Tree& counterexample) {
	CHECK(smaller.accepts(counterexample));
	CHECK_FALSE(bigger.accepts(counterexample));
}

Ta token_initial() {
	Ta aut{};
	const State idle{aut.add_state()};
	const State marked{aut.add_state()};
	aut.final.insert(marked);
	aut.add_transition(0, idle);
	aut.add_transition(1, marked);
	aut.add_transition(2, {idle, idle}, idle);
	aut.add_transition(2, {marked, idle}, marked);
	aut.add_transition(2, {idle, marked}, marked);
	return aut;
}

Ta token_bad() {
	Ta aut{};
	const State idle{aut.add_state()};
	const State marked{aut.add_state()};
	const State bad{aut.add_state()};
	aut.final.insert(bad);
	aut.add_transition(0, idle);
	aut.add_transition(1, marked);
	aut.add_transition(2, {idle, idle}, idle);
	aut.add_transition(3, {idle, idle}, marked);
	aut.add_transition(2, {marked, idle}, marked);
	aut.add_transition(2, {idle, marked}, marked);
	aut.add_transition(3, {marked, idle}, bad);
	aut.add_transition(3, {idle, marked}, bad);
	aut.add_transition(2, {marked, marked}, bad);
	aut.add_transition(3, {marked, marked}, bad);
	for (const State left : {idle, marked, bad}) {
		aut.add_transition(2, {bad, left}, bad);
		aut.add_transition(2, {left, bad}, bad);
		aut.add_transition(3, {bad, left}, bad);
		aut.add_transition(3, {left, bad}, bad);
	}
	return aut;
}

} // namespace

TEST_CASE("mata::ta::is_included accepts inclusion and returns a valid tree counterexample") {
	Ta smaller{};
	const State small_leaf{smaller.add_state()};
	const State small_root{smaller.add_state()};
	smaller.final.insert(small_root);
	smaller.add_transition(0, small_leaf);
	smaller.add_transition(1, {small_leaf}, small_root);

	Ta bigger{};
	const State big_leaf{bigger.add_state()};
	const State big_root{bigger.add_state()};
	const State other_leaf{bigger.add_state()};
	bigger.final = {big_root, other_leaf};
	bigger.add_transition(0, big_leaf);
	bigger.add_transition(1, {big_leaf}, big_root);
	bigger.add_transition(2, other_leaf);

	Tree unchanged{99};
	CHECK(is_included(smaller, bigger, &unchanged));
	CHECK(unchanged == Tree{99});

	Ta missing_root{};
	const State missing_leaf{missing_root.add_state()};
	missing_root.final.insert(missing_leaf);
	missing_root.add_transition(0, missing_leaf);
	Tree counterexample{99};
	REQUIRE_FALSE(is_included(smaller, missing_root, &counterexample));
	check_counterexample(smaller, missing_root, counterexample);
	CHECK(counterexample == Tree{1, {Tree{0}}});
}

TEST_CASE("mata::ta::is_included bootstraps nullary-only languages and handles empty languages") {
	Ta single{};
	const State single_root{single.add_state()};
	single.final.insert(single_root);
	single.add_transition(0, single_root);

	Ta two_leaves{};
	const State first{two_leaves.add_state()};
	const State second{two_leaves.add_state()};
	two_leaves.final = {first, second};
	two_leaves.add_transition(0, first);
	two_leaves.add_transition(1, second);

	CHECK(is_included(single, two_leaves));
	Tree counterexample{99};
	REQUIRE_FALSE(is_included(two_leaves, single, &counterexample));
	CHECK(counterexample == Tree{1});
	check_counterexample(two_leaves, single, counterexample);

	Ta empty{};
	Ta no_bootstrap{};
	const State unreachable{no_bootstrap.add_state()};
	no_bootstrap.final.insert(unreachable);
	no_bootstrap.add_transition(2, {unreachable}, unreachable);
	CHECK(is_included(empty, single));
	CHECK(is_included(no_bootstrap, empty));
	CHECK(is_included(empty, empty));
}

TEST_CASE("mata::ta::is_included respects repeated ordered children") {
	Ta smaller{};
	const State small_leaf{smaller.add_state()};
	const State small_root{smaller.add_state()};
	smaller.final.insert(small_root);
	smaller.add_transition(0, small_leaf);
	smaller.add_transition(1, {small_leaf, small_leaf}, small_root);

	Ta matching{};
	const State matching_leaf{matching.add_state()};
	const State matching_root{matching.add_state()};
	matching.final.insert(matching_root);
	matching.add_transition(0, matching_leaf);
	matching.add_transition(1, {matching_leaf, matching_leaf}, matching_root);
	CHECK(is_included(smaller, matching));

	Ta swapped{};
	const State left{swapped.add_state()};
	const State right{swapped.add_state()};
	const State swapped_root{swapped.add_state()};
	swapped.final.insert(swapped_root);
	swapped.add_transition(0, left);
	swapped.add_transition(2, right);
	swapped.add_transition(1, {left, right}, swapped_root);

	Tree counterexample{99};
	REQUIRE_FALSE(is_included(smaller, swapped, &counterexample));
	CHECK(counterexample == Tree{1, {Tree{0}, Tree{0}}});
	check_counterexample(smaller, swapped, counterexample);
}

TEST_CASE("mata::ta::is_included unions nondeterministic bigger post-images") {
	Ta smaller{};
	const State small_leaf{smaller.add_state()};
	const State small_root{smaller.add_state()};
	smaller.final.insert(small_root);
	smaller.add_transition(0, small_leaf);
	smaller.add_transition(1, {small_leaf, small_leaf}, small_root);

	Ta bigger{};
	const State first_leaf{bigger.add_state()};
	const State second_leaf{bigger.add_state()};
	const State first_root{bigger.add_state()};
	const State second_root{bigger.add_state()};
	const State nonaccepting{bigger.add_state()};
	bigger.final.insert(second_root);
	bigger.add_transition(0, first_leaf);
	bigger.add_transition(0, second_leaf);
	bigger.add_transition(1, {first_leaf, first_leaf}, first_root);
	bigger.add_transition(1, {second_leaf, second_leaf}, second_root);

	CHECK(is_included(smaller, bigger));
	bigger.final = {nonaccepting};
	Tree counterexample{99};
	REQUIRE_FALSE(is_included(smaller, bigger, &counterexample));
	check_counterexample(smaller, bigger, counterexample);
}

TEST_CASE("mata::ta::is_included treats missing larger rules as an empty post-image") {
	Ta smaller{};
	const State leaf{smaller.add_state()};
	const State root{smaller.add_state()};
	smaller.final.insert(root);
	smaller.add_transition(0, leaf);
	smaller.add_transition(1, {leaf}, root);

	Ta bigger{};
	const State big_leaf{bigger.add_state()};
	bigger.final.insert(big_leaf);
	bigger.add_transition(0, big_leaf);

	Tree counterexample{99};
	REQUIRE_FALSE(is_included(smaller, bigger, &counterexample));
	CHECK(counterexample == Tree{1, {Tree{0}}});
	check_counterexample(smaller, bigger, counterexample);
}

TEST_CASE("mata::ta::is_included checks bad roots before subsumption and survives pruning") {
	Ta leaves{};
	const State leaf_target{leaves.add_state()};
	leaves.final.insert(leaf_target);
	leaves.add_transition(0, leaf_target);
	leaves.add_transition(1, leaf_target);

	Ta one_good_leaf{};
	const State good{one_good_leaf.add_state()};
	one_good_leaf.final.insert(good);
	one_good_leaf.add_transition(0, good);
	Tree leaf_counterexample{99};
	REQUIRE_FALSE(is_included(leaves, one_good_leaf, &leaf_counterexample));
	CHECK(leaf_counterexample == Tree{1});

	Ta smaller{};
	const State child{smaller.add_state()};
	const State root{smaller.add_state()};
	smaller.final.insert(root);
	smaller.add_transition(0, child);
	smaller.add_transition(1, child);
	smaller.add_transition(2, {child}, root);

	Ta bigger{};
	const State big_child{bigger.add_state()};
	const State big_root{bigger.add_state()};
	bigger.final.insert(big_root);
	bigger.add_transition(0, big_child);
	bigger.add_transition(2, {big_child}, big_root);

	Tree counterexample{99};
	REQUIRE_FALSE(is_included(smaller, bigger, &counterexample));
	CHECK(counterexample == Tree{2, {Tree{1}}});
	check_counterexample(smaller, bigger, counterexample);
}

TEST_CASE("mata::ta::is_included handles token bad states and a concrete transducer post tree") {
	const Ta initial{token_initial()};
	const Ta bad{token_bad()};
	Tree counterexample{99};
	REQUIRE_FALSE(is_included(initial, bad, &counterexample));
	check_counterexample(initial, bad, counterexample);

	// nn(t,n) is mapped by ta-token-step.mata to this concrete output tree.
	const Tree post_tree{3, {Tree{0}, Tree{0}}};
	REQUIRE(initial.accepts(Tree{2, {Tree{1}, Tree{0}}}));
	REQUIRE_FALSE(initial.accepts(post_tree));
	REQUIRE_FALSE(bad.accepts(post_tree));

	Ta singleton_post{};
	const State idle{singleton_post.add_state()};
	const State root{singleton_post.add_state()};
	singleton_post.final.insert(root);
	singleton_post.add_transition(0, idle);
	singleton_post.add_transition(3, {idle, idle}, root);
	REQUIRE(singleton_post.accepts(post_tree));
	Tree bad_counterexample{99};
	REQUIRE_FALSE(is_included(singleton_post, bad, &bad_counterexample));
	CHECK(bad_counterexample == post_tree);
	Tree initial_counterexample{99};
	REQUIRE_FALSE(is_included(singleton_post, initial, &initial_counterexample));
	CHECK(initial_counterexample == post_tree);
}

TEST_CASE("mata::ta::is_included instantiates every tuple over repeated child states") {
	constexpr Symbol a{0};
	constexpr Symbol b{1};
	constexpr Symbol f{2};
	constexpr unsigned arity{3};
	constexpr unsigned combinations{1U << arity};
	Ta smaller{};
	const State leaf{smaller.add_state()};
	const State root{smaller.add_state()};
	smaller.final.insert(root);
	smaller.add_transition(a, leaf);
	smaller.add_transition(b, leaf);
	smaller.add_transition(f, {leaf, leaf, leaf}, root);

	// Bit i of a combination selects leaf b at child i. The two leaves give one smaller state two incomparable
	// bigger macrostates, so each mixed tuple of them needs its own visit. Passing @c combinations drops no rule.
	const auto bigger_without{[&](const unsigned missing) {
		Ta bigger{};
		const State big_a{bigger.add_state()};
		const State big_b{bigger.add_state()};
		const State big_root{bigger.add_state()};
		bigger.final.insert(big_root);
		bigger.add_transition(a, big_a);
		bigger.add_transition(b, big_b);
		for (unsigned combination{0}; combination < combinations; ++combination) {
			if (combination == missing) { continue; }
			std::vector<State> children{};
			for (unsigned position{0}; position < arity; ++position) {
				children.push_back(((combination >> position) & 1U) != 0 ? big_b : big_a);
			}
			bigger.add_transition(f, std::move(children), big_root);
		}
		return bigger;
	}};

	CHECK(is_included(smaller, bigger_without(combinations)));
	for (unsigned missing{0}; missing < combinations; ++missing) {
		INFO("missing combination " << missing);
		std::vector<Tree> children{};
		for (unsigned position{0}; position < arity; ++position) {
			children.emplace_back(((missing >> position) & 1U) != 0 ? b : a);
		}
		Tree counterexample{99};
		REQUIRE_FALSE(is_included(smaller, bigger_without(missing), &counterexample));
		CHECK(counterexample == Tree{f, std::move(children)});
	}
}

TEST_CASE("mata::ta::is_included replaces a node dominated after its expansion") {
	constexpr Symbol a{0};
	constexpr Symbol g{1};
	constexpr Symbol f{2};
	Ta smaller{};
	const State leaf{smaller.add_state()};
	const State root{smaller.add_state()};
	smaller.final.insert(root);
	smaller.add_transition(a, leaf);
	smaller.add_transition(g, {leaf}, leaf);
	smaller.add_transition(f, {leaf, leaf}, root);

	// a yields {p1,p2}; expanding it yields g(a) with {p1}, which prunes the expanded node. Only f(p1,p1) is bad.
	Ta bigger{};
	const State p1{bigger.add_state()};
	const State p2{bigger.add_state()};
	const State good{bigger.add_state()};
	const State bad{bigger.add_state()};
	bigger.final.insert(good);
	bigger.add_transition(a, p1);
	bigger.add_transition(a, p2);
	bigger.add_transition(g, {p1}, p1);
	bigger.add_transition(g, {p2}, p1);
	bigger.add_transition(f, {p1, p2}, good);
	bigger.add_transition(f, {p2, p1}, good);
	bigger.add_transition(f, {p2, p2}, good);
	bigger.add_transition(f, {p1, p1}, bad);

	Tree counterexample{99};
	REQUIRE_FALSE(is_included(smaller, bigger, &counterexample));
	const Tree g_leaf{g, {Tree{a}}};
	CHECK(counterexample == Tree{f, {g_leaf, g_leaf}});
	check_counterexample(smaller, bigger, counterexample);

	bigger.add_transition(f, {p1, p1}, good);
	CHECK(is_included(smaller, bigger));
}
