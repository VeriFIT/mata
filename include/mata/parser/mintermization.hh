/* mintermization.hh -- Mintermization of automaton.
 *
 * It transforms an automaton with a bitvector formula used as a symbol to minterminized version of the automaton.
 */

#ifndef MATA_MINTERM_HH
#define MATA_MINTERM_HH

#include "mata/cudd/cuddObj.hh"

#include "inter-aut.hh"

namespace mata {

class Mintermization {
	struct OptionalBdd {
		enum class Type { NothingE, BddE };

		Type type{};
		BDD val{};

		OptionalBdd() = default;
		explicit OptionalBdd(const Type t) : type(t) {}
		explicit OptionalBdd(const BDD& bdd) : type(Type::BddE), val(bdd) {}
		OptionalBdd(const Type t, const BDD& bdd) : type(t), val(bdd) {}

		OptionalBdd operator*(const OptionalBdd& b) const;
		OptionalBdd operator+(const OptionalBdd& b) const;
		OptionalBdd operator!() const;
	};

	using DisjunctStatesPair = std::pair<const FormulaGraph*, const FormulaGraph*>;

	Cudd bdd_mng_{}; // Manager of BDDs from lib cubdd, it allocates and manages BDDs.
	std::unordered_map<std::string, BDD> symbol_to_bddvar_{};
	std::unordered_map<const FormulaGraph*, BDD> trans_to_bddvar_{};
	std::unordered_map<const FormulaNode*, std::vector<DisjunctStatesPair>> lhs_to_disjuncts_and_states_{};
	std::unordered_set<BDD> bdds_{}; // bdds created from transitions
	/// BDDs created from transitions, in the order of their first appearance in the transitions.
	std::vector<BDD> source_bdds_{};
	/// Position of each BDD of @c source_bdds_ in that vector.
	std::unordered_map<BDD, size_t> source_bdd_to_index_{};

	/// Registers @p bdd as a source BDD of the refinement unless it is already known.
	void add_source_bdd(const BDD& bdd);

	void trans_to_bdd_nfa(const IntermediateAut& aut);
	void trans_to_bdd_afa(const IntermediateAut& aut);

  public:
	/**
	 * The minterm partition of a sequence of source BDDs together with the incidence between the two.
	 *
	 * The minterms are numbered by their position in @c minterms, which is the order in which the refinement
	 * created them and therefore depends only on the input, not on the addresses CUDD assigned to the nodes.
	 */
	struct MintermPartition {
		/// Minterms of the source BDDs, in creation order.
		std::vector<BDD> minterms{};
		/// For each source BDD, the ascending numbers of the minterms below it.
		std::vector<std::vector<size_t>> minterms_of_source{};
	};

	/**
	 * Takes a set of BDDs and build a minterm tree over it.
	 * The leaves of BDDs, which are minterms of input set, are returned
	 * @param source_bdds BDDs for which minterms are computed
	 * @return Computed minterms
	 */
	std::unordered_set<BDD> compute_minterms(const std::unordered_set<BDD>& source_bdds) const;

	/**
	 * Computes the same partition as @c compute_minterms() and additionally records, for each source BDD, which
	 * minterms lie below it.
	 *
	 * The incidence is carried along the refinement: when a region is split by a source BDD, the part inside the
	 * BDD inherits the incidence of the region plus that BDD, and the part outside inherits it unchanged. No BDD
	 * operation is therefore needed afterwards to relate a source BDD to the minterms below it.
	 * @param source_bdds BDDs for which minterms are computed
	 * @return Computed minterms and their incidence with @p source_bdds
	 */
	MintermPartition compute_minterms_with_incidence(const std::vector<BDD>& source_bdds) const;

	/**
	 * Transforms a graph representing formula at transition to bdd.
	 * @param graph Graph to be transformed
	 * @return Resulting BDD
	 */
	BDD graph_to_bdd_nfa(const FormulaGraph& graph);

	/**
	 * Transforms a graph representing formula at transition to bdd.
	 * This version of method is a more general one and accepts also
	 * formula including states.
	 * @param graph Graph to be transformed
	 * @return Resulting BDD
	 */
	OptionalBdd graph_to_bdd_afa(const FormulaGraph& graph);

	/**
	 * Method mintermizes given automaton which has bitvector alphabet.
	 * It transforms its transitions to BDDs, then build a minterm tree over the BDDs
	 * and finally transforms automaton to explicit one.
	 * @param aut Automaton to be mintermized.
	 * @return Mintermized automaton
	 */
	IntermediateAut mintermize(const IntermediateAut& aut);

	/**
	 * Methods mintermize given automata which have bitvector alphabet.
	 * It transforms transitions of all automata to BDDs, then build a minterm tree over the BDDs
	 * and finally transforms automata to explicit one (sharing the same minterms).
	 * @param auts Automata to be mintermized.
	 * @return Mintermized automata corresponding to the input autamata
	 */
	std::vector<IntermediateAut> mintermize(const std::vector<const IntermediateAut*>& auts);
	std::vector<IntermediateAut> mintermize(const std::vector<IntermediateAut>& auts);

	/**
	 * The method performs the mintermization over @aut with given @minterms.
	 * It is method specialized for NFA.
	 * @param res The resulting mintermized automaton
	 * @param aut Automaton to be mintermized
	 * @param minterms_of_source For each source BDD, the indices of minterms below it
	 */
	void minterms_to_aut_nfa(IntermediateAut& res, const IntermediateAut& aut, const std::vector<std::vector<size_t>>& minterms_of_source);

	/**
	 * The method for mintermization of alternating finite automaton using
	 * a given set of minterms
	 * @param res The resulting mintermized automaton
	 * @param aut Automaton to be mintermized
	 * @param minterms_of_source For each source BDD, the indices of minterms below it
	 */
	void minterms_to_aut_afa(IntermediateAut& res, const IntermediateAut& aut, const std::vector<std::vector<size_t>>& minterms_of_source);

	Mintermization() : bdd_mng_(0) {}
};

} // namespace mata
#endif
