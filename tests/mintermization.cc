/** @file
 * @brief Tests for mintermization.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/applications/strings.hh"
#include "mata/parser/inter-aut.hh"
#include "mata/parser/mintermization.hh"

using namespace mata::parser;

TEST_CASE("mata::Mintermization::trans_to_bdd_nfa") {
	Parsed parsed;
	mata::Mintermization mintermization{};

	SECTION("Empty trans") {
		std::string file = "@NFA-explicit\n"
						   "%States-enum q r s t \"(r,s)\"\n"
						   "%Alphabet-auto\n"
						   "%Initial q & r\n"
						   "%Final q | r\n"
						   "q a r\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];
		REQUIRE(aut.transitions[0].first.is_operand());
		REQUIRE(aut.transitions[0].second.children[0].node.is_operand());
		const BDD bdd = mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]);
		REQUIRE(bdd.nodeCount() == 2);
	}

	SECTION("Small bitvector transition") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t \"(r,s)\"\n"
						   "%Alphabet-auto\n"
						   "%Initial q & r\n"
						   "%Final q | r\n"
						   "q (a1 | !a2)  r\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];
		REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
		REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
		const BDD bdd = mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]);
		REQUIRE(bdd.nodeCount() == 3);
	}

	SECTION("Complex bitvector transition") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t \"(r,s)\"\n"
						   "%Alphabet-auto\n"
						   "%Initial q & r\n"
						   "%Final q | r\n"
						   "q ((a1 | !a2) | (!a1 & a3 | (a4 & !a2)))  r\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];
		REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
		REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
		const BDD bdd = mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]);
		REQUIRE(bdd.nodeCount() == 4);
		int inputs[] = {0, 0, 0, 0};
		REQUIRE(bdd.Eval(inputs).IsOne());
		int inputs_false[] = {0, 1, 0, 0};
		REQUIRE(bdd.Eval(inputs_false).IsZero());
	}
} // trans to bdd section

TEST_CASE("mata::Mintermization::compute_minterms") {
	Parsed parsed;
	mata::Mintermization mintermization{};

	SECTION("Minterm from trans no elimination") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t \"(r,s)\"\n"
						   "%Alphabet-auto\n"
						   "%Initial q & r\n"
						   "%Final q | r\n"
						   "q (a1 | !a2) r\n"
						   "q (a3 & a4) r\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];
		REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
		REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
		std::unordered_set<BDD> bdds;
		bdds.insert(mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]));
		bdds.insert(mintermization.graph_to_bdd_nfa(aut.transitions[1].second.children[0]));
		auto res = mintermization.compute_minterms(bdds);
		REQUIRE(res.size() == 4);
	}

	SECTION("Minterm from trans with minterm tree elimination") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t \"(r,s)\"\n"
						   "%Alphabet-auto\n"
						   "%Initial q & r\n"
						   "%Final q | r\n"
						   "q (a1 | a2) r\n"
						   "q (a1 & a4) r\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];
		REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
		REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
		std::unordered_set<BDD> bdds;
		bdds.insert(mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]));
		bdds.insert(mintermization.graph_to_bdd_nfa(aut.transitions[1].second.children[0]));
		auto res = mintermization.compute_minterms(bdds);
		REQUIRE(res.size() == 3);
	}
} // compute_minterms

TEST_CASE("mata::Mintermization::mintermization") {
	Parsed parsed;
	mata::Mintermization mintermization{};

	SECTION("Mintermization small") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t \"(r,s)\"\n"
						   "%Alphabet-auto\n"
						   "%Initial q & r\n"
						   "%Final q | r\n"
						   "q (a1 | !a2) r\n"
						   "s (a3 & a4) t\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];
		REQUIRE(aut.transitions[0].second.children[0].node.is_operator());
		REQUIRE(aut.transitions[0].second.children[1].node.is_operand());

		const auto res = mintermization.mintermize(aut);
		REQUIRE(res.transitions.size() == 4);
		REQUIRE(res.transitions[0].first.name == "q");
		REQUIRE(res.transitions[1].first.name == "q");
		REQUIRE(res.transitions[2].first.name == "s");
		REQUIRE(res.transitions[3].first.name == "s");
		REQUIRE(res.transitions[0].second.children[1].node.name == "r");
		REQUIRE(res.transitions[1].second.children[1].node.name == "r");
		REQUIRE(res.transitions[2].second.children[1].node.name == "t");
		REQUIRE(res.transitions[3].second.children[1].node.name == "t");
	}

	SECTION("Mintermization NFA true and false") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final r\n"
						   "q \\true r\n"
						   "r a1 & a2 s\n"
						   "s \\false s\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];
		REQUIRE(aut.transitions[0].second.children[0].node.is_operand());
		REQUIRE(aut.transitions[0].second.children[0].node.raw == "\\true");
		REQUIRE(aut.transitions[0].second.children[1].node.is_operand());
		REQUIRE(aut.transitions[0].second.children[1].node.raw == "r");

		const auto res = mintermization.mintermize(aut);
		REQUIRE(res.transitions.size() == 3);
		REQUIRE(res.transitions[0].first.name == "q");
		REQUIRE(res.transitions[1].first.name == "q");
		REQUIRE(res.transitions[2].first.name == "r");
	}

	SECTION("Mintermization NFA multiple") {
		Parsed parsed;
		mata::Mintermization mintermization{};

		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final q | r\n"
						   "q (a1 | a2) r\n"
						   "s (a3 & a4) t\n"
						   "@NFA-bits\n"
						   "%States-enum q r\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final q | r\n"
						   "q (a1 & a4) r\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);

		const auto res = mintermization.mintermize(auts);
		REQUIRE(res.size() == 2);
		REQUIRE(res[0].transitions.size() == 7);
		REQUIRE(res[0].transitions[0].first.name == "q");
		REQUIRE(res[0].transitions[1].first.name == "q");
		REQUIRE(res[0].transitions[2].first.name == "q");
		REQUIRE(res[0].transitions[3].first.name == "q");
		REQUIRE(res[0].transitions[4].first.name == "s");
		REQUIRE(res[0].transitions[5].first.name == "s");
		REQUIRE(res[0].transitions[6].first.name == "s");
		REQUIRE(res[0].transitions[0].second.children[1].node.name == "r");
		REQUIRE(res[0].transitions[1].second.children[1].node.name == "r");
		REQUIRE(res[0].transitions[2].second.children[1].node.name == "r");
		REQUIRE(res[0].transitions[3].second.children[1].node.name == "r");
		REQUIRE(res[0].transitions[4].second.children[1].node.name == "t");
		REQUIRE(res[0].transitions[5].second.children[1].node.name == "t");
		REQUIRE(res[0].transitions[6].second.children[1].node.name == "t");
		REQUIRE(res[1].transitions.size() == 2);
		REQUIRE(res[1].transitions[0].first.name == "q");
		REQUIRE(res[1].transitions[1].first.name == "q");
		REQUIRE(res[1].transitions[0].second.children[1].node.name == "r");
		REQUIRE(res[1].transitions[1].second.children[1].node.name == "r");
	}

	SECTION("Mintermization AFA disjuncts") {
		Parsed parsed;
		mata::Mintermization mintermization{};

		// The symbol a1 is shared by two transitions, so its minterms are computed once and reused; the second
		//  disjunct carries a compound state formula and the third one carries no state at all.
		std::string file = "@AFA-bits\n"
						   "%States-enum q r s t\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final r\n"
						   "q (a1 & r) | ((a1 & a2) & (r & s)) | (!a1)\n"
						   "r a1 & t\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto res = mintermization.mintermize(auts[0]);

		// The three BDDs a1, a1 & a2 and !a1 have the minterms a1 & a2, a1 & !a2 and !a1. The disjuncts are taken
		//  from the right, so !a1 comes first and a1 & r last; a1 holds two minterms, the other two one each.
		REQUIRE(res.transitions.size() == 6);
		for (const auto& lhs : res.transitions | std::views::keys) { REQUIRE(lhs.is_state()); }
		REQUIRE(res.transitions[0].first.name == "q");
		REQUIRE(res.transitions[1].first.name == "q");
		REQUIRE(res.transitions[2].first.name == "q");
		REQUIRE(res.transitions[3].first.name == "q");
		REQUIRE(res.transitions[4].first.name == "r");
		REQUIRE(res.transitions[5].first.name == "r");

		// A symbol leaf: an operand numbering a minterm, named by its number and without children.
		const auto symbol_of = [](const mata::FormulaGraph& graph) {
			REQUIRE(graph.node.is_operand());
			REQUIRE(graph.node.is_symbol());
			REQUIRE(graph.node.raw == graph.node.name);
			REQUIRE(graph.children.empty());
			const size_t symbol{std::stoul(graph.node.name)};
			REQUIRE(graph.node.name == std::to_string(symbol));
			REQUIRE(symbol < 3);
			return symbol;
		};
		const auto conjunction_of = [&symbol_of](const mata::FormulaGraph& graph) {
			REQUIRE(graph.node.is_and());
			REQUIRE(graph.node.raw == "&");
			REQUIRE(graph.node.name == "&");
			REQUIRE(graph.children.size() == 2);
			return symbol_of(graph.children[0]);
		};

		// !a1: a disjunct without a state on the right-hand side is emitted as the bare symbol.
		const size_t negated{symbol_of(res.transitions[0].second)};

		// (a1 & a2) & (r & s): the compound state formula is kept whole next to the symbol.
		const mata::FormulaGraph& compound{res.transitions[1].second};
		const size_t conjoined{conjunction_of(compound)};
		REQUIRE(compound.children[1].node.is_and());
		REQUIRE(compound.children[1].children.size() == 2);
		REQUIRE(compound.children[1].children[0].node.is_state());
		REQUIRE(compound.children[1].children[0].node.name == "r");
		REQUIRE(compound.children[1].children[1].node.is_state());
		REQUIRE(compound.children[1].children[1].node.name == "s");

		// a1 & r and a1 & t: the same BDD on both transitions, so the same two symbols in the same order.
		const std::vector<size_t> from_q{
			conjunction_of(res.transitions[2].second), conjunction_of(res.transitions[3].second)
		};
		const std::vector<size_t> from_r{
			conjunction_of(res.transitions[4].second), conjunction_of(res.transitions[5].second)
		};
		REQUIRE(from_q == from_r);
		REQUIRE(from_q[0] != from_q[1]);
		REQUIRE(res.transitions[2].second.children[1].node.is_state());
		REQUIRE(res.transitions[2].second.children[1].node.name == "r");
		REQUIRE(res.transitions[2].second.children[1].children.empty());
		REQUIRE(res.transitions[3].second.children[1].node.name == "r");
		REQUIRE(res.transitions[4].second.children[1].node.is_state());
		REQUIRE(res.transitions[4].second.children[1].node.name == "t");
		REQUIRE(res.transitions[5].second.children[1].node.name == "t");

		// a1 & a2 implies a1, !a1 excludes it, and the three minterms cover the alphabet.
		REQUIRE(std::ranges::find(from_q, conjoined) != from_q.end());
		REQUIRE(std::ranges::find(from_q, negated) == from_q.end());
		REQUIRE(std::unordered_set<size_t>{negated, from_q[0], from_q[1]} == std::unordered_set<size_t>{0, 1, 2});
	}
} // TEST_CASE("mata::Mintermization::mintermization")
