// TODO: some header

#include "utils.hh"

#include "mata/alphabet.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/types.hh"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <random>
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

TEST_CASE("mata::nfa types should not be polymorphic - #746") {
	// OrdVector members no longer virtual, so StateSet/SymbolPost/StatePost should not be polymorphic
	CHECK(!std::is_polymorphic_v<StateSet>);
	CHECK(!std::is_polymorphic_v<SymbolPost>);
	CHECK(!std::is_polymorphic_v<StatePost>);
	// StateSet should be same size as std::vector<State>
	CHECK(sizeof(StateSet) == sizeof(std::vector<State>));
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

	bool e = delta.empty();
	CHECK(delta.mutation_epoch() == initial_epoch + 2);
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
	for (size_t i = 0; i < delta.num_of_states(); ++i) {
		for (const auto& sp : delta[i]) { count += sp.num_of_targets(); }
	}
	return count;
}

TEST_CASE("mata::nfa::Delta randomized mutation sequence vs oracle") {
	std::mt19937 gen(42); // Fixed seed for reproducibility
	std::uniform_int_distribution<> state_dist(0, 99);
	std::uniform_int_distribution<> symbol_dist(0, 9);
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
					for (size_t i = 0; i < delta.num_of_states() && posts.size() < 5; ++i) {
						posts.push_back(delta[i]);
					}
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
				delta.allocate(std::max(delta.num_of_states(), (size_t) (state_dist(gen) + 5)));
				break;
			}
			case 6: { // mutable_state_post direct edit
				State src = state_dist(gen) % (delta.num_of_states() + 10);
				auto& post = delta.mutable_state_post(src);
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

	SECTION("Buffer reuse: buffer is cleared between calls, no state leakage") {
		Nfa aut{};
		aut.add_state(4);
		aut.delta.add(0, 'a', 1);
		aut.delta.add(0, 'a', 2);
		aut.delta.add(1, 'a', 3);
		aut.delta.add(2, 'c', 0);
		aut.delta.add(2, 'c', 3);
		
		// Call unify_targets three times: first with k>=3, then again to ensure buffer cleared
		SynchronizedExistentialSymbolPostIterator sync_it;
		sync_it.push_back(aut.delta.state_post(0).cbegin(), aut.delta.state_post(0).cend());
		sync_it.push_back(aut.delta.state_post(1).cbegin(), aut.delta.state_post(1).cend());
		sync_it.push_back(aut.delta.state_post(2).cbegin(), aut.delta.state_post(2).cend());
		
		// First call: all three states have 'a' (state 2 doesn't, so k=2)
		CHECK(sync_it.synchronize_with('a'));
		StateSet result1 = sync_it.unify_targets();
		StateSet expected1{1, 2, 3};
		CHECK(result1 == expected1);
		
		// Reset and try again: ensure reusing the iterator gives same results
		sync_it.reset(3);
		sync_it.push_back(aut.delta.state_post(0).cbegin(), aut.delta.state_post(0).cend());
		sync_it.push_back(aut.delta.state_post(1).cbegin(), aut.delta.state_post(1).cend());
		sync_it.push_back(aut.delta.state_post(2).cbegin(), aut.delta.state_post(2).cend());
		CHECK(sync_it.synchronize_with('a'));
		StateSet result2 = sync_it.unify_targets();
		CHECK(result2 == expected1);
	}
}

