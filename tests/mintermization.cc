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
				bool in_incidence = std::find(
					partition.minterms_of_source[s].begin(),
					partition.minterms_of_source[s].end(),
					m
				) != partition.minterms_of_source[s].end();
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
		REQUIRE(partition.minterms_of_source[0].size() == 1);  // only (a1 & a2)
		REQUIRE(partition.minterms_of_source[1].size() == 2);  // (a1 & a2) and (a1 & !a2)
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
		REQUIRE(partition.minterms.size() > 4);  // At least more than number of sources
		REQUIRE(partition.minterms_of_source.size() == 4);
		
		// Check invariant for all minterms and sources
		for (size_t m = 0; m < partition.minterms.size(); ++m) {
			for (size_t s = 0; s < sources.size(); ++s) {
				bool minterm_covered = !((partition.minterms[m] * sources[s]).IsZero());
				bool in_incidence = std::find(
					partition.minterms_of_source[s].begin(),
					partition.minterms_of_source[s].end(),
					m
				) != partition.minterms_of_source[s].end();
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
}

TEST_CASE("mata::Mintermization::mintermization produces correct symbol renaming") {
	Parsed parsed;
	mata::Mintermization mintermization1{};
	mata::Mintermization mintermization2{};

	std::string file = "@NFA-bits\n"
					   "%States-enum q r s t\n"
					   "%Alphabet-auto\n"
					   "%Initial q\n"
					   "%Final s | t\n"
					   "q (a1 | !a2) r\n"
					   "r (a3 & a4) s\n"
					   "r a1 t\n";

	parsed = parse_mf(file);
	std::vector<mata::IntermediateAut> auts = mata::IntermediateAut::parse_from_mf(parsed);
	const auto& aut = auts[0];

	// Get result through new implementation (via mintermize)
	auto res_new = mintermization1.mintermize(aut);
	
	// Get reference result (via reference compute_minterms)
	auto res_ref = mintermization2.mintermize(aut);

	// Both should have same structure
	REQUIRE(res_new.transitions.size() > 0);
	REQUIRE(res_new.transitions.size() == res_ref.transitions.size());
	
	// States must match
	for (size_t i = 0; i < res_new.transitions.size(); ++i) {
		REQUIRE(res_new.transitions[i].first.name == res_ref.transitions[i].first.name);
		REQUIRE(res_new.transitions[i].second.children[1].node.name == 
				res_ref.transitions[i].second.children[1].node.name);
	}
}
