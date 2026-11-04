// TODO: some header

#include "utils.hh"

#include "mata/alphabet.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/types.hh"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <random>
#include <set>
#include <thread>
#include <vector>

using namespace mata::nfa;

using Symbol = mata::Symbol;

TEST_CASE("mata::nfa::SymbolPost") {
	CHECK(SymbolPost{0, StateSet{}} == SymbolPost{0, StateSet{0, 1}});
	CHECK(SymbolPost{1, StateSet{}} != SymbolPost{0, StateSet{}});
	CHECK(SymbolPost{0, StateSet{1}} < SymbolPost{1, StateSet{}});
	CHECK(SymbolPost{0, StateSet{1}} <= SymbolPost{1, StateSet{}});
	CHECK(SymbolPost{0, StateSet{1}} <= SymbolPost{0, StateSet{}});
	CHECK(SymbolPost{1, StateSet{0}} > SymbolPost{0, StateSet{1}});
	CHECK(SymbolPost{1, StateSet{0}} >= SymbolPost{0, StateSet{1}});
	CHECK(SymbolPost{1, StateSet{0}} >= SymbolPost{0, StateSet{1}});
}

TEST_CASE("mata::nfa::SymbolPost move assignment keeps the targets - issue #732") {
	// The move assignment used to be guarded by `*this != rhs`, which compares the symbols only, so
	//  every assignment between posts over the same symbol silently did nothing.
	SECTION("assigning a post over the same symbol replaces the targets") {
		SymbolPost lhs{1, StateSet{1, 2}};
		SymbolPost rhs{1, StateSet{3}};
		lhs = std::move(rhs);
		CHECK(lhs.targets == StateSet{3});
	}

	SECTION("swapping two posts over the same symbol exchanges the targets") {
		SymbolPost lhs{1, StateSet{5, 6}};
		SymbolPost rhs{1, StateSet{7}};
		std::swap(lhs, rhs);
		CHECK(lhs.targets == StateSet{7});
		CHECK(rhs.targets == StateSet{5, 6});
	}

	SECTION("self-swap keeps the targets") {
		SymbolPost post{1, StateSet{8, 9}};
		std::swap(post, post);
		CHECK(post.targets == StateSet{8, 9});
	}

	SECTION("sorting posts with repeated symbols keeps every target") {
		std::vector<SymbolPost> posts{};
		posts.emplace_back(2, StateSet{10});
		posts.emplace_back(2, StateSet{11});
		posts.emplace_back(1, StateSet{12});
		std::ranges::stable_sort(posts);
		REQUIRE(posts.size() == 3);
		CHECK(posts[0].targets == StateSet{12});
		CHECK(posts[1].targets == StateSet{10});
		CHECK(posts[2].targets == StateSet{11});
	}
}

TEST_CASE("mata::nfa::StatePost equality compares the targets - issue #732") {
	StatePost one_target{};
	one_target.insert(SymbolPost{1, StateSet{1}});
	StatePost two_targets{};
	two_targets.insert(SymbolPost{1, StateSet{2, 3}});
	StatePost same_as_one{};
	same_as_one.insert(SymbolPost{1, StateSet{1}});

	CHECK(one_target != two_targets);
	CHECK(one_target == same_as_one);
	CHECK(one_target != StatePost{});

	// `SymbolPost` itself stays symbol-only; `StatePost::find()` and `erase()` identify a post by it.
	CHECK(SymbolPost{1, StateSet{1}} == SymbolPost{1, StateSet{2, 3}});
	CHECK(two_targets.find(SymbolPost{1}) != two_targets.end());
}

TEST_CASE("mata::nfa concurrent lookups and unions on unrelated automata - issue #731") {
	// `StatePost::find(Symbol)` wrote its lookup key into a function-local `static`, and
	//  `OrdVector::insert(const OrdVector&)` merged into one. Two threads working on automata that
	//  share nothing therefore raced on those buffers: the unions came out wrong and the heap was
	//  corrupted. With both buffers gone the threads touch no common object at all.
	Nfa first{2, {0}, {1}};
	first.delta.add(0, 'a', 1);
	Nfa second{2, {0}, {1}};
	second.delta.add(0, 'b', 1);

	std::atomic<long> wrong_find{0};
	std::atomic<long> wrong_union{0};
	constexpr int iterations{200'000};
	const auto worker{[&](const Nfa& nfa, const Symbol symbol, const State base) {
		for (int iteration{0}; iteration < iterations; ++iteration) {
			if (nfa.delta[0].find(symbol) == nfa.delta[0].end()) { ++wrong_find; }
			StateSet targets{base, base + 2};
			targets.insert(StateSet{base + 1});
			if (targets != StateSet{base, base + 1, base + 2}) { ++wrong_union; }
		}
	}};

	std::thread first_worker{worker, std::cref(first), 'a', 0};
	std::thread second_worker{worker, std::cref(second), 'b', 100};
	first_worker.join();
	second_worker.join();

	CHECK(wrong_find == 0);
	CHECK(wrong_union == 0);
}

TEST_CASE("mata::nfa::Delta::get_used_symbols_set() starts empty on every call - issue #731") {
	// The set was a function-local `static` the default branch never cleared, so a second call
	//  returned the union of everything seen so far, on this `Delta` and on every other one.
	Nfa first{2, {0}, {1}};
	first.delta.add(0, 'a', 1);
	Nfa second{2, {0}, {1}};
	second.delta.add(0, 'b', 1);

	CHECK(first.delta.get_used_symbols_set() == std::set<Symbol>{'a'});
	CHECK(second.delta.get_used_symbols_set() == std::set<Symbol>{'b'});
	CHECK(first.delta.get_used_symbols_set() == std::set<Symbol>{'a'});
}

TEST_CASE("mata::nfa::Delta::state_post()") {
	Nfa aut{};

	SECTION("Add new states within the limit") {
		aut.add_state(19);
		aut.initial.insert(0);
		aut.initial.insert(1);
		aut.initial.insert(2);
		REQUIRE_NOTHROW(aut.delta.state_post(0));
		REQUIRE_NOTHROW(aut.delta.state_post(1));
		REQUIRE_NOTHROW(aut.delta.state_post(2));
		REQUIRE(aut.delta.state_post(0).empty());
		REQUIRE(aut.delta.state_post(1).empty());
		REQUIRE(aut.delta.state_post(2).empty());

		CHECK(&aut.delta.state_post(4) == &aut.delta[4]);
	}

	SECTION("Add new states over the limit") {
		aut.add_state(1);
		REQUIRE_NOTHROW(aut.initial.insert(0));
		REQUIRE_NOTHROW(aut.initial.insert(1));
		REQUIRE_NOTHROW(aut.delta.state_post(0));
		REQUIRE_NOTHROW(aut.delta.state_post(1));
		REQUIRE_NOTHROW(aut.delta.state_post(2));
		CHECK(aut.delta.state_post(0).empty());
		CHECK(aut.delta.state_post(1).empty());
		CHECK(aut.delta.state_post(2).empty());
	}

	SECTION("Add new states without specifying the number of states") {
		CHECK_NOTHROW(aut.initial.insert(0));
		CHECK_NOTHROW(aut.delta.state_post(2));
		CHECK(aut.delta.state_post(0).empty());
		CHECK(aut.delta.state_post(2).empty());
	}

	SECTION("Add new initial without specifying the number of states with over +1 number") {
		REQUIRE_NOTHROW(aut.initial.insert(25));
		CHECK_NOTHROW(aut.delta.state_post(25));
		CHECK_NOTHROW(aut.delta.state_post(26));
		CHECK(aut.delta.state_post(25).empty());
		CHECK(aut.delta.state_post(26).empty());
	}

	SECTION("Add multiple targets at once") {
		CHECK_NOTHROW(aut.delta.add(0, 1, {3, 4, 5, 6}));
		CHECK_NOTHROW(aut.delta.add(26, 1, StateSet{}));
		CHECK_NOTHROW(aut.delta.add(42, 1, StateSet{43}));
		CHECK(aut.delta.num_of_transitions() == 5);
	}
}

TEST_CASE("mata::nfa::Delta::contains()") {
	Nfa nfa;
	CHECK(!nfa.delta.contains(0, 1, 0));
	CHECK(!nfa.delta.contains(Transition{0, 1, 0}));
	nfa.delta.add(0, 1, 0);
	CHECK(nfa.delta.contains(0, 1, 0));
	CHECK(nfa.delta.contains(Transition{0, 1, 0}));
}

TEST_CASE("mata::nfa::Delta::remove()") {
	Nfa nfa;

	SECTION("Simple remove") {
		nfa.delta.add(0, 1, 0);
		CHECK_NOTHROW(nfa.delta.remove(3, 5, 6));
		CHECK_NOTHROW(nfa.delta.remove(0, 1, 0));
		CHECK(nfa.delta.empty());
		nfa.delta.add(10, 1, 0);
		CHECK_THROWS_AS(nfa.delta.remove(3, 5, 6), std::invalid_argument);
	}
}

TEST_CASE("mata::nfa::Delta::mutable_post()") {
	Nfa nfa;

	SECTION("Default initialized") {
		CHECK(nfa.delta.num_of_states() == 0);
		CHECK(!nfa.delta.uses_state(0));
		CHECK(nfa.delta.mutable_state_post(0).empty());
		CHECK(nfa.delta.num_of_states() == 1);
		CHECK(nfa.delta.uses_state(0));

		CHECK(nfa.delta.mutable_state_post(9).empty());
		CHECK(nfa.delta.num_of_states() == 10);
		CHECK(nfa.delta.uses_state(1));
		CHECK(nfa.delta.uses_state(2));
		CHECK(nfa.delta.uses_state(9));
		CHECK(!nfa.delta.uses_state(10));

		CHECK(nfa.delta.mutable_state_post(9).empty());
		CHECK(nfa.delta.num_of_states() == 10);
		CHECK(nfa.delta.uses_state(9));
		CHECK(!nfa.delta.uses_state(10));
	}
}

TEST_CASE("mata::nfa::StatePost iteration over moves") {
	Nfa nfa;
	std::vector<Move> iterated_moves{};
	std::vector<Move> expected_moves{};
	StatePost state_post{};

	SECTION("Simple NFA") {
		nfa.initial.insert(0);
		nfa.final.insert(3);
		nfa.delta.add(0, 1, 1);
		nfa.delta.add(0, 2, 1);
		nfa.delta.add(0, 5, 1);
		nfa.delta.add(1, 3, 2);
		nfa.delta.add(2, 0, 1);
		nfa.delta.add(2, 0, 3);

		state_post = nfa.delta.state_post(0);
		expected_moves = std::vector<Move>{{1, 1}, {2, 1}, {5, 1}};
		StatePost::Moves moves{state_post.moves()};
		iterated_moves.clear();
		for (auto move_it{moves.begin()}; move_it != moves.end(); ++move_it) { iterated_moves.push_back(*move_it); }
		CHECK(iterated_moves == expected_moves);

		iterated_moves = {moves.begin(), moves.end()};
		CHECK(iterated_moves == expected_moves);

		iterated_moves.clear();
		for (const Move& move : state_post.moves()) { iterated_moves.push_back(move); }
		CHECK(iterated_moves == expected_moves);

		StatePost::Moves epsilon_moves{state_post.moves_epsilons()};
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()}.empty());

		state_post = nfa.delta.state_post(1);
		moves = state_post.moves();
		StatePost::Moves moves_custom;
		moves_custom = moves;
		CHECK(
			std::vector<Move>{moves.begin(), moves.end()} == std::vector<Move>{moves_custom.begin(), moves_custom.end()}
		);
		moves_custom = state_post.moves(state_post.begin(), state_post.end());
		CHECK(
			std::vector<Move>{moves.begin(), moves.end()} == std::vector<Move>{moves_custom.begin(), moves_custom.end()}
		);
		iterated_moves.clear();
		for (auto move_it{moves.begin()}; move_it != moves.end(); ++move_it) { iterated_moves.push_back(*move_it); }
		expected_moves = std::vector<Move>{{3, 2}};
		CHECK(iterated_moves == expected_moves);
		iterated_moves = {moves.begin(), moves.end()};
		CHECK(iterated_moves == expected_moves);
		iterated_moves.clear();
		for (const Move& move : state_post.moves()) { iterated_moves.push_back(move); }
		CHECK(iterated_moves == expected_moves);
		epsilon_moves = state_post.moves_epsilons();
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()}.empty());

		state_post = nfa.delta.state_post(2);
		moves = state_post.moves();
		iterated_moves.clear();
		for (auto move_it{moves.begin()}; move_it != moves.end(); ++move_it) { iterated_moves.push_back(*move_it); }
		expected_moves = std::vector<Move>{{0, 1}, {0, 3}};
		CHECK(iterated_moves == expected_moves);
		iterated_moves = {moves.begin(), moves.end()};
		CHECK(iterated_moves == expected_moves);
		iterated_moves.clear();
		for (const Move& move : state_post.moves()) { iterated_moves.push_back(move); }
		CHECK(iterated_moves == expected_moves);
		epsilon_moves = state_post.moves_epsilons();
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()}.empty());

		state_post = nfa.delta.state_post(3);
		moves = state_post.moves();
		iterated_moves.clear();
		for (auto move_it{moves.begin()}; move_it != moves.end(); ++move_it) { iterated_moves.push_back(*move_it); }
		CHECK(iterated_moves.empty());
		CHECK(StatePost::Moves::const_iterator{state_post} == moves.end());
		iterated_moves = {moves.begin(), moves.end()};
		CHECK(iterated_moves.empty());
		iterated_moves.clear();
		for (const Move& move : state_post.moves()) { iterated_moves.push_back(move); }
		CHECK(iterated_moves.empty());
		epsilon_moves = state_post.moves_epsilons();
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()}.empty());

		state_post = nfa.delta.state_post(4);
		moves = state_post.moves();
		iterated_moves.clear();
		for (auto move_it{moves.begin()}; move_it != moves.end(); ++move_it) { iterated_moves.push_back(*move_it); }
		CHECK(iterated_moves.empty());
		iterated_moves = {moves.begin(), moves.end()};
		CHECK(iterated_moves.empty());
		iterated_moves.clear();
		for (const Move& move : state_post.moves()) { iterated_moves.push_back(move); }
		CHECK(iterated_moves.empty());
		epsilon_moves = state_post.moves_epsilons();
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()}.empty());

		nfa.delta.add(0, EPSILON, 2);
		state_post = nfa.delta.state_post(0);
		epsilon_moves = state_post.moves_epsilons();
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()} == std::vector<Move>{{EPSILON, 2}});
		nfa.delta.add(1, EPSILON, 3);
		state_post = nfa.delta.state_post(1);
		epsilon_moves = state_post.moves_epsilons();
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()} == std::vector<Move>{{EPSILON, 3}});
		nfa.delta.add(4, EPSILON, 4);
		state_post = nfa.delta.state_post(4);
		epsilon_moves = state_post.moves_epsilons();
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()} == std::vector<Move>{{EPSILON, 4}});

		state_post = nfa.delta.state_post(0);
		epsilon_moves = state_post.moves_epsilons(3);
		iterated_moves.clear();
		for (const Move& move : epsilon_moves) { iterated_moves.push_back(move); }
		CHECK(iterated_moves == std::vector<Move>{{5, 1}, {EPSILON, 2}});
		state_post = nfa.delta.state_post(1);
		epsilon_moves = state_post.moves_epsilons(3);
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()} == std::vector<Move>{{3, 2}, {EPSILON, 3}});

		state_post = nfa.delta.state_post(2);
		epsilon_moves = state_post.moves_epsilons(3);
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()}.empty());
		state_post = nfa.delta.state_post(4);
		epsilon_moves = state_post.moves_epsilons(3);
		CHECK(std::vector<Move>{epsilon_moves.begin(), epsilon_moves.end()} == std::vector<Move>{{EPSILON, 4}});

		state_post = nfa.delta.state_post(0);
		StatePost::Moves symbol_moves = state_post.moves_symbols(3);
		iterated_moves.clear();
		for (const Move& move : symbol_moves) { iterated_moves.push_back(move); }
		CHECK(iterated_moves == std::vector<Move>{{1, 1}, {2, 1}});
		symbol_moves = state_post.moves_symbols(0);
		iterated_moves.clear();
		for (const Move& move : symbol_moves) { iterated_moves.push_back(move); }
		CHECK(iterated_moves.empty());

		state_post = nfa.delta.state_post(1);
		symbol_moves = state_post.moves_symbols(3);
		CHECK(std::vector<Move>{symbol_moves.begin(), symbol_moves.end()} == std::vector<Move>{{3, 2}});
		state_post = nfa.delta.state_post(2);
		symbol_moves = state_post.moves_symbols(3);
		CHECK(std::vector<Move>{symbol_moves.begin(), symbol_moves.end()} == std::vector<Move>{{0, 1}, {0, 3}});
		state_post = nfa.delta.state_post(4);
		symbol_moves = state_post.moves_symbols(3);
		CHECK(std::vector<Move>{symbol_moves.begin(), symbol_moves.end()}.empty());

		// Create custom moves iterator.
		state_post = nfa.delta[0];
		moves = {state_post.cbegin(), state_post.cbegin() + 2};
		iterated_moves = {moves.begin(), moves.end()};
		CHECK(iterated_moves == std::vector<Move>{{1, 1}, {2, 1}});

		state_post = nfa.delta[20];
		moves = {state_post.cbegin(), state_post.cend()};
		iterated_moves = {moves.begin(), moves.end()};
		CHECK(iterated_moves.empty());
	}
}

TEST_CASE("mata::nfa::Delta iteration over transitions") {
	Nfa nfa;
	std::vector<Transition> iterated_transitions{};
	std::vector<Transition> expected_transitions{};

	SECTION("empty automaton") {
		Delta::Transitions transitions{nfa.delta.transitions()};
		CHECK(transitions.begin() == transitions.end());
		Delta::Transitions::const_iterator transition_it{nfa.delta};
		CHECK(transition_it == transitions.end());
		transition_it = {nfa.delta, 0};
		CHECK(transition_it == transitions.end());
	}

	SECTION("Simple NFA") {
		nfa.initial.insert(0);
		nfa.final.insert(3);
		nfa.delta.add(0, 1, 1);
		nfa.delta.add(0, 2, 1);
		nfa.delta.add(0, 5, 1);
		nfa.delta.add(1, 3, 2);
		nfa.delta.add(2, 0, 1);
		nfa.delta.add(2, 0, 3);

		Delta::Transitions transitions{nfa.delta.transitions()};
		iterated_transitions.clear();
		for (auto transitions_it{transitions.begin()}; transitions_it != transitions.end(); ++transitions_it) {
			iterated_transitions.push_back(*transitions_it);
		}
		expected_transitions =
			std::vector<Transition>{{0, 1, 1}, {0, 2, 1}, {0, 5, 1}, {1, 3, 2}, {2, 0, 1}, {2, 0, 3}};
		CHECK(iterated_transitions == expected_transitions);

		iterated_transitions = {transitions.begin(), transitions.end()};
		CHECK(iterated_transitions == expected_transitions);

		iterated_transitions.clear();
		for (const Transition& transition : nfa.delta.transitions()) { iterated_transitions.push_back(transition); }
		CHECK(iterated_transitions == expected_transitions);

		Delta::Transitions::const_iterator transitions_it{nfa.delta.transitions().begin()};
		CHECK(*transitions_it == Transition{0, 1, 1});
		transitions_it++;
		CHECK(*transitions_it == Transition{0, 2, 1});
		transitions_it++;
		transitions_it++;
		CHECK(*transitions_it == Transition{1, 3, 2});

		Delta::Transitions::const_iterator transitions_from_1_to_end_it{nfa.delta, 1};
		iterated_transitions.clear();
		while (transitions_from_1_to_end_it != nfa.delta.transitions().end()) {
			iterated_transitions.push_back(*transitions_from_1_to_end_it);
			transitions_from_1_to_end_it++;
		}
		expected_transitions = std::vector<Transition>{{1, 3, 2}, {2, 0, 1}, {2, 0, 3}};
		CHECK(iterated_transitions == expected_transitions);
	}

	SECTION("Sparse automaton") {
		const size_t state_num = 'r' + 1;
		nfa.delta.reserve(state_num);

		nfa.delta.add('q', 'a', 'r');
		nfa.delta.add('q', 'b', 'r');
		const Delta::Transitions transitions{nfa.delta.transitions()};
		Delta::Transitions::const_iterator it{transitions.begin()};
		Delta::Transitions::const_iterator jt{transitions.begin()};
		CHECK(it == jt);
		++it;
		CHECK(it != jt);
		CHECK(it != transitions.begin());
		CHECK(it != transitions.end());
		CHECK(jt == transitions.begin());

		++jt;
		CHECK(it == jt);
		CHECK(jt != transitions.begin());
		CHECK(jt != transitions.end());

		jt = transitions.end();
		CHECK(it != jt);
		CHECK(jt != transitions.begin());
		CHECK(jt == transitions.end());

		it = transitions.end();
		CHECK(it == jt);
		CHECK(it != transitions.begin());
		CHECK(it == transitions.end());
	}
}

TEST_CASE("mata::nfa::Delta::operator=()") {
	Nfa nfa{};
	nfa.initial.insert(0);
	nfa.final.insert(1);
	nfa.delta.add(0, 'a', 1);

	Nfa copied_nfa{nfa};
	nfa.delta.add(1, 'b', 0);
	CHECK(nfa.delta.num_of_transitions() == 2);
	CHECK(copied_nfa.delta.num_of_transitions() == 1);
}

TEST_CASE("mata::nfa::StatePost::Moves") {
	Nfa nfa{};
	nfa.initial.insert(0);
	nfa.final.insert(5);
	nfa.delta.add(0, 'a', 1);
	nfa.delta.add(1, 'b', 2);
	nfa.delta.add(1, 'c', 2);
	nfa.delta.add(1, 'd', 2);
	nfa.delta.add(2, 'e', 3);
	nfa.delta.add(3, 'e', 4);
	nfa.delta.add(4, 'f', 5);
	// TODO: rewrite in a check of moves.
	StatePost::Moves moves_from_source{nfa.delta[0].moves()};

	CHECK(std::vector<Move>{moves_from_source.begin(), moves_from_source.end()} == std::vector<Move>{{'a', 1}});
	moves_from_source = nfa.delta[1].moves();
	CHECK(
		std::vector<Move>{moves_from_source.begin(), moves_from_source.end()} ==
		std::vector<Move>{{'b', 2}, {'c', 2}, {'d', 2}}
	);
	StatePost::Moves::const_iterator move_incremented_it{moves_from_source.begin()};
	move_incremented_it++;
	CHECK(*move_incremented_it == Move{'c', 2});
	CHECK(*StatePost::Moves::const_iterator{nfa.delta.state_post(1)} == Move{'b', 2});
	CHECK(move_incremented_it != moves_from_source.begin());
	CHECK(move_incremented_it == ++moves_from_source.begin());
	StatePost::Moves moves_from_source_copy_constructed{nfa.delta[12].moves()};
	CHECK(
		std::vector<Move>{moves_from_source_copy_constructed.begin(), moves_from_source_copy_constructed.end()}.empty()
	);
}

TEST_CASE("mata::nfa::Delta::operator==()") {
	Delta delta{};
	Delta delta2{};
	CHECK(delta == delta2);
	delta.add(0, 0, 0);
	CHECK(delta != delta2);
	delta2.add(0, 0, 0);
	CHECK(delta == delta2);
	delta.add(0, 0, 1);
	delta2.add(0, 0, 2);
	CHECK(delta != delta2);
	delta2.add(0, 0, 1);
	CHECK(delta != delta2);
	delta.add(0, 0, 2);
	CHECK(delta == delta2);
	delta2.add(0, 0, 3);
	CHECK(delta != delta2);
	delta.add(0, 0, 3);
	CHECK(delta == delta2);
}

TEST_CASE("mata::nfa::Delta::add_symbols_to()") {
	mata::OnTheFlyAlphabet empty_alphabet{};
	mata::OnTheFlyAlphabet alphabet{};
	Delta delta{};
	delta.add_symbols_to(alphabet);
	CHECK(alphabet.get_symbol_map().empty());
	delta.add(0, 0, 0);
	delta.add_symbols_to(alphabet);
	CHECK(alphabet.get_symbol_map().size() == 1);
	delta.add(0, 0, 0);
	delta.add_symbols_to(alphabet);
	CHECK(alphabet.get_symbol_map().size() == 1);
	delta.add(0, 1, 0);
	delta.add_symbols_to(alphabet);
	CHECK(alphabet.get_symbol_map().size() == 2);
	delta.add(0, 2, 0);
	delta.add(0, 3, 0);
	delta.add_symbols_to(alphabet);
	CHECK(alphabet.get_symbol_map().size() == 4);
	CHECK(
		alphabet.get_symbol_map() ==
		std::unordered_map<std::string, mata::Symbol>{{"0", 0}, {"1", 1}, {"2", 2}, {"3", 3}}
	);
}

TEST_CASE("Transition comparison") {
	Transition tr1{1, 2, 3};
	Transition tr2{1, 3, 1};
	Transition tr3{1, 2, 5};
	Transition tr4{1, 2, 1};
	Transition tr5{1, 2, 1};

	CHECK(tr1 < tr2);
	CHECK(tr2 >= tr3);
	CHECK(tr3 >= tr1);
	CHECK(tr4 < tr3);
	CHECK(tr5 <= tr4);
	CHECK(tr5 == tr4);
}

TEST_CASE("mata::nfa types should not be polymorphic - #746") {
	// OrdVector members no longer virtual, so StateSet/SymbolPost/StatePost should not be polymorphic
	CHECK(!std::is_polymorphic_v<StateSet>);
	CHECK(!std::is_polymorphic_v<SymbolPost>);
	CHECK(!std::is_polymorphic_v<StatePost>);
	// StateSet should be same size as std::vector<State>
	CHECK(sizeof(StateSet) == sizeof(std::vector<State>));
}

TEST_CASE("mata::nfa::SynchronizedExistentialSymbolPostIterator::unify_targets") {
	SECTION("Not synchronized returns empty") {
		Nfa aut{};
		aut.add_state(3);
		aut.initial.insert(0);
		aut.final.insert(2);
		// Add transitions from state 0
		aut.delta.add(0, 'a', 1);
		aut.delta.add(0, 'b', 2);

		// Create a synchronized iterator and don't synchronize it
		SynchronizedExistentialSymbolPostIterator sync_it;
		sync_it.push_back(aut.delta.state_post(0).cbegin(), aut.delta.state_post(0).cend());
		// Calling unify_targets without synchronizing should return empty
		StateSet result = sync_it.unify_targets();
		CHECK(result.empty());
	}

	SECTION("k == 1: single post contributes all targets") {
		Nfa aut{};
		aut.add_state(4);
		aut.delta.add(0, 'a', 1);
		aut.delta.add(0, 'a', 3);
		aut.delta.add(1, 'b', 2);

		SynchronizedExistentialSymbolPostIterator sync_it;
		sync_it.push_back(aut.delta.state_post(0).cbegin(), aut.delta.state_post(0).cend());
		sync_it.push_back(aut.delta.state_post(1).cbegin(), aut.delta.state_post(1).cend());

		CHECK(sync_it.synchronize_with('a'));
		StateSet result = sync_it.unify_targets();
		// Only post from state 0 has symbol 'a', so result should be {1, 3}
		StateSet expected{1, 3};
		CHECK(result == expected);
	}

	SECTION("k == 2: merging two sorted sets with overlapping targets") {
		Nfa aut{};
		aut.add_state(5);
		aut.delta.add(0, 'a', 1);
		aut.delta.add(0, 'a', 3);
		aut.delta.add(1, 'a', 2);
		aut.delta.add(1, 'a', 3);

		SynchronizedExistentialSymbolPostIterator sync_it;
		sync_it.push_back(aut.delta.state_post(0).cbegin(), aut.delta.state_post(0).cend());
		sync_it.push_back(aut.delta.state_post(1).cbegin(), aut.delta.state_post(1).cend());

		CHECK(sync_it.synchronize_with('a'));
		StateSet result = sync_it.unify_targets();
		// Both have 'a': state 0 -> {1, 3}, state 1 -> {2, 3}
		// Union should be {1, 2, 3}
		StateSet expected{1, 2, 3};
		CHECK(result == expected);
	}

	SECTION("k >= 3: gathering, sorting, unique with duplicates") {
		Nfa aut{};
		aut.add_state(6);
		aut.delta.add(0, 'a', 1);
		aut.delta.add(0, 'a', 3);
		aut.delta.add(1, 'a', 2);
		aut.delta.add(1, 'a', 3);
		aut.delta.add(2, 'a', 3);
		aut.delta.add(2, 'a', 4);

		SynchronizedExistentialSymbolPostIterator sync_it;
		sync_it.push_back(aut.delta.state_post(0).cbegin(), aut.delta.state_post(0).cend());
		sync_it.push_back(aut.delta.state_post(1).cbegin(), aut.delta.state_post(1).cend());
		sync_it.push_back(aut.delta.state_post(2).cbegin(), aut.delta.state_post(2).cend());

		CHECK(sync_it.synchronize_with('a'));
		StateSet result = sync_it.unify_targets();
		// state 0 -> {1, 3}, state 1 -> {2, 3}, state 2 -> {3, 4}
		// Union should be {1, 2, 3, 4}
		StateSet expected{1, 2, 3, 4};
		CHECK(result == expected);
	}

	SECTION("Buffer reuse: a later call with three posts returns no targets of an earlier call") {
		Nfa aut{};
		aut.add_state(8);
		aut.delta.add(0, 'a', 4);
		aut.delta.add(0, 'a', 5);
		aut.delta.add(0, 'b', 1);
		aut.delta.add(1, 'a', 5);
		aut.delta.add(1, 'a', 6);
		aut.delta.add(1, 'b', 1);
		aut.delta.add(1, 'b', 2);
		aut.delta.add(2, 'a', 6);
		aut.delta.add(2, 'a', 7);
		aut.delta.add(2, 'b', 2);

		SynchronizedExistentialSymbolPostIterator sync_it;
		sync_it.push_back(aut.delta.state_post(0).cbegin(), aut.delta.state_post(0).cend());
		sync_it.push_back(aut.delta.state_post(1).cbegin(), aut.delta.state_post(1).cend());
		sync_it.push_back(aut.delta.state_post(2).cbegin(), aut.delta.state_post(2).cend());

		REQUIRE(sync_it.synchronize_with('a'));
		REQUIRE(sync_it.get_current().size() == 3);
		CHECK(sync_it.unify_targets() == StateSet{4, 5, 6, 7});

		// The second union is smaller and disjoint from the first, so any target kept in the buffer shows up.
		REQUIRE(sync_it.synchronize_with('b'));
		REQUIRE(sync_it.get_current().size() == 3);
		CHECK(sync_it.unify_targets() == StateSet{1, 2});
	}

	SECTION("k >= 3: dense and sparse target sets, repeated calls on one iterator") {
		// Each round synchronizes 3-6 posts with targets drawn either from a narrow window at a random offset (bitset
		//  path) or from a range much wider than the number of targets (sort path). Rounds reuse one iterator, so stale
		//  scratch content from a previous round would show.
		std::mt19937 gen{752};
		constexpr State num_of_states{4'096};
		SynchronizedExistentialSymbolPostIterator sync_it;
		for (size_t round{0}; round < 200; ++round) {
			const bool dense{round % 2 == 0};
			const size_t num_of_posts{3 + round % 4};
			const State window{dense ? State{130} : num_of_states};
			const State window_start{std::uniform_int_distribution<State>{0, num_of_states - window}(gen)};
			Delta delta{};
			std::set<State> expected{};
			for (State source{0}; source < num_of_posts; ++source) {
				const size_t num_of_targets{dense ? size_t{40} : size_t{4}};
				for (size_t i{0}; i < num_of_targets; ++i) {
					const State target{window_start + std::uniform_int_distribution<State>{0, window - 1}(gen)};
					delta.add(source, 'a', target);
					expected.insert(target);
				}
			}
			sync_it.reset(num_of_posts);
			for (State source{0}; source < num_of_posts; ++source) {
				sync_it.push_back(delta.state_post(source).cbegin(), delta.state_post(source).cend());
			}
			REQUIRE(sync_it.synchronize_with('a'));
			CHECK(sync_it.unify_targets() == StateSet{expected.begin(), expected.end()});
		}
	}
}

namespace {
/// The same transitions inserted one by one through @c Delta::add(), which is the reference behaviour.
Delta delta_from_adds(const std::vector<Transition>& transitions) {
	Delta delta{};
	for (const Transition& transition : transitions) { delta.add(transition); }
	return delta;
}
} // namespace

TEST_CASE("mata::nfa::DeltaBuilder") {
	SECTION("ordered construction equals repeated Delta::add()") {
		const std::vector<Transition> transitions{{0, 'a', 1}, {0, 'a', 3}, {0, 'b', 2},
												  {2, 'a', 0}, {2, 'c', 2}, {5, 'a', 1}};
		DeltaBuilder builder{6};
		size_t index{0};
		while (index < transitions.size()) {
			const State source{transitions[index].source};
			builder.begin_state(source);
			while (index < transitions.size() && transitions[index].source == source) {
				const Symbol symbol{transitions[index].symbol};
				builder.begin_symbol(symbol);
				while (index < transitions.size() && transitions[index].source == source &&
					   transitions[index].symbol == symbol) {
					builder.push_sorted_target(transitions[index].target);
					++index;
				}
				builder.finish_symbol();
			}
			builder.finish_state();
		}
		const Delta built{builder.finish()};
		CHECK(built == delta_from_adds(transitions));
		CHECK(built.num_of_transitions() == transitions.size());
	}

	SECTION("a state with no emitted symbol keeps an empty post") {
		DeltaBuilder builder{};
		builder.begin_state(2);
		builder.begin_symbol('a');
		builder.finish_symbol(); // No target: nothing is emitted.
		builder.finish_state();
		const Delta built{builder.finish()};
		CHECK(built.empty());
		CHECK(built.num_of_states() == 3);
	}

	SECTION("an empty builder yields an empty delta") {
		CHECK(DeltaBuilder{}.finish().empty());
		CHECK(DeltaBuilder{}.finish().num_of_states() == 0);
	}
}

TEST_CASE("mata::nfa::Delta::add() of a whole symbol post") {
	Delta delta{};
	delta.add(1, 'a', 4);
	delta.add(1, 'a', 8);
	delta.add(1, 'c', 2);

	SECTION("an empty symbol post changes nothing") {
		const Delta before{delta};
		delta.add(1, SymbolPost{'a', StateSet{}});
		CHECK(delta == before);
	}

	SECTION("disjoint, overlapping, equal and duplicate-heavy target sets merge like repeated add()") {
		const std::vector<StateSet> target_sets{StateSet{0, 2}, StateSet{4, 6, 8}, StateSet{4, 8},
												StateSet{4},	StateSet{9},	   StateSet{0, 4, 8, 11}};
		for (const StateSet& targets : target_sets) {
			Delta merged{delta};
			merged.add(1, SymbolPost{'a', targets});

			Delta one_by_one{delta};
			for (const State target : targets) { one_by_one.add(1, 'a', target); }
			CHECK(merged == one_by_one);
		}
	}

	SECTION("a symbol that is not in the post yet is inserted in order") {
		delta.add(1, SymbolPost{'b', StateSet{3, 5}});
		CHECK(delta.contains(1, 'b', 3));
		CHECK(delta.contains(1, 'b', 5));
		std::vector<Symbol> symbols{};
		for (const SymbolPost& symbol_post : delta[1]) { symbols.push_back(symbol_post.symbol); }
		CHECK(symbols == std::vector<Symbol>{'a', 'b', 'c'});
	}
}

TEST_CASE("mata::nfa::Delta::add() of a batch of transitions") {
	const std::vector<Transition> sorted{{0, 'a', 1}, {0, 'a', 2}, {0, 'b', 0}, {1, 'a', 1}, {3, 'c', 2}, {3, 'c', 7}};

	SECTION("the input order does not matter and duplicates collapse") {
		std::vector<std::vector<Transition>> inputs{};
		inputs.push_back(sorted);
		std::vector<Transition> descending{sorted};
		std::ranges::reverse(descending);
		inputs.push_back(descending);
		std::vector<Transition> shuffled{sorted};
		std::ranges::shuffle(shuffled, std::mt19937{11});
		inputs.push_back(shuffled);
		std::vector<Transition> duplicate_heavy{};
		for (int repetition{0}; repetition < 3; ++repetition) {
			duplicate_heavy.insert(duplicate_heavy.end(), sorted.begin(), sorted.end());
		}
		std::ranges::shuffle(duplicate_heavy, std::mt19937{12});
		inputs.push_back(duplicate_heavy);

		const Delta reference{delta_from_adds(sorted)};
		for (std::vector<Transition>& input : inputs) {
			Delta batched{};
			batched.add(std::move(input));
			CHECK(batched == reference);
		}
	}

	SECTION("a batch merges into a delta that already holds transitions") {
		Delta batched{};
		batched.add(0, 'a', 1);
		batched.add(9, 'z', 9);
		std::vector<Transition> input{sorted};
		std::ranges::shuffle(input, std::mt19937{13});
		batched.add(std::move(input));

		Delta reference{};
		reference.add(0, 'a', 1);
		reference.add(9, 'z', 9);
		for (const Transition& transition : sorted) { reference.add(transition); }
		CHECK(batched == reference);
	}

	SECTION("an empty batch changes nothing") {
		Delta batched{};
		batched.add(0, 'a', 1);
		const Delta before{batched};
		batched.add(std::vector<Transition>{});
		CHECK(batched == before);
	}

	SECTION("states up to the largest mentioned one are allocated") {
		Delta batched{};
		batched.add(std::vector<Transition>{{1, 'a', 7}});
		CHECK(batched.num_of_states() == 8);
		CHECK(batched == delta_from_adds({{1, 'a', 7}}));
	}
}

TEST_CASE("mata::nfa::State width and state overflow checks") {
	CHECK(sizeof(State) * 8 == MATA_STATE_BITS);
	// The largest value is the "no state" sentinel, so states are 0, ..., max_state - 1.
	CHECK(Limits::max_num_of_states == Limits::max_state);

	SECTION("conversions to State") {
		CHECK(to_state(0) == 0);
		CHECK(to_state(Limits::max_state - 1) == Limits::max_state - 1);
		CHECK_THROWS_AS(to_state(Limits::max_state), std::overflow_error);
		CHECK(num_of_states_for(Limits::max_state - 1) == Limits::max_num_of_states);
		CHECK_THROWS_AS(num_of_states_for(Limits::max_state), std::overflow_error);
		CHECK_NOTHROW(check_num_of_states(Limits::max_num_of_states));
		if constexpr (sizeof(State) < sizeof(size_t)) {
			CHECK_THROWS_AS(to_state(size_t{1} << MATA_STATE_BITS), std::overflow_error);
			CHECK_THROWS_AS(check_num_of_states(Limits::max_num_of_states + 1), std::overflow_error);
			CHECK_THROWS_AS(Delta(Limits::max_num_of_states + 1), std::overflow_error);
		}
	}

	SECTION("the sentinel cannot become a state") {
		Delta delta{};
		CHECK_THROWS_AS(delta.add(Limits::max_state, 0, 0), std::overflow_error);
		CHECK_THROWS_AS(delta.add(0, 0, Limits::max_state), std::overflow_error);
		CHECK_THROWS_AS(delta.mutable_state_post(Limits::max_state), std::overflow_error);
		CHECK(delta.num_of_states() == 0);

		Nfa nfa{};
		CHECK_THROWS_AS(nfa.add_state(Limits::max_state), std::overflow_error);
		CHECK(nfa.num_of_states() == 0);
	}

	SECTION("{symbol, state} of equal integer types builds a symbol post") {
		// With a 32-bit State, Symbol and State are the same type. The pair must not be taken for an iterator range.
		StatePost state_post{};
		state_post.insert({Symbol{7}, State{3}});
		REQUIRE(state_post.size() == 1);
		CHECK(state_post.front().symbol == 7);
		CHECK(state_post.front().targets == StateSet{3});
		CHECK(!std::is_constructible_v<StateSet, State, State>);
	}
}

// ============================================================
// Tests for Issue #805: O(1) empty() and transition_count
// ============================================================

TEST_CASE("mata::nfa::Delta mutation epoch") {
	Delta delta;
	const auto initial_epoch = delta.mutation_epoch();

	// Epoch should increment on mutations
	delta.add(0, 'a', 1);
	CHECK(delta.mutation_epoch() == initial_epoch + 1);

	delta.add(1, 'b', 2);
	CHECK(delta.mutation_epoch() == initial_epoch + 2);

	// Non-mutating operations should not change epoch
	auto _ = delta.num_of_transitions();
	CHECK(delta.mutation_epoch() == initial_epoch + 2);

	(void) delta.empty();
	CHECK(delta.mutation_epoch() == initial_epoch + 2);
}

TEST_CASE("mata::nfa::Delta mutation epoch of resize_for_states()") {
	Delta delta;
	const auto initial_epoch = delta.mutation_epoch();
	delta.resize_for_states(State{10});
	CHECK(delta.num_of_states() == 11);
	CHECK(delta.mutation_epoch() == initial_epoch + 1);
	// Already fits: nothing changes.
	delta.resize_for_states(State{5});
	CHECK(delta.mutation_epoch() == initial_epoch + 1);
	// A growing add() still moves the epoch once.
	delta.add(20, 'a', 30);
	CHECK(delta.mutation_epoch() == initial_epoch + 2);
}

TEST_CASE("mata::nfa::Delta moves and assignments keep count and epoch consistent") {
	Delta source;
	source.add(0, 'a', 1);
	REQUIRE(source.num_of_transitions() == 1);

	SECTION("move construction leaves a reusable empty source") {
		Delta target{std::move(source)};
		CHECK(target.num_of_transitions() == 1);
		CHECK(source.num_of_transitions() == 0);
		CHECK(source.empty());
		source.add(0, 'b', 1);
		CHECK(source.num_of_transitions() == 1);
	}

	SECTION("move assignment leaves a reusable empty source") {
		Delta target;
		target.add(0, 'c', 0);
		target.add(1, 'c', 0);
		REQUIRE(target.num_of_transitions() == 2);
		target = std::move(source);
		CHECK(target.num_of_transitions() == 1);
		CHECK(source.num_of_transitions() == 0);
		source.add(0, 'b', 1);
		CHECK(source.num_of_transitions() == 1);
	}

	SECTION("assignment moves the epoch of the target past both operands") {
		Delta target;
		const auto target_epoch = target.mutation_epoch();
		target = source;
		CHECK(target.mutation_epoch() > std::max(target_epoch, source.mutation_epoch()));
		CHECK(target.num_of_transitions() == 1);

		// Equal epochs before the assignment must not survive it.
		Delta other;
		other.add(5, 'z', 5);
		REQUIRE(other.mutation_epoch() == source.mutation_epoch());
		const auto other_epoch = other.mutation_epoch();
		other = std::move(source);
		CHECK(other.mutation_epoch() != other_epoch);
		CHECK(other.contains(0, 'a', 1));
		CHECK(!other.contains(5, 'z', 5));
	}
}

TEST_CASE("mata::nfa::Delta transition_count caching") {
	Delta delta;

	CHECK(delta.num_of_transitions() == 0);
	CHECK(delta.empty());

	// Add some transitions
	delta.add(0, 'a', 1);
	delta.add(0, 'a', 2);
	delta.add(1, 'b', 2);
	delta.add(2, 'a', 3);

	// Cached count should match scan
	CHECK(delta.num_of_transitions() == 4);
	CHECK(!delta.empty());

	// Duplicate add should not change count
	delta.add(0, 'a', 1);
	CHECK(delta.num_of_transitions() == 4);

	// Remove should decrement
	delta.remove(0, 'a', 1);
	CHECK(delta.num_of_transitions() == 3);

	// Clear should zero count
	delta.clear();
	CHECK(delta.num_of_transitions() == 0);
	CHECK(delta.empty());
}

TEST_CASE("mata::nfa::Delta long empty prefix no scan") {
	// Create a delta with 2000 empty state posts, then a few transitions
	// The old empty() would scan all 2000; now it just checks the count

	Delta delta;
	delta.allocate(2'000);

	// Add transitions only at the end
	delta.add(1'999, 'x', 1'990);

	// The new empty() should be O(1) cache lookup, not O(2000) scan
	CHECK(!delta.empty());
	CHECK(delta.num_of_transitions() == 1);

	// Similarly, verify with a totally empty large delta
	Delta empty_delta;
	empty_delta.allocate(5'000);
	CHECK(empty_delta.empty()); // Should not scan all 5000 posts
	CHECK(empty_delta.num_of_transitions() == 0);
}

TEST_CASE("mata::nfa::Delta mutable_state_post invalidates count") {
	Delta delta;
	delta.add(0, 'a', 1);
	delta.add(0, 'b', 2);

	CHECK(delta.num_of_transitions() == 2);

	// Raw edit via mutable_state_post should mark count dirty
	auto& post = delta.mutable_state_post(1);
	post.insert(SymbolPost('c', StateSet{3})); // Add transition 1 -> 3 via 'c'

	// After the raw edit, the cached count is stale; next query recomputes
	CHECK(delta.num_of_transitions() == 3);
}

TEST_CASE("mata::nfa::Delta empty and num_of_transitions agree") {
	// Ensure they can never disagree (the fix for #747)
	Delta delta;

	CHECK(delta.empty() == (delta.num_of_transitions() == 0));

	delta.add(0, 'a', 1);
	CHECK(delta.empty() == (delta.num_of_transitions() == 0));

	delta.add(1, 'b', 2);
	CHECK(delta.empty() == (delta.num_of_transitions() == 0));

	delta.clear();
	CHECK(delta.empty() == (delta.num_of_transitions() == 0));
}

TEST_CASE("mata::nfa::Delta SymbolPost insert/erase return values") {
	SymbolPost post('a');

	// insert(State) returns true on new insertion, false on duplicate
	CHECK(post.insert(1) == true);
	CHECK(post.insert(2) == true);
	CHECK(post.insert(1) == false); // duplicate

	// erase(State) returns true iff removed
	CHECK(post.erase(1) == true);
	CHECK(post.erase(1) == false); // not present
	CHECK(post.erase(2) == true);

	// insert(StateSet) returns count of newly added states
	StateSet states{10, 11, 12};
	CHECK(post.insert(states) == 3);
	CHECK(post.insert(states) == 0); // all already present

	StateSet partial{11, 13, 14};
	CHECK(post.insert(partial) == 2); // only 13 and 14 are new
}

// Oracle: recompute transition count by full scan (slow but correct)
static size_t oracle_num_of_transitions(const Delta& delta) {
	size_t count = 0;
	for (const StatePost& state_post : delta) {
		for (const auto& sp : state_post) { count += sp.num_of_targets(); }
	}
	return count;
}

TEST_CASE("mata::nfa::Delta randomized mutation sequence vs oracle") {
	std::mt19937 gen(42); // Fixed seed for reproducibility
	std::uniform_int_distribution<State> state_dist(0, 99);
	std::uniform_int_distribution<Symbol> symbol_dist(0, 9);
	std::uniform_int_distribution<> op_dist(0, 6); // 7 operations

	Delta delta;

	for (int step = 0; step < 500; ++step) {
		int op = op_dist(gen);

		switch (op) {
			case 0: { // add
				State src = state_dist(gen);
				Symbol sym = symbol_dist(gen);
				State tgt = state_dist(gen);
				delta.add(src, sym, tgt);
				break;
			}
			case 1: { // remove (often fails)
				State src = state_dist(gen);
				Symbol sym = symbol_dist(gen);
				State tgt = state_dist(gen);
				try {
					delta.remove(src, sym, tgt);
				} catch (...) {
					// Failed remove is fine; doesn't change count
				}
				break;
			}
			case 2: { // clear
				delta.clear();
				break;
			}
			case 3: { // append
				if (!delta.empty() && step % 3 == 0) {
					std::vector<StatePost> posts;
					for (State i = 0; i < delta.num_of_states() && posts.size() < 5; ++i) { posts.push_back(delta[i]); }
					delta.append(posts);
				}
				break;
			}
			case 4: { // emplace_back
				StatePost new_post;
				delta.emplace_back(std::move(new_post));
				break;
			}
			case 5: { // allocate
				delta.allocate(std::max(delta.num_of_states(), size_t{state_dist(gen) + 5}));
				break;
			}
			case 6: { // mutable_state_post direct edit
				const State src{static_cast<State>(state_dist(gen) % (delta.num_of_states() + 10))};
				(void) delta.mutable_state_post(src);
				// Do NOT further mutate after this before checking count
				// (respecting the reference-invalidation rule)
				break;
			}
		}

		// After each operation, verify count matches oracle
		CHECK(delta.num_of_transitions() == oracle_num_of_transitions(delta));
		CHECK(delta.empty() == (oracle_num_of_transitions(delta) == 0));
	}
}

TEST_CASE("mata::nfa::Delta batched imports keep the count and the epoch") {
	Delta delta{};
	delta.add(0, 'a', 1);
	delta.add(1, 'b', 2);
	REQUIRE(delta.num_of_transitions() == 2); // The count is clean from here on.

	SECTION("add(std::vector<Transition>&&) into a non-empty delta") {
		const uint64_t epoch{delta.mutation_epoch()};
		// Merges into the post of (0, 'a'), adds a new symbol to state 1 and a new state, with a duplicate.
		delta.add(std::vector<Transition>{{0, 'a', 1}, {0, 'a', 3}, {1, 'c', 0}, {4, 'a', 0}, {4, 'a', 0}});
		CHECK(delta.num_of_transitions() == 5);
		CHECK(delta.num_of_transitions() == oracle_num_of_transitions(delta));
		CHECK(delta.mutation_epoch() > epoch);
	}

	SECTION("add(State, SymbolPost&&)") {
		const uint64_t epoch{delta.mutation_epoch()};
		delta.add(0, SymbolPost{'a', StateSet{1, 2}});
		CHECK(delta.mutation_epoch() == epoch + 1);
		delta.add(0, SymbolPost{'b', StateSet{0}});
		delta.add(5, SymbolPost{'a', StateSet{0}});
		CHECK(delta.num_of_transitions() == 5);
		CHECK(delta.num_of_transitions() == oracle_num_of_transitions(delta));
		CHECK(!delta.empty());
	}
}

TEST_CASE("mata::nfa::Delta::append_shifted() keeps the count and the epoch") {
	Delta lhs{};
	lhs.add(0, 'a', 1);
	Delta rhs{};
	rhs.add(0, 'c', 1);
	rhs.add(0, 'c', 0);
	rhs.add(1, 'c', 1);
	REQUIRE(lhs.num_of_transitions() == 1); // Make the cached count clean before appending.
	const auto epoch{lhs.mutation_epoch()};

	SECTION("copying overload") {
		lhs.append_shifted(rhs, 5);
		CHECK(lhs.mutation_epoch() == epoch + 1);
		CHECK(lhs.num_of_transitions() == 4);
		CHECK(lhs.num_of_transitions() == oracle_num_of_transitions(lhs));
	}

	SECTION("consuming overload") {
		lhs.append_shifted(std::move(rhs), 5);
		CHECK(lhs.mutation_epoch() == epoch + 1);
		CHECK(lhs.num_of_transitions() == 4);
		CHECK(lhs.num_of_transitions() == oracle_num_of_transitions(lhs));
		CHECK(rhs.num_of_transitions() == 0);
	}

	SECTION("self-append") {
		lhs.append_shifted(lhs, 2);
		CHECK(lhs.num_of_transitions() == 2);
		CHECK(lhs.num_of_transitions() == oracle_num_of_transitions(lhs));
	}
}

TEST_CASE("mata::nfa::Delta count and epoch of sized and rebuilt deltas and of rejected edits") {
	SECTION("sized constructor") {
		Delta delta{5};
		CHECK(delta.num_of_states() == 5);
		CHECK(delta.empty());
		delta.add(4, 'a', 0);
		CHECK(delta.num_of_transitions() == 1);
		CHECK(delta.num_of_transitions() == oracle_num_of_transitions(delta));
	}

	SECTION("add(std::vector<Transition>&&) into an unallocated delta builds it from scratch") {
		Delta delta{};
		const uint64_t epoch{delta.mutation_epoch()};
		delta.add(std::vector<Transition>{{2, 'a', 0}, {0, 'b', 3}, {0, 'b', 3}});
		CHECK(delta.mutation_epoch() > epoch);
		CHECK(delta.num_of_states() == 4);
		CHECK(delta.num_of_transitions() == 2);
		CHECK(delta.num_of_transitions() == oracle_num_of_transitions(delta));
		delta.add(3, 'c', 3);
		CHECK(delta.num_of_transitions() == 3);
		CHECK(delta.num_of_transitions() == oracle_num_of_transitions(delta));
	}

	SECTION("reverted()") {
		Delta delta{};
		delta.add(0, 'a', 1);
		delta.add(0, 'a', 2);
		delta.add(1, 'b', 2);
		Delta reverted{delta.reverted(3)};
		CHECK(reverted.num_of_transitions() == 3);
		CHECK(reverted.num_of_transitions() == oracle_num_of_transitions(reverted));
		reverted.add(2, 'z', 0);
		CHECK(reverted.num_of_transitions() == 4);
		CHECK(reverted.num_of_transitions() == oracle_num_of_transitions(reverted));
	}

	SECTION("a rejected state leaves the count exact") {
		Delta delta{};
		delta.add(0, 'a', 1);
		REQUIRE(delta.num_of_transitions() == 1);
		CHECK_THROWS_AS(delta.add(Limits::max_state, 'a', 0), std::overflow_error);
		CHECK_THROWS_AS(delta.add(0, 'a', StateSet{Limits::max_state}), std::overflow_error);
		CHECK(delta.num_of_transitions() == 1);
		CHECK(delta.num_of_transitions() == oracle_num_of_transitions(delta));
	}

	SECTION("an overflowing append_shifted() changes neither operand") {
		Delta lhs{};
		lhs.add(0, 'a', 1);
		Delta rhs{};
		rhs.add(0, 'b', 1);
		REQUIRE(lhs.num_of_transitions() == 1);
		REQUIRE(rhs.num_of_transitions() == 1);
		const uint64_t lhs_epoch{lhs.mutation_epoch()};
		const uint64_t rhs_epoch{rhs.mutation_epoch()};
		const State offset{Limits::max_state - 1};
		CHECK_THROWS_AS(lhs.append_shifted(rhs, offset), std::overflow_error);
		CHECK_THROWS_AS(lhs.append_shifted(std::move(rhs), offset), std::overflow_error);
		CHECK(lhs.mutation_epoch() == lhs_epoch);
		CHECK(rhs.mutation_epoch() == rhs_epoch);
		CHECK(lhs.num_of_transitions() == 1);
		CHECK(rhs.num_of_transitions() == 1);
		CHECK(rhs.num_of_transitions() == oracle_num_of_transitions(rhs));
	}
}

TEST_CASE("mata::nfa::Delta count of consuming union and concatenation") {
	Nfa lhs{};
	lhs.initial = {0};
	lhs.final = {1};
	lhs.delta.add(0, 'a', 1);
	lhs.delta.add(1, 'b', 1);
	Nfa rhs{};
	rhs.initial = {0};
	rhs.final = {1};
	rhs.delta.add(0, 'c', 1);
	// Clean counts, so the appends below update them instead of leaving them to a recount.
	REQUIRE(lhs.delta.num_of_transitions() == 2);
	REQUIRE(rhs.delta.num_of_transitions() == 1);

	const auto check_consumed{[](const Nfa& aut) {
		CHECK(aut.delta.num_of_transitions() == oracle_num_of_transitions(aut.delta));
		CHECK(aut.delta.empty() == (oracle_num_of_transitions(aut.delta) == 0));
	}};

	SECTION("union_nondet(Nfa&&, Nfa&&)") {
		const Nfa result{union_nondet(std::move(lhs), std::move(rhs))};
		CHECK(result.delta.num_of_transitions() == 3);
		CHECK(result.delta.num_of_transitions() == oracle_num_of_transitions(result.delta));
		check_consumed(lhs);
		check_consumed(rhs);
	}

	SECTION("Nfa::unite_nondet_with(Nfa&&)") {
		const uint64_t epoch{lhs.delta.mutation_epoch()};
		lhs.unite_nondet_with(std::move(rhs));
		CHECK(lhs.delta.mutation_epoch() > epoch);
		CHECK(lhs.delta.num_of_transitions() == 3);
		CHECK(lhs.delta.num_of_transitions() == oracle_num_of_transitions(lhs.delta));
		check_consumed(rhs);
	}

	SECTION("concatenate(Nfa&&, Nfa&&) with an epsilon transition") {
		const Nfa result{concatenate(std::move(lhs), std::move(rhs), true)};
		CHECK(result.delta.num_of_transitions() == 4);
		CHECK(result.delta.num_of_transitions() == oracle_num_of_transitions(result.delta));
		check_consumed(lhs);
		check_consumed(rhs);
	}

	SECTION("Nfa::concatenate(Nfa&&)") {
		const uint64_t epoch{lhs.delta.mutation_epoch()};
		lhs.concatenate(std::move(rhs));
		CHECK(lhs.delta.mutation_epoch() > epoch);
		CHECK(lhs.delta.num_of_transitions() == 4);
		CHECK(lhs.delta.num_of_transitions() == oracle_num_of_transitions(lhs.delta));
		check_consumed(rhs);
	}
}

// The bad read only shows under AddressSanitizer (MATA_SANITIZE=address): freed memory usually still holds the size.
TEST_CASE("mata::nfa::Delta::add() of targets taken from the same state post") {
	// Fill the capacity that `reserve_on_insert()` gives a state post, so the next new symbol post reallocates it.
	constexpr Symbol num_of_symbol_posts{32};
	Delta delta{};
	for (Symbol symbol{0}; symbol < num_of_symbol_posts; ++symbol) { delta.add(0, 2 * symbol, 1); }
	delta.add(0, 0, 2);
	delta.add(0, 2 * (num_of_symbol_posts - 1), StateSet{3, 4, 5});
	const size_t num_of_transitions{num_of_symbol_posts + 4};
	REQUIRE(delta.num_of_transitions() == num_of_transitions); // Make the cached count clean.

	SECTION("new greatest symbol") {
		delta.add(0, 1'000, delta.get_successors(0, 0));
		CHECK(delta.get_successors(0, 1'000) == StateSet{1, 2});
		CHECK(delta.num_of_transitions() == num_of_transitions + 2);
		CHECK(delta.num_of_transitions() == oracle_num_of_transitions(delta));
	}

	SECTION("new symbol in the middle") {
		delta.add(0, 1, delta.get_successors(0, 2 * (num_of_symbol_posts - 1)));
		CHECK(delta.get_successors(0, 1) == StateSet{1, 3, 4, 5});
		CHECK(delta.num_of_transitions() == num_of_transitions + 4);
		CHECK(delta.num_of_transitions() == oracle_num_of_transitions(delta));
	}
}

// A data race here is reported by a `MATA_SANITIZE=thread` build.
TEST_CASE("mata::nfa::Delta concurrent const queries of a dirty count") {
	Delta delta;
	delta.add(0, 'a', 1);
	delta.add(1, 'b', 2);
	delta.mutable_state_post(2).insert(SymbolPost('c', StateSet{0, 1}));
	const Delta& shared{delta};

	// Each thread queries the shared delta and copies it, while the count of the shared delta is dirty.
	struct Seen {
		size_t count{};
		bool empty{true};
		size_t count_of_copy{};
	};
	std::array<Seen, 2> seen{};
	const auto query{[&shared](Seen& out) {
		out.count = shared.num_of_transitions();
		out.empty = shared.empty();
		out.count_of_copy = Delta{shared}.num_of_transitions();
	}};
	std::thread first{query, std::ref(seen[0])};
	std::thread second{query, std::ref(seen[1])};
	first.join();
	second.join();

	for (const Seen& s : seen) {
		CHECK(s.count == 4);
		CHECK(!s.empty);
		CHECK(s.count_of_copy == 4);
	}
}
