/** @file
 * @brief Naming a key position of a relation, building a chain, and writing to one at any arity.
 */

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "mata/core/automaton.hh"
#include "mata/core/concepts.hh"
#include "mata/core/delta.hh"
#include "mata/relation.hh"

using namespace mata;

namespace {

using D1 = posts::RelationOf<Symbol, SmallStateSet>;
using D2 = posts::RelationOf<Symbol, Symbol, StateSet>;
using D3 = posts::RelationOf<Symbol, Symbol, Symbol, StateSet>;
using D4 = posts::RelationOf<Symbol, Symbol, Symbol, Symbol, StateSet>;

// The second key position has its own reserved keys.
using Mixed2 = posts::RelationOf<Symbol, ReservedKeys<Symbol, 100>, StateSet>;

// Probes: does the relation offer the member with this number of keys at all.
template <typename D>
concept Add1 = requires(D d) { d.add(0, 1, 2); };
template <typename D>
concept Add2 = requires(D d) { d.add(0, 1, 2, 3); };
template <typename D>
concept Add3 = requires(D d) { d.add(0, 1, 2, 3, 4); };
template <typename D>
concept Add4 = requires(D d) { d.add(0, 1, 2, 3, 4, 5); };
template <typename D>
concept Remove1 = requires(D d) { d.remove(0, 1, 2); };
template <typename D>
concept Remove2 = requires(D d) { d.remove(0, 1, 2, 3); };
template <typename D>
concept Contains1 = requires(const D d) { d.contains(0, 1, 2); };
template <typename D>
concept Contains2 = requires(const D d) { d.contains(0, 1, 2, 3); };
template <typename D>
concept HasAddTargets = requires(D d, const typename D::Nested& n) { d.add(0, 1, n); };
template <typename D>
concept HasAddTargets2 = requires(D d, const typename D::TargetSet& n) { d.add(0, 1, 2, n); };
template <typename D>
concept HasTransitions = requires(const D d) { d.transitions(); };
template <typename D>
concept HasTransitionsTo = requires(const D d) { d.get_transitions_to(0); };
template <typename D>
concept HasTransitionsBetween = requires(const D d) { d.get_transitions_between(0, 1); };
template <typename D>
concept HasKeyedSuccessors = requires(const D d) { d.get_successors(0, 1); };

// Every target of the relation, gathered through the generic walk.
template <typename D> std::vector<State> all_targets(const D& delta) {
	std::vector<State> targets{};
	const size_t states{delta.num_of_states()};
	for (State source{0}; source < states; ++source) {
		posts::visit_targets(delta.state_post(source), [&targets](const State target) { targets.push_back(target); });
	}
	return targets;
}

} // anonymous namespace.

// A module with its own transition type over the same chain as the shipped relation.
namespace theirs {
struct Transition {
	State from{};
	Symbol letter{};
	State to{};
	bool operator==(const Transition&) const = default;
	static auto parts(const Transition& t) noexcept {
		return std::tuple<utils::ArgOf<State>, utils::ArgOf<Symbol>, utils::ArgOf<State>>{t.from, t.letter, t.to};
	}
};
struct TransitionTraits {
	using Type = Transition;
	static Type make(const State source, const Symbol key, const State target) { return Type{source, key, target}; }
};
using Delta = posts::DeltaBase<StatePost, TransitionTraits>;
} // namespace theirs.

// The library ships concrete classes in the chain RelationOf builds: the entry derives from the chain's entry,
//  the post is the chain's post of that entry. Neither adds state.
static_assert(std::derived_from<SymbolPost, posts::PostChain<Symbol, SmallStateSet>::Entry>);
static_assert(
	std::derived_from<StatePost, posts::Post<SymbolPost, ReservedKeys<Symbol>, utils::DeltaAllocator<SymbolPost>>>
);
// The state post's entries come from the Delta allocator, as the spilled target sets do.
static_assert(std::same_as<StatePost::VectorType::allocator_type, utils::DeltaAllocator<SymbolPost>>);
// The successors over every symbol are a StateSet for the shipped relation, the target set for a generic one.
static_assert(std::same_as<decltype(std::declval<const StatePost&>().get_successors()), StateSet>);
static_assert(std::same_as<decltype(std::declval<const Delta&>().get_successors(State{})), StateSet>);
static_assert(std::same_as<decltype(std::declval<const Delta::Base&>().get_successors(State{})), StateSet>);
static_assert(std::same_as<decltype(std::declval<const D1&>().get_successors(State{})), D1::TargetSet>);
static_assert(std::same_as<decltype(std::declval<const StatePost&>().get_successors(Symbol{})), const SmallStateSet&>);
static_assert(std::same_as<Delta::Base, posts::DeltaBase<StatePost, mata::TransitionTraits>>);
// Members returning the relation return Delta itself, and a base is never copied into one without saying so.
static_assert(std::same_as<decltype(std::declval<Delta&>().defragment(BoolVector{}, std::vector<State>{})), Delta&>);
static_assert(std::same_as<decltype(std::declval<Delta&>().resize_for_states(State{})), Delta&>);
static_assert(!std::is_convertible_v<const Delta::Base&, Delta> && std::is_constructible_v<Delta, const Delta::Base&>);
static_assert(std::is_convertible_v<Delta::Base&&, Delta>);
static_assert(!std::is_assignable_v<Delta&, const Delta::Base&>);

// A descriptor position gives its key type and keeps its reserved keys; other positions keep the default.
static_assert(std::same_as<D2::Key<1>, Symbol> && std::same_as<Mixed2::Key<1>, Symbol>);
static_assert(std::same_as<Mixed2::Reserved<1>, ReservedKeys<Symbol, 100>>);
static_assert(std::same_as<Mixed2::Reserved<0>, ReservedKeys<Symbol>>);
static_assert(Mixed2::Reserved<0>::min_epsilon == EPSILON && Mixed2::Reserved<1>::min_epsilon == 100);

static_assert(D1::key_arity == 1 && D2::key_arity == 2 && D3::key_arity == 3);

// Entry fields: symbol and targets only where the nested type is the target set; elsewhere key() and nested().
template <typename E>
concept HasEntryFields = requires(E e) {
	e.symbol;
	e.targets;
};
static_assert(HasEntryFields<D1::Entry>);
static_assert(!HasEntryFields<D2::Entry> && !HasEntryFields<D3::Entry>);
static_assert(HasEntryFields<D2::Entry::Nested::Entry>);
// Noexcept moves, so that std::vector relocates by move.
static_assert(std::is_nothrow_move_constructible_v<D1::Entry> && std::is_nothrow_move_assignable_v<D1::Entry>);
static_assert(std::is_nothrow_move_constructible_v<D2::Entry> && std::is_nothrow_move_assignable_v<D2::Entry>);

// Structured bindings exactly where the fields are public.
template <typename E>
concept Decomposable = requires { std::tuple_size<E>::value; };
static_assert(Decomposable<D1::Entry> && Decomposable<D2::Entry::Nested::Entry>);
static_assert(!Decomposable<D2::Entry> && !Decomposable<D3::Entry>);
static_assert(std::same_as<std::tuple_element_t<0, D1::Entry>, Symbol>);
static_assert(std::same_as<std::tuple_element_t<1, D1::Entry>, SmallStateSet>);

// The target-set members exist at arity 1 and are absent above it.
template <typename E>
concept HasTargetSetMembers = requires(E e, const E ce, typename E::Target t) {
	ce.count(t);
	ce.num_of_targets();
	e.insert(t);
	e.insert(ce.nested());
	e.push_back(t);
	e.emplace_back(t);
	e.erase(t);
	ce.find(t);
	e.find(t);
	ce.for_each_target([](typename E::Target) {});
	ce.has_target(t);
	ce.target_span();
	E{typename E::Key{}, t};
};
static_assert(HasTargetSetMembers<D1::Entry> && HasTargetSetMembers<D2::Entry::Nested::Entry>);
static_assert(!HasTargetSetMembers<D2::Entry> && !HasTargetSetMembers<D3::Entry>);

// The members every entry has.
template <typename E>
concept HasEntryProtocol = requires(E e, const E ce) {
	ce.key();
	ce.nested();
	e.nested();
	ce.empty();
	e.begin();
	ce.cbegin();
	E{typename E::Key{}};
	E{typename E::Key{}, typename E::Nested{}};
};
static_assert(HasEntryProtocol<D1::Entry> && HasEntryProtocol<D2::Entry> && HasEntryProtocol<D3::Entry>);

// PostAt<0> is the whole chain, PostAt<key_arity> the target set.
static_assert(std::same_as<D3::PostAt<0>, D3::PostType>);
static_assert(std::same_as<D3::PostAt<1>, typename D3::PostType::Nested>);
static_assert(std::same_as<D3::PostAt<3>, StateSet>);
static_assert(std::same_as<D3::TargetSet, StateSet> && std::same_as<D1::TargetSet, SmallStateSet>);
static_assert(std::same_as<D2::PostAt<1>, posts::RelationOf<Symbol, StateSet>::PostType>);

static_assert(std::same_as<D3::Key<0>, Symbol> && std::same_as<D3::Key<2>, Symbol>);

// A post has one key, so it keeps the singular Key.
static_assert(std::same_as<StatePost::Key, Symbol> && std::same_as<SymbolPost::Key, Symbol>);

// DeltaLike requires neither a key type nor add.
static_assert(DeltaLike<D1> && DeltaLike<D2> && DeltaLike<D3> && DeltaLike<Mixed2>);

// Arity 1 has its own bulk add; above it the bulk add takes one key per position.
static_assert(HasAddTargets<D1> && !HasAddTargets<D2>);
static_assert(HasAddTargets2<D2> && !HasAddTargets2<D1>);
static_assert(HasTransitions<D1> && HasTransitions<D2> && HasTransitions<D3>);
static_assert(HasTransitionsTo<D1> && HasTransitionsTo<D2>);
static_assert(HasTransitionsBetween<D1> && HasTransitionsBetween<D2>);

// The keyed get_successors returns a reference where the targets are stored together (arity 1) and a new set
//  otherwise; Nfa::post() returns it as a const StateSet&.
static_assert(HasKeyedSuccessors<D1> && HasKeyedSuccessors<D2> && HasKeyedSuccessors<D3>);
static_assert(std::same_as<D1::KeyedSuccessors, const SmallStateSet&>);
static_assert(std::same_as<D2::KeyedSuccessors, StateSet> && std::same_as<D3::KeyedSuccessors, StateSet>);
static_assert(std::same_as<decltype(std::declval<const D1&>().get_successors(0)), SmallStateSet>);
static_assert(std::same_as<decltype(std::declval<const D3&>().get_successors(0)), StateSet>);

// add, remove and contains take exactly key_arity keys.
static_assert(Add1<D1> && !Add2<D1> && !Add3<D1> && !Add4<D1>);
static_assert(!Add1<D2> && Add2<D2> && !Add3<D2> && !Add4<D2>);
static_assert(!Add1<D3> && !Add2<D3> && Add3<D3> && !Add4<D3>);
static_assert(!Add1<D4> && !Add2<D4> && !Add3<D4> && Add4<D4>);
static_assert(Remove1<D1> && !Remove2<D1> && !Remove1<D2> && Remove2<D2>);
static_assert(Contains1<D1> && !Contains2<D1> && !Contains1<D2> && Contains2<D2>);

// AutomatonBase over D2, D3 and D4 is instantiated explicitly in cursor.cc, which names the same types: a program may
//  hold only one explicit instantiation definition of each.

TEST_CASE("mata::posts::DeltaBase::add - writing a key path at any arity") {
	SECTION("at arity 1 the hand-written add() and the generic key-path write store the same transitions") {
		using T1 = D1::TransitionType;
		// Out of order, so that both writes also insert into the middle of a post.
		const std::vector<T1> written{{5, 9, 5}, {0, 4, 3}, {0, 1, 2}, {2, 0, 0}, {0, 1, 1}};
		const std::vector<T1> expected{{0, 1, 1}, {0, 1, 2}, {0, 4, 3}, {2, 0, 0}, {5, 9, 5}};
		D1 by_add{};
		D1 by_path{};
		for (const T1& t : written) {
			by_add.add(t.source, t.symbol, t.target);
			posts::insert_target(by_path.mutable_state_post(t.source), t.target, t.symbol);
		}
		std::vector<T1> from_add{};
		for (const T1& t : by_add.transitions()) { from_add.push_back(t); }
		std::vector<T1> from_path{};
		for (const T1& t : by_path.transitions()) { from_path.push_back(t); }
		CHECK(from_add == expected);
		CHECK(from_path == expected);
	}

	SECTION("it presizes, exactly as add() does") {
		D2 delta{};
		REQUIRE(delta.num_of_states() == 0);
		delta.add(3, 1, 1, 7); // Neither state exists yet.
		CHECK(delta.num_of_states() == 8);
		CHECK(delta.uses_state(7));
	}

	SECTION("at arity 2 it creates the posts the path passes through") {
		D2 delta{};
		delta.add(0, 1, 1, 10);
		delta.add(0, 1, 1, 11); // Same path, second target.
		delta.add(0, 1, 2, 12); // Same first key, new second key.
		delta.add(0, 2, 1, 13); // New first key.

		const auto& post{delta.state_post(0)};
		REQUIRE(post.size() == 2); // Two first keys.
		CHECK(post.is_sorted());
		REQUIRE(post.front().nested().size() == 2); // Two second keys under key 1.
		CHECK(post.front().nested().front().nested() == StateSet{10, 11});
		CHECK(post.front().nested().back().nested() == StateSet{12});
		CHECK(post.back().nested().front().nested() == StateSet{13});
		CHECK(delta.num_of_transitions() == 4);
	}

	SECTION("at arity 3 as well, and the walk agrees") {
		D3 delta{};
		delta.add(0, 1, 2, 3, 5);
		delta.add(0, 1, 2, 4, 6);
		delta.add(1, 1, 2, 3, 7);
		CHECK(delta.num_of_transitions() == 3);
		CHECK(all_targets(delta) == std::vector<State>{5, 6, 7});

		std::vector<std::tuple<Symbol, Symbol, Symbol, State>> moves{};
		delta.for_each_move(0, [&moves](const Symbol a, const Symbol b, const Symbol c, const State t) {
			moves.emplace_back(a, b, c, t);
		});
		CHECK(moves == std::vector<std::tuple<Symbol, Symbol, Symbol, State>>{{1, 2, 3, 5}, {1, 2, 4, 6}});
	}

	SECTION("at arity 4 too, with nothing spelled for it") {
		D4 delta{};
		delta.add(0, 1, 2, 3, 4, 5);
		delta.add(0, 1, 2, 3, 6, 7);
		delta.add(2, 1, 2, 3, 4, StateSet{8, 9});
		CHECK(delta.num_of_transitions() == 4);
		CHECK(all_targets(delta) == std::vector<State>{5, 7, 8, 9});
		CHECK(delta.contains(0, 1, 2, 3, 6, 7));
		CHECK_FALSE(delta.contains(0, 1, 2, 3, 6, 5));

		delta.remove(0, 1, 2, 3, 4, 5);
		CHECK_FALSE(delta.contains(0, 1, 2, 3, 4, 5));
		CHECK_THROWS_AS(delta.remove(0, 1, 2, 9, 4, 5), std::invalid_argument);

		D4 copy{};
		for (const auto& transition : delta.transitions()) { copy.add(transition); }
		// The relation's == compares every key and target, unlike all_targets().
		CHECK(copy == delta);
	}

	SECTION("a deeper relation answers the backward walks") {
		mata::AutomatonBase<D2> aut{};
		aut.delta.add(0, 1, 2, 1);
		aut.delta.add(1, 3, 4, 2);
		aut.delta.add(3, 5, 6, 3); // A self-loop that never reaches a final state.
		aut.initial.insert(0);
		aut.final.insert(2);

		CHECK(aut.get_reachable_states() == mata::StateSet{0, 1, 2});
		// Answered over the predecessors, gathered through the successor visit at any arity.
		CHECK(aut.get_terminating_states() == mata::StateSet{0, 1, 2});

		// distances_to_final() returns num_of_states() + 1 entries at every arity, so only the first four are checked.
		const std::vector<State> to_final{aut.distances_to_final()};
		REQUIRE(to_final.size() > 3);
		CHECK(to_final[0] == 2);
		CHECK(to_final[1] == 1);
		CHECK(to_final[2] == 0);
		CHECK(to_final[3] == mata::Limits::max_state);
	}
}

TEST_CASE("mata::posts::DeltaBase - a module names its own transition fields through the traits argument") {
	using Theirs = theirs::TransitionTraits::Type;
	static_assert(std::same_as<theirs::Delta::TransitionType, Theirs>);
	static_assert(std::same_as<theirs::Delta::PostType, Delta::PostType>);
	static_assert(std::same_as<Delta::TransitionType, Transition>);

	theirs::Delta delta{};
	delta.add(0, 1, 1);
	delta.add(0, 2, 2);
	delta.add(1, 3, 2);

	std::vector<Theirs> seen{};
	for (const auto& t : delta.transitions()) { seen.push_back(t); }
	CHECK(seen == std::vector<Theirs>{{0, 1, 1}, {0, 2, 2}, {1, 3, 2}});
	// The fields are read by their own names, which is what this test is about.
	REQUIRE_FALSE(seen.empty());
	CHECK(seen.front().from == 0);
	CHECK(seen.front().letter == 1);
	CHECK(seen.front().to == 1);

	const std::vector<Theirs> to_two = delta.get_transitions_to(2);
	CHECK(to_two == std::vector<Theirs>{{0, 2, 2}, {1, 3, 2}});
}

TEST_CASE("mata::posts::DeltaBase::transitions - generic in the arity") {
	using T2 = D2::TransitionType;
	D2 delta{};
	delta.add(0, 1, 2, 7);
	delta.add(0, 1, 3, 8);
	delta.add(1, 4, 5, 0);

	std::vector<T2> seen{};
	for (const auto& t : delta.transitions()) { seen.push_back(t); }
	CHECK(seen == std::vector<T2>{{0, {1, 2}, 7}, {0, {1, 3}, 8}, {1, {4, 5}, 0}});
	REQUIRE_FALSE(seen.empty());
	CHECK(std::get<1>(seen.front().keys) == 2);

	CHECK(delta.get_transitions_to(8) == std::vector<T2>{{0, {1, 3}, 8}});
	CHECK(delta.get_transitions_between(1, 0) == std::vector<T2>{{1, {4, 5}, 0}});
	CHECK(delta.get_transitions_between(1, 7).empty());

	delta.add(2, 6, 6, StateSet{1, 2, 3});
	CHECK(delta.contains(2, 6, 6, 2));
	CHECK(delta.contains(T2{2, {6, 6}, 3}));
	CHECK(delta.get_transitions_between(2, 3) == std::vector<T2>{{2, {6, 6}, 3}});
	delta.remove(T2{2, {6, 6}, 2});
	CHECK_FALSE(delta.contains(2, 6, 6, 2));
}

TEST_CASE("mata::posts::DeltaBase::remove / contains - a key path at any arity") {
	SECTION("contains answers about the whole path, not just the first key") {
		D2 delta{};
		delta.add(0, 1, 2, 7);
		CHECK(delta.contains(0, 1, 2, 7));
		CHECK_FALSE(delta.contains(0, 1, 2, 8)); // right path, wrong target
		CHECK_FALSE(delta.contains(0, 1, 9, 7)); // right first key, wrong second key
		CHECK_FALSE(delta.contains(0, 9, 2, 7)); // wrong first key
		CHECK_FALSE(delta.contains(5, 1, 2, 7)); // state that does not exist
	}

	SECTION("remove takes one target and leaves its siblings") {
		D2 delta{};
		delta.add(0, 1, 2, 7);
		delta.add(0, 1, 2, 8);
		delta.remove(0, 1, 2, 7);
		CHECK_FALSE(delta.contains(0, 1, 2, 7));
		CHECK(delta.contains(0, 1, 2, 8));
		CHECK(delta.num_of_transitions() == 1);
	}

	SECTION("emptying a path prunes it, and the pruning cascades exactly as far as it should") {
		D2 delta{};
		delta.add(0, 1, 2, 7);
		delta.add(0, 1, 5, 9); // same first key, different second key
		delta.add(0, 3, 3, 3); // a different first key entirely
		REQUIRE(delta.state_post(0).size() == 2); // first keys 1 and 3

		// (1,2) goes, key 1 stays because (1,5) still has a target.
		delta.remove(0, 1, 2, 7);
		REQUIRE(delta.state_post(0).size() == 2);
		CHECK(delta.state_post(0).front().nested().size() == 1); // only key 5 left under key 1
		CHECK(delta.contains(0, 1, 5, 9));

		delta.remove(0, 1, 5, 9);
		CHECK(delta.state_post(0).size() == 1); // only key 3 survives
		CHECK(delta.contains(0, 3, 3, 3));

		delta.remove(0, 3, 3, 3);
		CHECK(delta.state_post(0).empty());
		CHECK(delta.num_of_transitions() == 0);
	}

	SECTION("removing a path that is not there throws, at every key position") {
		D2 delta{};
		delta.add(0, 1, 2, 7);
		CHECK_THROWS_AS(delta.remove(0, 9, 2, 7), std::invalid_argument); // missing first key
		CHECK_THROWS_AS(delta.remove(0, 1, 9, 7), std::invalid_argument); // missing second key
		CHECK_NOTHROW(delta.remove(0, 1, 2, 99)); // path exists, target does not

		// Like arity 1: a source past the end is ignored, an existing but empty one throws.
		REQUIRE(delta.num_of_states() == 8); // target 7 resized, so state 7 exists and is empty
		CHECK_NOTHROW(delta.remove(99, 1, 2, 7));
		CHECK_THROWS_AS(delta.remove(7, 1, 2, 7), std::invalid_argument);

		Delta arity1{};
		arity1.add(0, 1, 7);
		REQUIRE(arity1.num_of_states() == 8);
		// The generic arity-1 member; the shipped mata::Delta::remove() answers a missing transition by false.
		CHECK_NOTHROW(static_cast<Delta::Base&>(arity1).remove(99, 1, 7));
		CHECK_THROWS_AS(static_cast<Delta::Base&>(arity1).remove(7, 1, 7), std::invalid_argument);
		CHECK_FALSE(arity1.remove(7, 1, 7));

		CHECK(delta.contains(0, 1, 2, 7));
	}

	SECTION("arity 3 behaves the same, and the walk agrees throughout") {
		D3 delta{};
		delta.add(0, 1, 2, 3, 5);
		delta.add(0, 1, 2, 4, 6);
		CHECK(delta.contains(0, 1, 2, 3, 5));
		CHECK(delta.contains(0, 1, 2, 4, 6));
		CHECK(all_targets(delta) == std::vector<State>{5, 6});

		delta.remove(0, 1, 2, 3, 5);
		CHECK_FALSE(delta.contains(0, 1, 2, 3, 5));
		CHECK(all_targets(delta) == std::vector<State>{6});

		delta.remove(0, 1, 2, 4, 6);
		CHECK(all_targets(delta).empty());
		CHECK(delta.state_post(0).empty());
	}
}

TEST_CASE("mata::posts::DeltaBase::get_successors - the target set, at any arity") {
	SECTION("arity 1 is unchanged") {
		Delta delta{};
		delta.add(0, 1, 1);
		delta.add(0, 2, 3);
		delta.add(0, 2, 1);
		CHECK(delta.get_successors(0) == StateSet{1, 3});
		CHECK(delta.get_successors(1).empty());
	}

	SECTION("arity 2 collects from under every remaining key") {
		D2 delta{};
		delta.add(0, 1, 1, 4);
		delta.add(0, 1, 5, 2);
		delta.add(0, 9, 9, 4); // Duplicate target down a different path.
		static_assert(std::same_as<decltype(delta.get_successors(0)), StateSet>);
		CHECK(delta.get_successors(0) == StateSet{2, 4});
	}

	SECTION("the keyed form gives targets at arity 2 as well, not the post below") {
		D2 delta{};
		delta.add(0, 1, 1, 4);
		delta.add(0, 1, 5, 2); // Same first key, different second key.
		delta.add(0, 9, 9, 7); // Different first key: must not leak in.

		CHECK(delta.get_successors(0, 1) == StateSet{2, 4});
		CHECK(delta.get_successors(0, 9) == StateSet{7});
		CHECK(delta.get_successors(0, 3).empty()); // Absent key.
		CHECK(delta.get_successors(1, 1).empty()); // Absent state.
	}

	SECTION("at arity 1 the keyed form is still a reference into the relation") {
		Delta delta{};
		delta.add(0, 1, 5);
		delta.add(0, 1, 6);
		CHECK(&delta.get_successors(0, 1) == &delta.state_post(0).find(1)->targets);
		CHECK(delta.get_successors(0, 1) == StateSet{5, 6});
		CHECK(delta.get_successors(0, 2).empty()); // The static empty set.
	}
}

TEST_CASE("mata::posts::PostEntry - structured bindings at arity 1") {
	D1::Entry entry{1, StateSet{5, 6}};

	SECTION("a const binding reads the fields in place") {
		const auto& [symbol, targets] = entry;
		CHECK(symbol == 1);
		CHECK(&targets == &entry.targets);
	}

	SECTION("a mutable binding writes through") {
		auto& [symbol, targets] = entry;
		symbol = 2;
		targets.insert(7);
		CHECK(entry.symbol == 2);
		CHECK(entry.targets == StateSet{5, 6, 7});
	}

	SECTION("a binding by value copies, leaving the entry alone") {
		auto [symbol, targets] = entry;
		targets.insert(7);
		CHECK(symbol == 1);
		CHECK(entry.targets == StateSet{5, 6});
	}

	SECTION("a forwarding binding of an rvalue moves") {
		auto&& [symbol, targets] = std::move(entry);
		StateSet moved{std::move(targets)};
		CHECK(symbol == 1);
		CHECK(moved == StateSet{5, 6});
	}

	SECTION("over a post, as range-for") {
		D1 delta{};
		delta.add(0, 1, 5);
		delta.add(0, 2, 6);
		std::vector<std::pair<Symbol, StateSet>> seen{};
		for (const auto& [symbol, targets] : delta.state_post(0)) { seen.emplace_back(symbol, targets); }
		CHECK(seen == std::vector<std::pair<Symbol, StateSet>>{{1, StateSet{5}}, {2, StateSet{6}}});
	}
}
