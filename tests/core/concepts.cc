/** @file
 * @brief The extensibility contract, exercised by a relation defined outside @c mata.
 *
 * @c TinyDelta below shares nothing with @c mata::Delta: its own posts, its own storage, its own
 *  cursor. It is here so that the concepts in @c mata/core/concepts.hh are checked against a second
 *  implementation rather than only against the one they were extracted from -- a concept satisfied
 *  by exactly one type says very little.
 *
 * @c TinyDelta implements the whole of @c mata::DeltaLike and nothing more, so what is exercised
 *  here is exactly the claim the concept makes: satisfy this much, and every structural operation
 *  works. Anything @c mata::AutomatonBase needs that the concept does not ask for turns up as a
 *  compile error in this file.
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include "mata/core/automaton.hh"
#include "mata/core/concepts.hh"

namespace tiny_delta {

/// The innermost post: the targets reachable once the key has been supplied.
struct TinyTargets {
	using Target = mata::State;
	static constexpr size_t key_arity{0};
	static constexpr bool sorted_by_target{true};

	std::vector<Target> targets{};

	bool operator==(const TinyTargets& other) const = default;
	auto begin() const { return targets.begin(); }
	auto end() const { return targets.end(); }
	bool empty() const { return targets.empty(); }
	size_t size() const { return targets.size(); }
	bool is_sorted() const { return std::ranges::is_sorted(targets); }
};

/// One entry of a post: a single key, and the targets under it.
struct TinyEntry {
	using Key = unsigned;
	using Nested = TinyTargets;

	Key stored_key{};
	Nested stored_nested{};

	bool operator==(const TinyEntry& other) const = default;
	Key key() const { return stored_key; }
	const Nested& nested() const { return stored_nested; }
};

/// One post: the ordered map from a key to the targets under it.
struct TinyPost {
	using Entry = TinyEntry;
	using Key = TinyEntry::Key;
	using Nested = TinyEntry::Nested;
	using Target = TinyTargets::Target;
	static constexpr size_t key_arity{Nested::key_arity + 1};
	static constexpr bool sorted_by_key{true};

	std::vector<Entry> entries{};

	bool operator==(const TinyPost& other) const = default;
	auto begin() const { return entries.begin(); }
	auto end() const { return entries.end(); }
	bool empty() const { return entries.empty(); }
	size_t size() const { return entries.size(); }
	bool is_sorted() const {
		return std::ranges::is_sorted(entries, {}, [](const Entry& e) { return e.key(); });
	}
};

/// A resumable cursor over the successors of one state. Holds no reference to the cursor that
///  produced it, so a temporary cursor may be discarded once @c begin() has been called.
class TinyCursor {
  public:
	class const_iterator {
	  public:
		std::vector<TinyEntry>::const_iterator entry_it{}, entry_end{};
		const mata::State* target_it{nullptr};
		const mata::State* target_end{nullptr};

		void load_targets() {
			const std::vector<mata::State>& t{entry_it->nested().targets};
			target_it = t.data();
			target_end = target_it + t.size();
		}
		/// Restore the invariant "positioned on a target, or exhausted".
		void seek() {
			while (target_it == target_end) {
				if (++entry_it == entry_end) {
					target_it = target_end = nullptr;
					return;
				}
				load_targets();
			}
		}
		const mata::State& operator*() const { return *target_it; }
		const_iterator& operator++() {
			if (++target_it == target_end) { seek(); }
			return *this;
		}
		bool operator==(std::default_sentinel_t) const { return entry_it == entry_end; }
	};

	explicit TinyCursor(const TinyPost& post) : post_{&post} {}

	const_iterator begin() const {
		const_iterator it;
		it.entry_it = post_->entries.begin();
		it.entry_end = post_->entries.end();
		if (it.entry_it == it.entry_end) { return it; }
		it.load_targets();
		it.seek();
		return it;
	}
	std::default_sentinel_t end() const { return std::default_sentinel; }

  private:
	const TinyPost* post_;
};

/// A transition relation: state -> key -> targets. Shares no code with @c mata::Delta.
class TinyDelta {
  public:
	using PostType = TinyPost;
	using Target = TinyPost::Target;
	using State = mata::TargetTraits<Target>::State;
	using Key = TinyPost::Key;
	static constexpr size_t key_arity{TinyPost::key_arity};

	static State state_of(const Target& target) { return mata::TargetTraits<Target>::state_of(target); }

	TinyDelta() = default;
	/// Presized in one step, which is why @c mata::DeltaLike asks for a constructor rather than
	///  routing the presizing through @c allocate().
	explicit TinyDelta(const size_t num_of_states) : posts_(num_of_states) {}

	bool operator==(const TinyDelta& other) const = default;

	size_t num_of_states() const { return posts_.size(); }
	bool empty() const {
		return std::ranges::all_of(posts_, [](const TinyPost& p) { return p.empty(); });
	}
	void allocate(const size_t num_of_states) {
		if (num_of_states > posts_.size()) { posts_.resize(num_of_states); }
	}
	void clear() { posts_.clear(); }

	template <typename F> void for_each_successor(const State source, F&& handler) const {
		for (const TinyEntry& entry : post_of(source).entries) {
			for (const Target& target : entry.nested()) { handler(target); }
		}
	}

	template <typename F> void for_each_move(const State source, F&& handler) const {
		for (const TinyEntry& entry : post_of(source).entries) {
			for (const Target& target : entry.nested()) { handler(entry.key(), target); }
		}
	}

	TinyCursor successor_cursor(const State source) const { return TinyCursor{post_of(source)}; }

	bool has_self_loop(const State source) const {
		bool found{false};
		for_each_successor(source, [&](const Target& target) { found = found || state_of(target) == source; });
		return found;
	}

	/// Keeps the keys of a post, and the targets under each, sorted.
	void add(const State source, const Key key, const State target) {
		allocate(std::max(source, target) + 1);
		std::vector<TinyEntry>& entries{posts_[source].entries};
		auto it{std::ranges::lower_bound(entries, key, {}, [](const TinyEntry& e) { return e.key(); })};
		if (it == entries.end() || it->key() != key) { it = entries.insert(it, TinyEntry{key, {}}); }
		std::vector<State>& targets{it->stored_nested.targets};
		const auto target_it{std::ranges::lower_bound(targets, target)};
		if (target_it == targets.end() || *target_it != target) { targets.insert(target_it, target); }
	}

	/// Drop the states @p is_staying rejects and renumber the rest through @p renaming.
	TinyDelta& defragment(const mata::BoolVector& is_staying, const std::vector<State>& renaming) {
		const auto stays = [&](const State q) { return q < is_staying.size() && is_staying[q] != 0; };
		TinyDelta result{};
		result.allocate(static_cast<size_t>(std::ranges::count_if(is_staying, [](const auto b) { return b != 0; })));
		for (State source{0}; source < num_of_states(); ++source) {
			if (!stays(source)) { continue; }
			for_each_move(source, [&](const Key key, const State target) {
				if (stays(target)) { result.add(renaming[source], key, renaming[target]); }
			});
		}
		*this = std::move(result);
		return *this;
	}

  private:
	static const TinyPost& empty_post() {
		static const TinyPost empty{};
		return empty;
	}
	const TinyPost& post_of(const State source) const {
		return source < posts_.size() ? posts_[source] : empty_post();
	}

	std::vector<TinyPost> posts_{};
};

} // namespace tiny_delta.

using namespace tiny_delta;

// The post protocol, checked against an implementation that is not mata::Delta's.
static_assert(mata::TargetSetLike<TinyTargets>, "TinyTargets must be the innermost post.");
static_assert(mata::PostEntryLike<TinyEntry>, "TinyEntry must be TinyPost's entry type.");
static_assert(mata::PostLike<TinyPost>, "TinyPost must be one post of the relation.");

static_assert(mata::DeltaLike<TinyDelta>, "A relation written outside mata must be able to satisfy the contract.");

/// The deliverable: a third party's relation gets the structural operations unchanged.
using TinyAutomaton = mata::AutomatonBase<TinyDelta>;

static_assert(std::same_as<TinyAutomaton::State, mata::State>);
static_assert(std::same_as<TinyAutomaton::Target, mata::State>);
static_assert(std::same_as<TinyAutomaton::DeltaType, TinyDelta>);
static_assert(TinyAutomaton::key_arity == 1);

/**
 * @brief The whole class instantiated at once over a relation that offers only the contract.
 *
 * This is the check that @c mata::DeltaLike is the *complete* floor and not merely a plausible one.
 *  Ordinary use instantiates a member at a time, so a requirement that only one unused member
 *  imposes stays invisible; an explicit instantiation instantiates every member and so fails if any
 *  of them quietly needs more than the concept asks for. This is what caught
 *  @c AutomatonBase(size_t, ...) needing a `D(size_t)` constructor, which nothing else had
 *  surfaced -- neither review nor the whole passing test suite -- because ordinary use never
 *  instantiated that constructor over a relation lacking one.
 */
template class mata::AutomatonBase<TinyDelta>;

TEST_CASE("mata::AutomatonBase over a relation defined outside mata") {
	// 0 -a-> 1 -b-> 2 (final), plus a dead-end 0 -a-> 3 and an unreachable 4 -a-> 4.
	TinyAutomaton aut{};
	aut.delta.add(0, 'a', 1);
	aut.delta.add(0, 'a', 3);
	aut.delta.add(1, 'b', 2);
	aut.delta.add(4, 'a', 4);
	aut.initial.insert(0);
	aut.final.insert(2);

	SECTION("the presizing constructor reaches the relation") {
		const TinyAutomaton presized{7};
		CHECK(presized.num_of_states() == 7);
		CHECK(presized.delta.empty());
	}

	SECTION("the state space is read off the relation") {
		CHECK(aut.num_of_states() == 5);
		CHECK(aut.is_state(4));
		CHECK(!aut.is_state(5));
	}

	SECTION("reachability walks the third-party relation") {
		CHECK(aut.get_reachable_states() == mata::StateSet{0, 1, 2, 3});
	}

	SECTION("terminating states, which are answered by reverting") {
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
		same.delta.add(2, 'c', 0);
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
