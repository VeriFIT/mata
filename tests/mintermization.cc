/** @file
 * @brief Tests for mintermization.
 */

#include <algorithm>
#include <map>
#include <ranges>
#include <set>

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

TEST_CASE("mata::Mintermization::compute_minterms_with_incidence") {
	Parsed parsed;
	mata::Mintermization mintermization{};

	SECTION("Disjoint predicates") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final r\n"
						   "q a1 r\n"
						   "q a2 s\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];

		std::vector<BDD> sources;
		sources.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]));
		sources.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[1].second.children[0]));

		auto partition = mintermization.compute_minterms_with_incidence(sources);

		// Disjoint predicates on different variables -> 4 minterms
		REQUIRE(partition.minterms.size() == 4);
		REQUIRE(partition.minterms_of_source.size() == 2);

		// Check incidence invariant
		for (size_t m = 0; m < partition.minterms.size(); ++m) {
			for (size_t s = 0; s < sources.size(); ++s) {
				bool minterm_covered = !((partition.minterms[m] * sources[s]).IsZero());
				bool in_incidence =
					std::find(partition.minterms_of_source[s].begin(), partition.minterms_of_source[s].end(), m) !=
					partition.minterms_of_source[s].end();
				REQUIRE(minterm_covered == in_incidence);
			}
		}
	}

	SECTION("Identical predicates") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final r\n"
						   "q a1 r\n"
						   "q a1 s\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];

		std::vector<BDD> sources;
		sources.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]));
		sources.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[1].second.children[0]));

		auto partition = mintermization.compute_minterms_with_incidence(sources);

		// Identical predicates -> 2 minterms only
		REQUIRE(partition.minterms.size() == 2);
		// Both sources have identical incidence
		REQUIRE(partition.minterms_of_source[0] == partition.minterms_of_source[1]);
	}

	SECTION("Nested predicates") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final r | s\n"
						   "q (a1 & a2) r\n"
						   "q a1 s\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];

		std::vector<BDD> sources;
		sources.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]));
		sources.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[1].second.children[0]));

		auto partition = mintermization.compute_minterms_with_incidence(sources);

		// First is subset of second: (a1 & a2) ⊆ a1
		// 3 regions: a1&a2 (both), a1&!a2 (second only), !a1 (neither)
		REQUIRE(partition.minterms.size() == 3);
		REQUIRE(partition.minterms_of_source[0].size() == 1); // only (a1 & a2)
		REQUIRE(partition.minterms_of_source[1].size() == 2); // (a1 & a2) and (a1 & !a2)
	}

	SECTION("Complementary predicates") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final r | s\n"
						   "q a1 r\n"
						   "q !a1 s\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];

		std::vector<BDD> sources;
		sources.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]));
		sources.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[1].second.children[0]));

		auto partition = mintermization.compute_minterms_with_incidence(sources);

		// Complementary predicates -> 2 disjoint regions
		REQUIRE(partition.minterms.size() == 2);
		REQUIRE(partition.minterms_of_source[0].size() == 1);
		REQUIRE(partition.minterms_of_source[1].size() == 1);
		// No overlap
		REQUIRE(partition.minterms_of_source[0][0] != partition.minterms_of_source[1][0]);
	}

	SECTION("Heavily overlapping predicates") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t u\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final r | s | t | u\n"
						   "q (a1 | a2) r\n"
						   "q (a1 | a3) s\n"
						   "q (a2 | a3) t\n"
						   "q (a1 & a2) u\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];

		std::vector<BDD> sources;
		for (const auto& trans : aut.transitions) {
			sources.push_back(mintermization.graph_to_bdd_nfa(trans.second.children[0]));
		}

		auto partition = mintermization.compute_minterms_with_incidence(sources);

		// Multiple overlapping predicates
		REQUIRE(partition.minterms.size() > 4); // At least more than number of sources
		REQUIRE(partition.minterms_of_source.size() == 4);

		// Check invariant for all minterms and sources
		for (size_t m = 0; m < partition.minterms.size(); ++m) {
			for (size_t s = 0; s < sources.size(); ++s) {
				bool minterm_covered = !((partition.minterms[m] * sources[s]).IsZero());
				bool in_incidence =
					std::find(partition.minterms_of_source[s].begin(), partition.minterms_of_source[s].end(), m) !=
					partition.minterms_of_source[s].end();
				REQUIRE(minterm_covered == in_incidence);
			}
		}
	}

	SECTION("True and false predicates") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final r | s | t\n"
						   "q \\true r\n"
						   "q \\false s\n"
						   "q a1 t\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];

		std::vector<BDD> sources;
		for (const auto& trans : aut.transitions) {
			sources.push_back(mintermization.graph_to_bdd_nfa(trans.second.children[0]));
		}

		auto partition = mintermization.compute_minterms_with_incidence(sources);

		// true, false, and a1 -> should have regions
		REQUIRE(partition.minterms_of_source.size() == 3);
		// true covers all minterms
		REQUIRE(partition.minterms_of_source[0].size() == partition.minterms.size());
		// false covers no minterms
		REQUIRE(partition.minterms_of_source[1].size() == 0);
	}

	SECTION("Partition equals reference compute_minterms") {
		std::string file = "@NFA-bits\n"
						   "%States-enum q r s t\n"
						   "%Alphabet-auto\n"
						   "%Initial q\n"
						   "%Final r | s\n"
						   "q (a1 | !a2) r\n"
						   "q (a3 & a4) s\n";

		parsed = parse_mf(file);
		std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
		const auto& aut = auts[0];

		std::vector<BDD> sources;
		sources.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[0].second.children[0]));
		sources.push_back(mintermization.graph_to_bdd_nfa(aut.transitions[1].second.children[0]));

		// Get partition with incidence
		auto partition = mintermization.compute_minterms_with_incidence(sources);

		// Get reference partition
		std::unordered_set<BDD> sources_set(sources.begin(), sources.end());
		auto ref_partition = mintermization.compute_minterms(sources_set);

		// The partitions must be equal as sets of BDDs
		std::unordered_set<BDD> new_minterms(partition.minterms.begin(), partition.minterms.end());
		REQUIRE(new_minterms == ref_partition);
		REQUIRE(partition.minterms.size() == ref_partition.size());
	}

	// Cube k fixes seven variables to the bits of k, source i is the union of cubes i and i + 1. The partition is then
	//  cubes 0, ..., n and the rest of the space, and each source lies above exactly two minterms.
	std::vector<BDD> vars{};
	for (size_t v{0}; v < 7; ++v) {
		const std::string name{"a" + std::to_string(v)};
		vars.push_back(mintermization.graph_to_bdd_nfa(
			mata::FormulaGraph{
				mata::FormulaNode{mata::FormulaNode::Type::Operand, name, name, mata::FormulaNode::OperandType::Symbol}
			}
		));
	}
	const auto cube{[&](const size_t k) {
		BDD res{(k & 1) ? vars[0] : !vars[0]};
		for (size_t v{1}; v < vars.size(); ++v) { res = res * (((k >> v) & 1) ? vars[v] : !vars[v]); }
		return res;
	}};
	const auto check_against_reference{[&](const size_t num_of_sources) {
		std::vector<BDD> sources{};
		for (size_t i{0}; i < num_of_sources; ++i) { sources.push_back(cube(i) + cube(i + 1)); }

		const auto partition{mintermization.compute_minterms_with_incidence(sources)};

		CHECK(partition.minterms.size() == num_of_sources + 2);
		CHECK(
			std::unordered_set<BDD>{partition.minterms.begin(), partition.minterms.end()} ==
			mintermization.compute_minterms({sources.begin(), sources.end()})
		);
		REQUIRE(partition.minterms_of_source.size() == num_of_sources);
		for (size_t s{0}; s < num_of_sources; ++s) {
			std::vector<size_t> expected{};
			for (size_t m{0}; m < partition.minterms.size(); ++m) {
				if (partition.minterms[m].Leq(sources[s])) { expected.push_back(m); }
			}
			CHECK(expected.size() == 2);
			CHECK(partition.minterms_of_source[s] == expected);
		}
	}};

	SECTION("64 sources, the last bit of the bitmask incidence") { check_against_reference(64); }

	SECTION("65 sources, the vector incidence") { check_against_reference(65); }
}

TEST_CASE("mata::Mintermization::mintermization produces correct symbol renaming") {
	Parsed parsed;
	mata::Mintermization mintermization{};
	mata::Mintermization reference{};

	std::string file = "@NFA-bits\n"
					   "%States-enum q r s t\n"
					   "%Alphabet-auto\n"
					   "%Initial q\n"
					   "%Final s | t\n"
					   "q (a1 | !a2) r\n"
					   "r (a3 & a4) s\n"
					   "r a1 t\n"
					   "s (a1 & a2) t\n";

	parsed = parse_mf(file);
	std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
	const auto& aut = auts[0];

	using Edges = std::set<std::pair<std::string, std::string>>;
	// For each minterm, the (source, target) state pairs of the transitions over it.
	std::map<std::string, Edges> edges_of_symbol{};
	const mata::IntermediateAut res = mintermization.mintermize(aut);
	for (const auto& [lhs, rhs] : res.transitions) {
		edges_of_symbol[rhs.children[0].node.name].emplace(lhs.name, rhs.children[1].node.name);
	}

	// The same per-minterm edges computed by the reference compute_minterms() and one BDD test per (transition,
	//  minterm).
	std::vector<BDD> transition_bdds{};
	for (const auto& transition : aut.transitions) {
		transition_bdds.push_back(reference.graph_to_bdd_nfa(transition.second.children[0]));
	}
	std::vector<Edges> reference_edges{};
	for (const BDD& minterm : reference.compute_minterms({transition_bdds.begin(), transition_bdds.end()})) {
		Edges edges{};
		for (size_t i{0}; i < aut.transitions.size(); ++i) {
			if (!(transition_bdds[i] * minterm).IsZero()) {
				edges.emplace(aut.transitions[i].first.name, aut.transitions[i].second.children[1].node.name);
			}
		}
		if (!edges.empty()) { reference_edges.push_back(std::move(edges)); }
	}

	// Equal up to a renaming of the minterms.
	std::vector<Edges> result_edges{};
	for (Edges& edges : edges_of_symbol | std::views::values) { result_edges.push_back(std::move(edges)); }
	std::ranges::sort(result_edges);
	std::ranges::sort(reference_edges);
	CHECK(result_edges.size() > 1);
	CHECK(result_edges == reference_edges);
}
