/* nft-rvalue-ops.cc -- tests of the Nft&& overloads of union and concatenation and of moving an Nft
 */

#include <catch2/catch_test_macros.hpp>

#include "mata/nft/algorithms.hh"
#include "mata/nft/nft.hh"
#include "mata/nft/plumbing.hh"

using namespace mata::nft;
using mata::Symbol;

namespace {

/// A two-level chain reading @p letters pairs (a, b); initial 0 and final 2 * letters.
Nft chain_nft(const unsigned letters) {
	Nft nft{Nft::with_levels(2)};
	nft.add_state_with_level(0);
	for (unsigned letter{0}; letter < letters; ++letter) {
		const State source{2 * letter};
		nft.add_state_with_level(1);
		nft.add_state_with_level(0);
		nft.delta.add(source, 'a', source + 1);
		nft.delta.add(source + 1, 'b', source + 2);
	}
	nft.initial.insert(0);
	nft.final.insert(2 * letters);
	return nft;
}

/// A two-level transducer whose initial states are also final and which has a parallel edge and a cycle.
Nft tangled_nft() {
	Nft nft{Nft::with_levels(Levels{2, {0, 1, 0, 1}}, 4, {0, 2}, {0, 2})};
	nft.delta.add(0, 'a', 1);
	nft.delta.add(0, 'b', 3);
	nft.delta.add(1, 'a', 2);
	nft.delta.add(1, 'b', 2);
	nft.delta.add(1, 'b', 0);
	nft.delta.add(2, 'c', 3);
	nft.delta.add(3, 'c', 0);
	return nft;
}

const std::vector<std::pair<Nft, Nft>>& operands() {
	static const std::vector<std::pair<Nft, Nft>> operand_pairs{
		{chain_nft(2), chain_nft(1)},	{tangled_nft(), chain_nft(3)},		 {chain_nft(3), tangled_nft()},
		{tangled_nft(), tangled_nft()}, {Nft::with_levels(2), chain_nft(2)}, {chain_nft(2), Nft::with_levels(2)},
	};
	return operand_pairs;
}

/// Whether @p nft is empty and keeps @p num_of_levels levels, the state a consumed operand is left in.
bool is_consumed(const Nft& nft, const size_t num_of_levels = 2) {
	return nft.num_of_states() == 0 && nft.initial.empty() && nft.final.empty() && nft.delta.empty() &&
		   nft.levels.num_of_levels == num_of_levels;
}

} // namespace

TEST_CASE("mata::nft::Nft move-assignment moves the delta") {
	Nft source{Nft::with_levels(Levels{2, {0, 1, 0}}, 3, {0}, {2})};
	source.delta.add(0, 'a', 1);
	source.delta.add(1, 'b', 2);
	REQUIRE(source.delta.num_of_transitions() == 2);

	Nft target;
	target = std::move(source);
	// NOLINTNEXTLINE(bugprone-use-after-move): the moved-from state is exactly what is asserted here.
	CHECK(source.delta.empty());
	CHECK(target.delta.num_of_transitions() == 2);
	CHECK(target.levels.num_of_levels == 2);
	CHECK(target.levels[1] == 1);
	CHECK(target.is_in_lang_by_levels({mata::Word{'a'}, mata::Word{'b'}}));
}

TEST_CASE("mata::nft::union_nondet() rvalue overloads") {
	SECTION("the consuming overloads agree with the copying one") {
		for (const auto& [lhs, rhs] : operands()) {
			const Nft expected{union_nondet(lhs, rhs)};

			Nft moved_lhs{lhs};
			CHECK(union_nondet(std::move(moved_lhs), rhs).is_identical(expected));

			Nft moved_lhs2{lhs};
			Nft moved_rhs2{rhs};
			CHECK(union_nondet(std::move(moved_lhs2), std::move(moved_rhs2)).is_identical(expected));

			Nft in_place{lhs};
			CHECK(in_place.unite_nondet_with(Nft{rhs}).is_identical(expected));
		}
	}

	SECTION("the union keeps the levels of both operands") {
		Nft lhs{tangled_nft()};
		const Nft united{union_nondet(std::move(lhs), chain_nft(1))};
		CHECK(united.num_of_states() == 7);
		CHECK(united.levels.num_of_levels == 2);
		CHECK(united.levels[3] == 1);
		CHECK(united.levels[4] == 0);
		CHECK(united.levels[5] == 1);
		CHECK(united.delta.num_of_transitions() == tangled_nft().delta.num_of_transitions() + 2);
		CHECK(united.is_in_lang_by_levels({mata::Word{'a'}, mata::Word{'b'}}));
	}

	SECTION("aliased operands keep both copies") {
		const Nft copy{tangled_nft()};
		const Nft expected{union_nondet(copy, copy)};

		Nft aut{tangled_nft()};
		aut.unite_nondet_with(std::move(aut));
		CHECK(aut.is_identical(expected));

		Nft aut2{tangled_nft()};
		CHECK(union_nondet(std::move(aut2), aut2).is_identical(expected));

		Nft aut3{tangled_nft()};
		CHECK(union_nondet(std::move(aut3), std::move(aut3)).is_identical(expected));

		Nft in_place{tangled_nft()};
		plumbing::union_nondet(&in_place, std::move(in_place), in_place);
		CHECK(in_place.is_identical(expected));

		Nft lvalue_in_place{tangled_nft()};
		plumbing::union_nondet(&lvalue_in_place, lvalue_in_place, lvalue_in_place);
		CHECK(lvalue_in_place.is_identical(expected));
	}

	SECTION("plumbing routes every overload, including the in-place result") {
		for (const auto& [lhs, rhs] : operands()) {
			const Nft expected{union_nondet(lhs, rhs)};

			Nft lvalue_in_place{lhs};
			plumbing::union_nondet(&lvalue_in_place, lvalue_in_place, rhs);
			CHECK(lvalue_in_place.is_identical(expected));

			Nft result{};
			Nft moved_lhs{lhs};
			plumbing::union_nondet(&result, std::move(moved_lhs), rhs);
			CHECK(result.is_identical(expected));

			Nft in_place{lhs};
			plumbing::union_nondet(&in_place, std::move(in_place), rhs);
			CHECK(in_place.is_identical(expected));

			Nft both_in_place{lhs};
			Nft both_moved_rhs{rhs};
			plumbing::union_nondet(&both_in_place, std::move(both_in_place), std::move(both_moved_rhs));
			CHECK(both_in_place.is_identical(expected));
		}
	}

	SECTION("the consumed operands are empty and keep their number of levels") {
		Nft lhs{Nft::with_levels(3)};
		lhs.add_state_with_level(0);
		lhs.initial.insert(0);
		lhs.final.insert(0);
		Nft rhs{Nft::with_levels(3)};
		rhs.add_state_with_level(0);
		rhs.add_state_with_level(1);
		rhs.initial.insert(0);
		rhs.final.insert(1);
		rhs.delta.add(0, 'a', 1);
		const Nft united{union_nondet(std::move(lhs), std::move(rhs))};
		CHECK(united.num_of_states() == 3);
		CHECK(is_consumed(rhs, 3));

		Nft no_final{chain_nft(2)};
		no_final.final.clear();
		Nft member_lhs{chain_nft(1)};
		member_lhs.unite_nondet_with(std::move(no_final));
		CHECK(member_lhs.is_identical(chain_nft(1)));
		CHECK(is_consumed(no_final));

		Nft no_final_lhs{chain_nft(1)};
		no_final_lhs.final.clear();
		Nft operand{chain_nft(2)};
		no_final_lhs.unite_nondet_with(std::move(operand));
		CHECK(no_final_lhs.is_identical(chain_nft(2)));
		CHECK(is_consumed(operand));
	}
}

TEST_CASE("mata::nft::concatenate() rvalue overloads") {
	SECTION("the consuming overloads agree with the copying one, with and without epsilon") {
		for (const bool use_epsilon : {false, true}) {
			for (const auto& [lhs, rhs] : operands()) {
				StateRenaming expected_lhs_renaming{};
				StateRenaming expected_rhs_renaming{};
				const Nft expected{concatenate(lhs, rhs, use_epsilon, &expected_lhs_renaming, &expected_rhs_renaming)};

				StateRenaming lhs_renaming{};
				StateRenaming rhs_renaming{};
				Nft moved_lhs{lhs};
				CHECK(concatenate(std::move(moved_lhs), rhs, use_epsilon, &lhs_renaming, &rhs_renaming)
						  .is_identical(expected));
				CHECK(lhs_renaming == expected_lhs_renaming);
				CHECK(rhs_renaming == expected_rhs_renaming);

				StateRenaming both_lhs_renaming{};
				StateRenaming both_rhs_renaming{};
				Nft both_lhs{lhs};
				Nft both_rhs{rhs};
				CHECK(concatenate(
						  std::move(both_lhs), std::move(both_rhs), use_epsilon, &both_lhs_renaming, &both_rhs_renaming
				)
						  .is_identical(expected));
				CHECK(both_lhs_renaming == expected_lhs_renaming);
				CHECK(both_rhs_renaming == expected_rhs_renaming);
			}
		}
	}

	SECTION("concatenate_eps() with a custom epsilon agrees with the copying one") {
		const Symbol epsilon{'e'};
		for (const bool use_epsilon : {false, true}) {
			for (const auto& [lhs, rhs] : operands()) {
				const Nft expected{algorithms::concatenate_eps(lhs, rhs, epsilon, use_epsilon)};
				Nft moved_lhs{lhs};
				CHECK(
					algorithms::concatenate_eps(std::move(moved_lhs), rhs, epsilon, use_epsilon).is_identical(expected)
				);
				Nft both_lhs{lhs};
				Nft both_rhs{rhs};
				CHECK(
					algorithms::concatenate_eps(std::move(both_lhs), std::move(both_rhs), epsilon, use_epsilon)
						.is_identical(expected)
				);
			}
		}
	}

	SECTION("the concatenation connects the operands by one epsilon per level") {
		const Nft result{algorithms::concatenate_eps(chain_nft(1), chain_nft(1), 'e', true)};
		// Two chains of three states and one intermediate level-1 state on the connecting epsilon run.
		CHECK(result.num_of_states() == 7);
		CHECK(result.levels == std::vector<Level>{0, 1, 0, 0, 1, 0, 1});
		CHECK(result.delta.contains(2, 'e', 6));
		CHECK(result.delta.contains(6, 'e', 3));
		CHECK(result.delta.contains(3, 'a', 4));
		CHECK(result.delta.contains(4, 'b', 5));
		CHECK(result.delta.num_of_transitions() == 6);
		CHECK(result.final.size() == 1);
		CHECK(result.final.contains(5));
	}

	SECTION("the member overload agrees with the copying one") {
		for (const auto& [lhs, rhs] : operands()) {
			Nft expected{lhs};
			expected.concatenate(rhs);

			Nft result{lhs};
			result.concatenate(Nft{rhs});
			CHECK(result.is_identical(expected));
		}
	}

	SECTION("aliased operands keep both copies") {
		const Nft copy{tangled_nft()};
		for (const bool use_epsilon : {false, true}) {
			const Nft expected{concatenate(copy, copy, use_epsilon)};

			Nft aut{tangled_nft()};
			CHECK(concatenate(std::move(aut), aut, use_epsilon).is_identical(expected));

			Nft aut2{tangled_nft()};
			CHECK(concatenate(std::move(aut2), std::move(aut2), use_epsilon).is_identical(expected));

			Nft lvalue_in_place{tangled_nft()};
			plumbing::concatenate(&lvalue_in_place, lvalue_in_place, lvalue_in_place, use_epsilon);
			CHECK(lvalue_in_place.is_identical(expected));
		}

		Nft expected_member{tangled_nft()};
		expected_member.concatenate(copy);
		Nft member{tangled_nft()};
		member.concatenate(std::move(member));
		CHECK(member.is_identical(expected_member));
	}

	SECTION("plumbing routes every overload, including the in-place result") {
		for (const auto& [lhs, rhs] : operands()) {
			const Nft expected{concatenate(lhs, rhs)};

			Nft lvalue_in_place{lhs};
			plumbing::concatenate(&lvalue_in_place, lvalue_in_place, rhs);
			CHECK(lvalue_in_place.is_identical(expected));

			Nft result{};
			Nft moved_lhs{lhs};
			plumbing::concatenate(&result, std::move(moved_lhs), rhs);
			CHECK(result.is_identical(expected));

			Nft both_in_place{lhs};
			Nft both_moved_rhs{rhs};
			plumbing::concatenate(&both_in_place, std::move(both_in_place), std::move(both_moved_rhs));
			CHECK(both_in_place.is_identical(expected));
		}
	}

	SECTION("the consumed operands are empty and keep their number of levels") {
		Nft lhs{chain_nft(2)};
		Nft rhs{chain_nft(1)};
		concatenate(std::move(lhs), std::move(rhs));
		CHECK(is_consumed(lhs));
		CHECK(is_consumed(rhs));

		Nft lhs_of_empty{chain_nft(2)};
		Nft no_final{chain_nft(1)};
		no_final.final.clear();
		CHECK(concatenate(std::move(lhs_of_empty), std::move(no_final)).num_of_states() == 0);
		CHECK(is_consumed(lhs_of_empty));
		CHECK(is_consumed(no_final));

		Nft member_lhs{chain_nft(2)};
		Nft operand{chain_nft(1)};
		member_lhs.concatenate(std::move(operand));
		CHECK(is_consumed(operand));
		CHECK(member_lhs.is_in_lang_by_levels({mata::Word{'a', 'a', 'a'}, mata::Word{'b', 'b', 'b'}}));
	}
}
