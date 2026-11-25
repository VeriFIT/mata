/** @file
 * @brief The extensibility contract, exercised by a relation defined outside @c mata.
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include "mata/core/automaton.hh"
#include "mata/core/concepts.hh"
#include "mata/types.hh"

namespace tiny_delta {

// A cursor over the successors of one state. Its iterator points into the relation, so it outlives the cursor.
class TinyCursor {
  public:
	class const_iterator {
	  public:
		const mata::State* target_it{nullptr};
		const mata::State* target_end{nullptr};

		const mata::State& operator*() const { return *target_it; }
		const_iterator& operator++() {
			++target_it;
			return *this;
		}
		bool operator==(std::default_sentinel_t) const { return target_it == target_end; }
	};

	explicit TinyCursor(const std::vector<mata::State>& successors) : successors_{&successors} {}

	const_iterator begin() const { return {successors_->data(), successors_->data() + successors_->size()}; }
	std::default_sentinel_t end() const { return std::default_sentinel; }

  private:
	const std::vector<mata::State>* successors_;
};

// A relation written outside mata, with no keys and no post: each state's sorted successors.
class TinyDelta {
  public:
	using Target = mata::State;
	using State = mata::State;

	static State state_of(const Target target) { return target; }

	TinyDelta() = default;
	explicit TinyDelta(const size_t num_of_states) : successors_(num_of_states) {}

	bool operator==(const TinyDelta& other) const = default;

	size_t num_of_states() const { return successors_.size(); }
	void allocate(const size_t num_of_states) {
		if (num_of_states > successors_.size()) { successors_.resize(num_of_states); }
	}
	void clear() { successors_.clear(); }

	template <typename F> void for_each_successor(const State source, F&& handler) const {
		for (const State target : successors_of(source)) { handler(target); }
	}

	TinyCursor successor_cursor(const State source) const { return TinyCursor{successors_of(source)}; }

	bool has_self_loop(const State source) const { return std::ranges::binary_search(successors_of(source), source); }

	// Not required by the contract: how this relation is written to.
	void add(const State source, const State target) {
		allocate(std::max(source, target) + 1);
		std::vector<State>& successors{successors_[source]};
		const auto it{std::ranges::lower_bound(successors, target)};
		if (it == successors.end() || *it != target) { successors.insert(it, target); }
	}
	bool has_transitions() const {
		return std::ranges::any_of(successors_, [](const std::vector<State>& s) { return !s.empty(); });
	}

	TinyDelta& defragment(const mata::BoolVector& is_staying, const std::vector<State>& renaming) {
		const auto stays = [&](const State q) { return q < is_staying.size() && is_staying[q] != 0; };
		TinyDelta result{};
		result.allocate(static_cast<size_t>(std::ranges::count_if(is_staying, [](const auto b) { return b != 0; })));
		for (State source{0}; source < num_of_states(); ++source) {
			if (!stays(source)) { continue; }
			for (const State target : successors_[source]) {
				if (stays(target)) { result.add(renaming[source], renaming[target]); }
			}
		}
		*this = std::move(result);
		return *this;
	}

  private:
	// A state of the automaton that the relation has no successors stored for has none.
	const std::vector<State>& successors_of(const State source) const {
		static const std::vector<State> none{};
		return source < successors_.size() ? successors_[source] : none;
	}

	std::vector<std::vector<State>> successors_{};
};

} // namespace tiny_delta.

using namespace tiny_delta;

static_assert(mata::DeltaLike<TinyDelta>, "A plain adjacency list written outside mata must satisfy the contract.");

using TinyAutomaton = mata::AutomatonBase<TinyDelta>;

static_assert(std::same_as<TinyAutomaton::State, mata::State>);
static_assert(std::same_as<TinyAutomaton::Target, mata::State>);
static_assert(std::same_as<TinyAutomaton::DeltaType, TinyDelta>);

// Instantiates every member, so a member needing more than DeltaLike fails here.
template class mata::AutomatonBase<TinyDelta>;

// A leaf over the third-party relation, as Nfa is over Delta: it publishes the protected writes and has runs.
struct TinyLeaf : mata::AutomatonBase<TinyDelta> {
	using Run = mata::Run;
	using AutomatonBase::add_state;
	using AutomatonBase::clear;

	TinyLeaf() = default; // So that `aut{}` is not aggregate initialisation.
	std::pair<Run, bool> get_word_for_path(const Run& run) const { return {run, true}; }
};
static_assert(mata::AutomatonWithRuns<TinyLeaf>);

TEST_CASE("mata::AutomatonBase over a relation defined outside mata") {
	// 0 -> 1 -> 2 (final), plus a dead-end 0 -> 3 and an unreachable 4 -> 4.
	TinyAutomaton aut{};
	aut.delta.add(0, 1);
	aut.delta.add(0, 3);
	aut.delta.add(1, 2);
	aut.delta.add(4, 4);
	aut.initial.insert(0);
	aut.final.insert(2);

	SECTION("the presizing constructor reaches the relation") {
		const TinyAutomaton presized{7};
		CHECK(presized.num_of_states() == 7);
		CHECK(!presized.delta.has_transitions());
	}

	SECTION("the state space is read off the relation") {
		CHECK(aut.num_of_states() == 5);
		CHECK(aut.is_state(4));
		CHECK(!aut.is_state(5));
	}

	SECTION("a state past the relation, initial and final, is a state of the automaton") {
		aut.initial.insert(7);
		aut.final.insert(7);
		CHECK(aut.delta.num_of_states() == 5);
		CHECK(aut.num_of_states() == 8);
		CHECK(aut.get_reachable_states() == mata::StateSet{0, 1, 2, 3, 7});
		CHECK(aut.get_useful_states() == mata::BoolVector{1, 1, 1, 0, 0, 0, 0, 1});

		mata::StateRenaming renaming{};
		aut.trim(&renaming);
		CHECK(aut.num_of_states() == 4);
		CHECK(renaming == mata::StateRenaming{{0, 0}, {1, 1}, {2, 2}, {7, 3}});
		CHECK(aut.initial.contains(3));
		CHECK(aut.final.contains(3));
	}

	SECTION("reachability walks the third-party relation") {
		CHECK(aut.get_reachable_states() == mata::StateSet{0, 1, 2, 3});
	}

	SECTION("terminating states, answered over the predecessors") {
		CHECK(aut.get_terminating_states() == mata::StateSet{0, 1, 2});
	}

	SECTION("distances, in both directions") {
		const std::vector<mata::State> from_initial{aut.distances_from_initial()};
		CHECK(from_initial[0] == 0);
		CHECK(from_initial[1] == 1);
		CHECK(from_initial[2] == 2);
		CHECK(from_initial[3] == 1);

		const std::vector<mata::State> to_final{aut.distances_to_final()};
		CHECK(to_final[0] == 2);
		CHECK(to_final[1] == 1);
		CHECK(to_final[2] == 0);
	}

	SECTION("Tarjan finds the useful states, and only those") {
		CHECK(aut.get_useful_states() == mata::BoolVector{1, 1, 1, 0, 0});
	}

	SECTION("acyclicity sees the self-loop only once it is reachable") {
		CHECK(aut.is_acyclic());
		aut.initial.insert(4);
		CHECK(!aut.is_acyclic());
	}

	SECTION("structural comparison compares the relations") {
		TinyAutomaton same{aut};
		CHECK(aut.is_identical(same));
		same.delta.add(2, 0);
		CHECK(!aut.is_identical(same));
	}

	SECTION("trimming drops the dead end and the unreachable self-loop") {
		mata::StateRenaming renaming{};
		aut.trim(&renaming);
		CHECK(aut.num_of_states() == 3);
		CHECK(renaming == mata::StateRenaming{{0, 0}, {1, 1}, {2, 2}});
		CHECK(aut.get_reachable_states() == mata::StateSet{0, 1, 2});
		CHECK(aut.get_useful_states() == mata::BoolVector{1, 1, 1});
	}
}

TEST_CASE("mata::AutomatonBase over a relation defined outside mata, through a leaf") {
	// 0 -> 1 -> 2 (final), and the shortcut 0 -> 2.
	TinyLeaf aut{};
	aut.delta.add(0, 1);
	aut.delta.add(1, 2);
	aut.delta.add(0, 2);
	aut.initial.insert(0);
	aut.final.insert(2);

	SECTION("add_state grows the relation") {
		CHECK(aut.add_state() == 3);
		CHECK(aut.delta.num_of_states() == 4);
		CHECK(aut.add_state(6) == 6);
		CHECK(aut.delta.num_of_states() == 7);
		CHECK(aut.add_state(1) == 1);
		CHECK(aut.delta.num_of_states() == 7);
	}

	SECTION("clear empties the relation and both state sets") {
		aut.clear();
		CHECK(aut.num_of_states() == 0);
		CHECK(aut.initial.empty());
		CHECK(aut.final.empty());
		CHECK(!aut.delta.has_transitions());
	}

	SECTION("emptiness with a counter-example returns a shortest accepting path") {
		TinyLeaf::Run cex{};
		CHECK(!aut.is_lang_empty(&cex));
		CHECK(cex.path == std::vector<mata::State>{0, 2});

		aut.final.clear();
		CHECK(aut.is_lang_empty(&cex));
	}
}
