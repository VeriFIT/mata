/** @file
 * @brief A relation whose targets carry a payload (a state plus a weight), through every structural
 *  operation. Nothing in the tree ships one, so without this the @c with_state write paths are
 *  dead code that compiles only for `Target == State`.
 */

#include <catch2/catch_test_macros.hpp>
#include <compare>
#include <concepts>
#include <string>
#include <utility>
#include <vector>

#include "mata/core/automaton.hh"
#include "mata/core/concepts.hh"
#include "mata/core/delta.hh"

using namespace mata;

/// Not in an anonymous namespace: an explicit instantiation over an internal-linkage type trips
///  `-Werror=unused-function` on every member the test does not call.
struct Weighted {
	State to{};
	int weight{};
	auto operator<=>(const Weighted&) const = default; ///< The innermost post is an ordered vector.
};

/// The whole of what a payload has to supply.
template <> struct mata::TargetTraits<Weighted> {
	using State = mata::State;
	static State state_of(const Weighted& target) { return target.to; }
	static Weighted with_state(const Weighted& target, const State state) { return {state, target.weight}; }
};

using WeightedTargets = posts::StateTargets<Weighted>;
using WeightedDelta = posts::RelationOf<Symbol, WeightedTargets>;

static_assert(DeltaLike<WeightedDelta>, "a payload relation must satisfy the same contract as a plain one");
static_assert(std::same_as<WeightedDelta::Target, Weighted>);
static_assert(std::same_as<WeightedDelta::State, State>, "the state is read off the target through the traits");
static_assert(WeightedDelta::key_arity == 1);

/// The identity traits stay the identity: a bare state renamed is just the new state.
static_assert(TargetTraits<State>::state_of(5) == 5);
static_assert(TargetTraits<State>::with_state(5, 7) == 7);

/// A small payload is passed by value, one that owns memory by reference (type-level check only).
static_assert(std::same_as<WeightedDelta::TargetArg, const Weighted>);
struct Heavy {
	State to{};
	std::string label{};
	auto operator<=>(const Heavy&) const = default;
};
template <> struct mata::TargetTraits<Heavy> {
	using State = mata::State;
	static State state_of(const Heavy& target) { return target.to; }
	static Heavy with_state(const Heavy& target, const State state) { return {state, target.label}; }
};
static_assert(std::same_as<posts::RelationOf<Symbol, posts::StateTargets<Heavy>>::TargetArg, const Heavy&>);

struct WeightedAutomaton : AutomatonBase<WeightedDelta> {
	using Run = mata::Run;
	WeightedAutomaton() = default; ///< So `aut{}` is not aggregate-init through the explicit base ctor.
	/// What a path reads is not structural; this automaton reads nothing.
	std::pair<Run, bool> get_word_for_path(const Run& run) const { return {run, true}; }
};

/// Every member has to instantiate over a payload target, not only the ones a test happens to call.
template class mata::AutomatonBase<WeightedDelta>;

TEST_CASE("mata::AutomatonBase over a relation whose targets carry a payload") {
	// 0 -a-> 1(w5), 1 -b-> 2(w7), 1 -b-> 2(w9), 2 -d-> 2(w0), and 3 -c-> 0(w1) reaching in from the side.
	WeightedAutomaton aut{};
	aut.delta.add(0, 'a', Weighted{1, 5});
	aut.delta.add(1, 'b', Weighted{2, 7});
	aut.delta.add(1, 'b', Weighted{2, 9});
	aut.delta.add(2, 'd', Weighted{2, 0});
	aut.delta.add(3, 'c', Weighted{0, 1});
	aut.initial.insert(0);
	aut.final.insert(2);

	SECTION("the keyed writes take the payload, and keep it") {
		CHECK(aut.delta.contains(1, 'b', Weighted{2, 7}));
		CHECK(aut.delta.contains(1, 'b', Weighted{2, 9}));
		CHECK_FALSE(aut.delta.contains(1, 'b', Weighted{2, 8}));
		CHECK(aut.delta.num_of_transitions() == 5);

		aut.delta.remove(1, 'b', Weighted{2, 7});
		CHECK_FALSE(aut.delta.contains(1, 'b', Weighted{2, 7}));
		CHECK(aut.delta.contains(1, 'b', Weighted{2, 9}));
		CHECK(aut.delta.num_of_transitions() == 4);
	}

	SECTION("the reads project through state_of") {
		CHECK(aut.get_reachable_states() == StateSet{0, 1, 2});
		const BoolVector useful{aut.get_useful_states()};
		CHECK((useful[0] && useful[1] && useful[2] && !useful[3]));
		CHECK(aut.distances_from_initial()[2] == 2);
		CHECK_FALSE(aut.is_lang_empty());

		WeightedAutomaton::Run cex{};
		CHECK_FALSE(aut.is_lang_empty(&cex));
		CHECK(cex.path == std::vector<State>{0, 1, 2});
	}

	SECTION("a self-loop is found by the state it denotes, not by the target") {
		CHECK(aut.delta.has_self_loop(2));
		CHECK_FALSE(aut.delta.has_self_loop(1));
		CHECK_FALSE(aut.is_acyclic());
		aut.delta.remove(2, 'd', Weighted{2, 0});
		CHECK(aut.is_acyclic());
	}

	SECTION("reverting turns the source into a target and carries the weight across") {
		// Answered by reverting: 3 reaches 0, which reaches the final state.
		CHECK(aut.get_terminating_states() == StateSet{0, 1, 2, 3});
		const std::vector<State> to_final{aut.distances_to_final()};
		CHECK(to_final[0] == 2);
		CHECK(to_final[1] == 1);
		CHECK(to_final[2] == 0);
		CHECK(to_final[3] == 3);
	}

	SECTION("trimming renumbers the state inside the target and keeps the weight") {
		aut.trim();
		CHECK(aut.num_of_states() == 3);
		CHECK(aut.delta.num_of_transitions() == 4); // 3 -c-> 0 went with state 3.
		CHECK(aut.delta.contains(0, 'a', Weighted{1, 5}));
		CHECK(aut.delta.contains(1, 'b', Weighted{2, 7}));
		CHECK(aut.delta.contains(1, 'b', Weighted{2, 9}));
		CHECK(aut.delta.contains(2, 'd', Weighted{2, 0}));

		// Now with a hole in the numbering, so the renaming is not the identity.
		WeightedAutomaton gapped{};
		gapped.delta.add(0, 'a', Weighted{4, 11});
		gapped.delta.add(4, 'b', Weighted{4, 13});
		gapped.initial.insert(0);
		gapped.final.insert(4);
		gapped.trim();
		CHECK(gapped.num_of_states() == 2);
		CHECK(gapped.delta.contains(0, 'a', Weighted{1, 11}));
		CHECK(gapped.delta.contains(1, 'b', Weighted{1, 13}));
		CHECK(gapped.final.contains(1));
	}

	SECTION("structural identity compares the payload too") {
		WeightedAutomaton same{aut};
		CHECK(aut.is_identical(same));
		same.delta.remove(1, 'b', Weighted{2, 9});
		same.delta.add(1, 'b', Weighted{2, 10});
		CHECK_FALSE(aut.is_identical(same));
	}
}
