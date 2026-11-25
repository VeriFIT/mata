/** @file
 * @brief @c SuccessorCursor at every arity, cross-checked against the recursive walk.
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <vector>

#include "mata/core/automaton.hh"
#include "mata/core/delta.hh"
#include "mata/relation.hh"
#include "mata/utils/ord-vector.hh"

using namespace mata;

namespace {

// Arity 2: state -> key -> key -> targets.
using Targets = utils::OrdVector<State>;
using Inner2 = posts::PostChain<Symbol, Targets>;
using Post2 = posts::PostChain<Symbol, Symbol, Targets>;
// Arity 3.
using Inner3a = posts::PostChain<Symbol, Targets>;
using Inner3b = posts::PostChain<Symbol, Symbol, Targets>;
using Post3 = posts::PostChain<Symbol, Symbol, Symbol, Targets>;
// Arity 4.
using Post4 = posts::PostChain<Symbol, Symbol, Symbol, Symbol, Targets>;

// What the recursive walk yields.
template <typename Post> std::vector<State> expected(const Post& post) {
	std::vector<State> out;
	posts::visit_targets(post, [&out](const State t) { out.push_back(t); });
	return out;
}

// What the cursor yields.
template <typename Post> std::vector<State> from_cursor(const Post& post) {
	std::vector<State> out;
	const posts::SuccessorCursor<Post> cursor{post};
	for (auto it{cursor.begin()}; it != std::default_sentinel; ++it) { out.push_back(*it); }
	return out;
}

// The cursor stopped and resumed after every step, as Tarjan uses it.
template <typename Post> std::vector<State> from_resumed_cursor(const Post& post) {
	std::vector<State> out;
	const posts::SuccessorCursor<Post> cursor{post};
	auto it{cursor.begin()};
	while (it != std::default_sentinel) {
		auto saved{it}; // copy the position out, as TarjanNodeData does
		out.push_back(*saved);
		++saved;
		it = saved; // and put it back
	}
	return out;
}

} // anonymous namespace.

static_assert(Post2::key_arity == 2, "arity is computed from the nesting");
static_assert(Post3::key_arity == 3, "arity is computed from the nesting");
static_assert(Post4::key_arity == 4, "arity is computed from the nesting");
static_assert(StatePost::key_arity == 1);

TEST_CASE("mata::SuccessorCursor agrees with the recursive walk") {
	SECTION("arity 1: the shipping cursor") {
		StatePost post{};
		post.push_back(SymbolPost{1, StateSet{7, 9}});
		post.push_back(SymbolPost{4, StateSet{2}});
		CHECK(from_cursor(post) == expected(post));
		CHECK(from_cursor(post) == std::vector<State>{7, 9, 2});
		CHECK(from_resumed_cursor(post) == expected(post));
	}

	SECTION("arity 1: an empty post yields nothing") {
		const StatePost post{};
		CHECK(from_cursor(post).empty());
		CHECK(from_cursor(post) == expected(post));
	}

	SECTION("arity 1: a post whose only entry has no targets") {
		StatePost post{};
		post.push_back(SymbolPost{1, StateSet{}});
		CHECK(from_cursor(post).empty());
		CHECK(from_cursor(post) == expected(post));
	}

	SECTION("arity 2: the carry cascades through both levels") {
		Post2 post{};
		Inner2 inner_a{};
		inner_a.push_back({1, Targets{3, 4}});
		inner_a.push_back({2, Targets{5}});
		Inner2 inner_b{};
		inner_b.push_back({7, Targets{8}});
		post.push_back({10, inner_a});
		post.push_back({20, inner_b});
		CHECK(from_cursor(post) == expected(post));
		CHECK(from_cursor(post) == std::vector<State>{3, 4, 5, 8});
		CHECK(from_resumed_cursor(post) == expected(post));
	}

	SECTION("arity 2: empty inner posts and empty target sets are skipped") {
		Post2 post{};
		Inner2 empty_inner{}; // an entry whose nested post is empty
		Inner2 inner_with_empty{};
		inner_with_empty.push_back({1, Targets{}}); // an entry whose targets are empty
		inner_with_empty.push_back({2, Targets{42}});
		Inner2 tail{};
		tail.push_back({3, Targets{99}});
		post.push_back({10, empty_inner});
		post.push_back({20, inner_with_empty});
		post.push_back({30, tail});
		CHECK(from_cursor(post) == expected(post));
		CHECK(from_cursor(post) == std::vector<State>{42, 99});
		CHECK(from_resumed_cursor(post) == expected(post));
	}

	SECTION("arity 2: everything empty") {
		Post2 post{};
		post.push_back({10, Inner2{}});
		post.push_back({20, Inner2{}});
		CHECK(from_cursor(post).empty());
		CHECK(from_cursor(post) == expected(post));
	}

	SECTION("arity 3: the carry cascades through all three levels") {
		Post3 post{};
		Inner3a leaf_a{};
		leaf_a.push_back({1, Targets{11, 12}});
		Inner3a leaf_b{};
		leaf_b.push_back({2, Targets{13}});
		Inner3b mid_a{};
		mid_a.push_back({5, leaf_a});
		mid_a.push_back({6, leaf_b});
		Inner3a leaf_c{};
		leaf_c.push_back({3, Targets{14}});
		Inner3b mid_b{};
		mid_b.push_back({7, leaf_c});
		post.push_back({100, mid_a});
		post.push_back({200, mid_b});
		CHECK(from_cursor(post) == expected(post));
		CHECK(from_cursor(post) == std::vector<State>{11, 12, 13, 14});
		CHECK(from_resumed_cursor(post) == expected(post));
	}

	SECTION("arity 3: holes at every level") {
		Post3 post{};
		Inner3b empty_mid{}; // nothing under this level-0 key
		Inner3b mid_with_empty_leaf{};
		mid_with_empty_leaf.push_back({1, Inner3a{}}); // nothing under this level-1 key
		Inner3a leaf_with_empty{};
		leaf_with_empty.push_back({2, Targets{}}); // no targets under this level-2 key
		leaf_with_empty.push_back({3, Targets{77}});
		mid_with_empty_leaf.push_back({4, leaf_with_empty});
		post.push_back({10, empty_mid});
		post.push_back({20, mid_with_empty_leaf});
		CHECK(from_cursor(post) == expected(post));
		CHECK(from_cursor(post) == std::vector<State>{77});
		CHECK(from_resumed_cursor(post) == expected(post));
	}

	SECTION("arity 3: a single target, so every level exhausts at once") {
		Post3 post{};
		Inner3a leaf{};
		leaf.push_back({1, Targets{5}});
		Inner3b mid{};
		mid.push_back({2, leaf});
		post.push_back({3, mid});
		CHECK(from_cursor(post) == std::vector<State>{5});
		CHECK(from_cursor(post) == expected(post));
		CHECK(from_resumed_cursor(post) == expected(post));
	}

	SECTION("arity 4: holes at every depth, and the key path") {
		Inner3a leaf{};
		leaf.push_back({1, Targets{}}); // no targets under this key
		leaf.push_back({2, Targets{21, 22}});
		Inner3b mid{};
		mid.push_back({3, Inner3a{}}); // nothing under this key
		mid.push_back({4, leaf});
		Post3 upper{};
		upper.push_back({5, Inner3b{}}); // nothing under this key
		upper.push_back({6, mid});
		Inner3a last_leaf{};
		last_leaf.push_back({7, Targets{23}});
		Inner3b last_mid{};
		last_mid.push_back({8, last_leaf});
		Post3 last_upper{};
		last_upper.push_back({9, last_mid});
		Post4 post{};
		post.push_back({10, Post3{}}); // nothing under this key
		post.push_back({20, upper});
		post.push_back({30, last_upper});
		CHECK(from_cursor(post) == expected(post));
		CHECK(from_cursor(post) == std::vector<State>{21, 22, 23});
		CHECK(from_resumed_cursor(post) == expected(post));

		const posts::SuccessorCursor<Post4> cursor{post};
		auto it{cursor.begin()};
		CHECK(it.keys() == std::tuple<Symbol, Symbol, Symbol, Symbol>{20, 6, 4, 2});
		++it;
		++it;
		CHECK(it.keys() == std::tuple<Symbol, Symbol, Symbol, Symbol>{30, 9, 8, 7});
	}
}

TEST_CASE("mata::SuccessorCursor and the walks agree on counts") {
	Post2 post{};
	for (Symbol a{0}; a < 4; ++a) {
		Inner2 inner{};
		for (Symbol b{0}; b < 3; ++b) {
			Targets targets{};
			for (State t{0}; t < (a + b) % 3; ++t) { targets.insert(t); } // some runs are empty
			inner.push_back({b, targets});
		}
		post.push_back({a, inner});
	}
	CHECK(from_cursor(post) == expected(post));
	CHECK(posts::count_targets(post) == expected(post).size());
	CHECK(post.num_of_moves() == expected(post).size());

	// any_target must agree with a linear scan of the same walk.
	const std::vector<State> all{expected(post)};
	for (State probe{0}; probe < 5; ++probe) {
		const bool by_walk{std::find(all.begin(), all.end(), probe) != all.end()};
		CHECK(posts::any_target(post, [probe](const State t) { return t == probe; }) == by_walk);
		CHECK(post.has_target(probe) == by_walk);
	}
}

// An automaton over a relation deeper than two.
namespace {

using Delta2 = posts::DeltaBase<Post2>;
using Delta3 = posts::DeltaBase<Post3>;
using Delta4 = posts::DeltaBase<Post4>;

template <typename A>
concept AnswersBackwards = requires(const A a) {
	a.get_terminating_states();
	a.distances_to_final();
};
template <typename A>
concept AnswersForwards = requires(const A a) {
	a.get_reachable_states();
	a.distances_from_initial();
	a.get_useful_states();
	a.is_acyclic();
};

} // anonymous namespace.

static_assert(DeltaLike<Delta2>, "a depth-3 relation must satisfy the contract");
static_assert(DeltaLike<Delta3>, "a depth-4 relation must satisfy the contract");
static_assert(DeltaLike<Delta4>, "a depth-5 relation must satisfy the contract");
static_assert(Delta2::key_arity == 2 && Delta3::key_arity == 3 && Delta4::key_arity == 4);

// Instantiates every member, so a member needing more than DeltaLike fails here.
template class mata::AutomatonBase<Delta2>;
template class mata::AutomatonBase<Delta3>;
template class mata::AutomatonBase<Delta4>;

// Forward and backward operations, reverting included, exist at every arity.
static_assert(AnswersForwards<mata::AutomatonBase<Delta2>>);
static_assert(AnswersForwards<mata::AutomatonBase<Delta3>>);
static_assert(AnswersForwards<mata::AutomatonBase<Delta4>>);
static_assert(AnswersBackwards<mata::AutomatonBase<Delta2>>);
static_assert(AnswersBackwards<mata::AutomatonBase<Delta3>>);
static_assert(AnswersBackwards<mata::AutomatonBase<Delta4>>);
static_assert(AnswersBackwards<mata::AutomatonBase<mata::Delta>>);

TEST_CASE("mata::AutomatonBase over an arity-2 relation") {
	mata::AutomatonBase<Delta2> aut{};
	// 0 --(1,1)--> 1 --(2,2)--> 2, with 2 final and an unreachable 3 --(9,9)--> 3.
	auto make = [](Symbol inner_key, State target) {
		Inner2 inner{};
		inner.push_back({inner_key, Targets{target}});
		return inner;
	};
	aut.delta.allocate(4);
	aut.delta.mutable_state_post(0).push_back({1, make(1, 1)});
	aut.delta.mutable_state_post(1).push_back({2, make(2, 2)});
	aut.delta.mutable_state_post(3).push_back({9, make(9, 3)});
	aut.initial.insert(0);
	aut.final.insert(2);

	SECTION("the walks reach through both key levels") {
		CHECK(aut.get_reachable_states() == mata::StateSet{0, 1, 2});
		const std::vector<State> from_initial{aut.distances_from_initial()};
		CHECK(from_initial[0] == 0);
		CHECK(from_initial[1] == 1);
		CHECK(from_initial[2] == 2);
	}

	SECTION("Tarjan drives through the arity-2 cursor") {
		CHECK(aut.get_useful_states() == mata::BoolVector{1, 1, 1, 0});
		CHECK(aut.is_acyclic());
		aut.initial.insert(3); // reach the self-loop
		CHECK(!aut.is_acyclic());
	}

	SECTION("trimming recurses through every level") {
		mata::StateRenaming renaming{};
		aut.trim(&renaming);
		CHECK(aut.num_of_states() == 3);
		CHECK(aut.get_reachable_states() == mata::StateSet{0, 1, 2});
		CHECK(aut.delta.num_of_transitions() == 2);
	}

	SECTION("reverting works at arity 2, observed through what it serves") {
		CHECK(aut.get_terminating_states() == mata::StateSet{0, 1, 2});
		const std::vector<State> to_final{aut.distances_to_final()};
		CHECK(to_final[0] == 2);
		CHECK(to_final[1] == 1);
		CHECK(to_final[2] == 0);
		// The unreachable self-loop at 3 reaches no final state.
		CHECK(!aut.get_terminating_states().contains(3));
	}

	SECTION("for_each_move yields one key per level, then the target") {
		std::vector<std::tuple<State, Symbol, Symbol, State>> moves;
		for (State q{0}; q < aut.num_of_states(); ++q) {
			aut.delta.for_each_move(q, [&](const Symbol k0, const Symbol k1, const State t) {
				moves.emplace_back(q, k0, k1, t);
			});
		}
		CHECK(moves.size() == 3);
		CHECK(std::ranges::find(moves, std::tuple<State, Symbol, Symbol, State>{0, 1, 1, 1}) != moves.end());
		CHECK(std::ranges::find(moves, std::tuple<State, Symbol, Symbol, State>{1, 2, 2, 2}) != moves.end());
		CHECK(std::ranges::find(moves, std::tuple<State, Symbol, Symbol, State>{3, 9, 9, 3}) != moves.end());
	}

	SECTION("structural equality descends past the key") {
		mata::AutomatonBase<Delta2> same{aut};
		CHECK(aut.is_identical(same));
		same.delta.add(1, 2, 2, 99);
		CHECK(!aut.is_identical(same));
	}
}
