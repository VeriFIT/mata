/* nfa-rvalue-ops.cc -- tests of the Nfa&& overloads of union and concatenation
 */

#include <catch2/catch_test_macros.hpp>

#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/plumbing.hh"

using namespace mata::nfa;
using mata::Symbol;

namespace {

/// A chain of @p states states over 'a', initial 0 and final states-1.
Nfa chain_nfa(const unsigned states) {
	Nfa nfa{};
	nfa.initial.insert(0);
	nfa.final.insert(states - 1);
	for (unsigned source{0}; source + 1 < states; ++source) { nfa.delta.add(source, 'a', source + 1); }
	return nfa;
}

/// An automaton whose initial state is also final and which has a parallel edge and a self-loop.
Nfa tangled_nfa() {
	Nfa nfa{};
	nfa.initial.insert(0);
	nfa.initial.insert(1);
	nfa.final.insert(0);
	nfa.final.insert(2);
	nfa.delta.add(0, 'a', 1);
	nfa.delta.add(0, 'b', 2);
	nfa.delta.add(1, 'a', 1);
	nfa.delta.add(1, 'a', 2);
	nfa.delta.add(2, 'c', 0);
	return nfa;
}

} // namespace

TEST_CASE("mata::nfa::union_nondet() rvalue overloads") {
	SECTION("the consuming overloads agree with the copying one") {
		const std::vector<std::pair<Nfa, Nfa>> operands{
			{chain_nfa(3), chain_nfa(2)},	{tangled_nfa(), chain_nfa(4)}, {chain_nfa(4), tangled_nfa()},
			{tangled_nfa(), tangled_nfa()}, {Nfa{}, chain_nfa(3)},		   {chain_nfa(3), Nfa{}},
		};
		for (const auto& [lhs, rhs] : operands) {
			const Nfa expected{union_nondet(lhs, rhs)};

			Nfa moved_lhs{lhs};
			CHECK(union_nondet(std::move(moved_lhs), rhs).is_identical(expected));

			Nfa moved_lhs2{lhs};
			Nfa moved_rhs2{rhs};
			CHECK(union_nondet(std::move(moved_lhs2), std::move(moved_rhs2)).is_identical(expected));

			Nfa in_place{lhs};
			CHECK(in_place.unite_nondet_with(Nfa{rhs}).is_identical(expected));
		}
	}

	SECTION("the transition count of the result is exact") {
		Nfa lhs{tangled_nfa()};
		const Nfa rhs{chain_nfa(4)};
		const Nfa united{union_nondet(std::move(lhs), rhs)};
		CHECK(united.delta.num_of_transitions() == tangled_nfa().delta.num_of_transitions() + 3);
	}

	SECTION("self-union through an rvalue reference keeps both copies") {
		Nfa aut{tangled_nfa()};
		const Nfa expected{union_nondet(aut, aut)};
		aut.unite_nondet_with(std::move(aut));
		CHECK(aut.is_identical(expected));
	}

	SECTION("plumbing routes the consuming overload, including the in-place result") {
		const Nfa lhs{tangled_nfa()};
		const Nfa rhs{chain_nfa(3)};
		const Nfa expected{union_nondet(lhs, rhs)};

		Nfa result{};
		Nfa moved_lhs{lhs};
		plumbing::union_nondet(&result, std::move(moved_lhs), rhs);
		CHECK(result.is_identical(expected));

		Nfa in_place{lhs};
		plumbing::union_nondet(&in_place, std::move(in_place), rhs);
		CHECK(in_place.is_identical(expected));
	}

	SECTION("the consumed operand is empty and reusable") {
		Nfa lhs{chain_nfa(3)};
		union_nondet(std::move(lhs), chain_nfa(2));
		CHECK(lhs.num_of_states() == 0);
		CHECK(lhs.initial.empty());
		CHECK(lhs.final.empty());
		CHECK(lhs.delta.empty());

		Nfa member_lhs{chain_nfa(3)};
		Nfa operand{chain_nfa(2)};
		member_lhs.unite_nondet_with(std::move(operand));
		CHECK(operand.num_of_states() == 0);
		CHECK(operand.delta.empty());

		lhs.initial.insert(0);
		lhs.final.insert(0);
		CHECK(lhs.is_in_lang(mata::Word{}));
	}
}

TEST_CASE("mata::nfa::concatenate() rvalue overloads") {
	const std::vector<std::pair<Nfa, Nfa>> operands{
		{chain_nfa(3), chain_nfa(2)},	{tangled_nfa(), chain_nfa(4)}, {chain_nfa(4), tangled_nfa()},
		{tangled_nfa(), tangled_nfa()}, {Nfa{}, chain_nfa(3)},		   {chain_nfa(3), Nfa{}},
	};

	SECTION("the consuming overload agrees with the copying one, with and without epsilon") {
		for (const bool use_epsilon : {false, true}) {
			for (const auto& [lhs, rhs] : operands) {
				const Nfa expected{concatenate(lhs, rhs, use_epsilon)};
				Nfa moved_lhs{lhs};
				CHECK(concatenate(std::move(moved_lhs), rhs, use_epsilon).is_identical(expected));
			}
		}
	}

	SECTION("the consuming overload reports the same state renamings") {
		for (const auto& [lhs, rhs] : operands) {
			StateRenaming expected_lhs_renaming{};
			StateRenaming expected_rhs_renaming{};
			const Nfa expected{concatenate(lhs, rhs, false, &expected_lhs_renaming, &expected_rhs_renaming)};

			StateRenaming lhs_renaming{};
			StateRenaming rhs_renaming{};
			Nfa moved_lhs{lhs};
			const Nfa result{concatenate(std::move(moved_lhs), rhs, false, &lhs_renaming, &rhs_renaming)};

			CHECK(result.is_identical(expected));
			CHECK(lhs_renaming == expected_lhs_renaming);
			CHECK(rhs_renaming == expected_rhs_renaming);
		}
	}

	SECTION("concatenate_eps() with a custom epsilon removes only that symbol") {
		const Symbol epsilon{'e'};
		for (const bool use_epsilon : {false, true}) {
			for (const auto& [lhs, rhs] : operands) {
				const Nfa expected{algorithms::concatenate_eps(lhs, rhs, epsilon, use_epsilon)};
				Nfa moved_lhs{lhs};
				CHECK(
					algorithms::concatenate_eps(std::move(moved_lhs), rhs, epsilon, use_epsilon).is_identical(expected)
				);
			}
		}
	}

	SECTION("the member overload agrees with the copying one") {
		for (const auto& [lhs, rhs] : operands) {
			Nfa expected{lhs};
			expected.concatenate(rhs);

			Nfa result{lhs};
			result.concatenate(Nfa{rhs});
			CHECK(result.is_identical(expected));
		}
	}

	SECTION("the transition count of the result is exact") {
		Nfa lhs{chain_nfa(3)};
		lhs.concatenate(Nfa{chain_nfa(4)});
		CHECK(lhs.delta.num_of_transitions() == 2 + 3 + 1); // lhs chain, rhs chain, one connecting edge.
	}

	SECTION("self-concatenation through an rvalue reference keeps both copies") {
		Nfa aut{tangled_nfa()};
		Nfa expected{tangled_nfa()};
		expected.concatenate(tangled_nfa());
		aut.concatenate(std::move(aut));
		CHECK(aut.is_identical(expected));
	}

	SECTION("concatenate_eps() of an automaton with itself keeps both copies") {
		Nfa aut{tangled_nfa()};
		const Nfa expected{algorithms::concatenate_eps(aut, aut, EPSILON, true)};
		CHECK(algorithms::concatenate_eps(std::move(aut), aut, EPSILON, true).is_identical(expected));
	}

	SECTION("the consumed operand is empty") {
		Nfa lhs{chain_nfa(3)};
		concatenate(std::move(lhs), chain_nfa(2), false);
		CHECK(lhs.num_of_states() == 0);
		CHECK(lhs.initial.empty());
		CHECK(lhs.final.empty());
		CHECK(lhs.delta.empty());

		Nfa member_lhs{chain_nfa(3)};
		Nfa operand{chain_nfa(2)};
		member_lhs.concatenate(std::move(operand));
		CHECK(operand.num_of_states() == 0);
		CHECK(operand.delta.empty());
	}
}

TEST_CASE("mata::nfa::Delta::append_shifted()") {
	Delta lhs{};
	lhs.add(0, 'a', 1);
	lhs.add(1, 'b', 0);
	Delta rhs{};
	rhs.add(0, 'c', 1);
	rhs.add(1, 'c', 1);

	SECTION("copying and consuming overloads append the same shifted posts") {
		Delta copied{lhs};
		copied.append_shifted(rhs, 2);
		Delta consumed{lhs};
		Delta consumable{rhs};
		consumed.append_shifted(std::move(consumable), 2);

		CHECK(copied == consumed);
		CHECK(copied.num_of_states() == 4);
		CHECK(copied.contains(0, 'a', 1));
		CHECK(copied.contains(1, 'b', 0));
		CHECK(copied.contains(2, 'c', 3));
		CHECK(copied.contains(3, 'c', 3));
		CHECK(copied.num_of_transitions() == 4);
		CHECK(consumed.num_of_transitions() == 4);
		CHECK(consumable.empty());
	}

	SECTION("an offset past the end leaves empty posts in between") {
		lhs.append_shifted(rhs, 5);
		CHECK(lhs.num_of_states() == 7);
		CHECK(lhs.state_post(2).empty());
		CHECK(lhs.state_post(4).empty());
		CHECK(lhs.contains(5, 'c', 6));
		CHECK(lhs.num_of_transitions() == 4);
	}

	SECTION("self-append keeps the original posts and appends their shifted copy") {
		lhs.append_shifted(lhs, 2);
		CHECK(lhs.contains(0, 'a', 1));
		CHECK(lhs.contains(1, 'b', 0));
		CHECK(lhs.contains(2, 'a', 3));
		CHECK(lhs.contains(3, 'b', 2));
		CHECK(lhs.num_of_transitions() == 4);
	}

	SECTION("an offset that would overflow the state type throws") {
		CHECK_THROWS_AS(lhs.append_shifted(rhs, Limits::max_state), std::overflow_error);
		CHECK_THROWS_AS(lhs.append_shifted(Delta{rhs}, Limits::max_state), std::overflow_error);
	}
}
