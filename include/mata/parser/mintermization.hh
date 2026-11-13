/* mintermization.hh -- Mintermization of automaton.
 *
 * It transforms an automaton with a bitvector formula used as a symbol to minterminized version of the automaton.
 */

#ifndef MATA_MINTERM_HH
#define MATA_MINTERM_HH

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

#include "mata/cudd/cuddObj.hh"
#include "mata/utils/utils.hh"

#include "inter-aut.hh"

namespace mata {

class Mintermization {
	// TODO(afa): Re-add the alternating automata support. The AFA code is kept, but not compiled: `MATA_AFA_SUPPORT`
	//  is never defined.
#ifdef MATA_AFA_SUPPORT
	struct OptionalBdd {
		enum class Type : std::uint8_t { NothingE, BddE };

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
#endif

	Cudd bdd_mng_{}; // Manager of BDDs from lib cubdd, it allocates and manages BDDs.
	utils::StringMap<BDD> symbol_to_bddvar_{};
	/// The BDDs of the formulas @c graph_to_bdd_nfa() converted, keyed by a serialization of the formula. A formula
	///  usually labels many transitions. Its BDD depends only on @c symbol_to_bddvar_, so the entries stay valid across
	///  calls.
	std::unordered_map<std::string, BDD> formula_to_bdd_{};
	/// Buffer for the key of the formula being looked up in @c formula_to_bdd_.
	std::string formula_key_{};
	/// Buffer for the BDDs of the operands @c convert_graph_to_bdd_nfa() has yet to combine.
	std::vector<BDD> operand_bdds_{};
	/// The BDDs of the symbol parts of the transitions, keyed by the root node of the symbol part.
	std::unordered_map<const FormulaNode*, BDD> trans_to_bddvar_{};
	/// The keys added to @c symbol_to_bddvar_ and @c formula_to_bdd_, in their order, for @c rollback().
	std::vector<const std::string*> added_symbols_{};
	std::vector<const std::string*> added_formulas_{};
#ifdef MATA_AFA_SUPPORT
	std::unordered_map<const FormulaNode*, std::vector<DisjunctStatesPair>> lhs_to_disjuncts_and_states_{};
#endif
	/// BDDs created from transitions, in the order of their first appearance in the transitions.
	std::vector<BDD> source_bdds_{};
	/// Position of each BDD of @c source_bdds_ in that vector.
	std::unordered_map<BDD, size_t> source_bdd_to_index_{};

	/// Registers @p bdd as a source BDD of the refinement unless it is already known.
	void add_source_bdd(const BDD& bdd);

	void trans_to_bdd_nfa(const IntermediateAut& aut);
	/// @c graph_to_bdd_nfa() without the lookup in @c formula_to_bdd_. Iterative, so that deep formulas cannot exhaust
	///  the call stack.
	BDD convert_graph_to_bdd_nfa(FormulaGraphView graph);
#ifdef MATA_AFA_SUPPORT
	void trans_to_bdd_afa(const IntermediateAut& aut);
#endif

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
	BDD graph_to_bdd_nfa(FormulaGraphView graph);

	/**
	 * The BDD of the bitvector symbol @p name, as @c graph_to_bdd_nfa() makes it for an operand of that name: the
	 *  BDD of the first operand of that name, which is a fresh variable or the @p constant the operand stands for.
	 * @param name Name of the symbol.
	 * @param constant The constant (@c \\true or @c \\false) the operand stands for, if it does.
	 * @return The BDD this object keeps for @p name, valid until a @c rollback() past its creation.
	 */
	const BDD& symbol_bdd(std::string_view name, std::optional<bool> constant = std::nullopt);

	/// The symbols and formulas converted so far, which @c rollback() returns to.
	struct Checkpoint {
		size_t num_of_symbols;
		size_t num_of_formulas;
	};

	Checkpoint checkpoint() const { return {added_symbols_.size(), added_formulas_.size()}; }

	/**
	 * Forgets the symbols and formulas first converted after @p checkpoint, so that a later conversion reads their
	 *  names as if they had never been seen.
	 */
	void rollback(const Checkpoint& checkpoint);

#ifdef MATA_AFA_SUPPORT
	/**
	 * Transforms a graph representing formula at transition to bdd.
	 * This version of method is a more general one and accepts also
	 * formula including states.
	 * @param graph Graph to be transformed
	 * @return Resulting BDD
	 */
	OptionalBdd graph_to_bdd_afa(const FormulaGraph& graph);
#endif

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
	void minterms_to_aut_nfa(
		IntermediateAut& res, const IntermediateAut& aut, const std::vector<std::vector<size_t>>& minterms_of_source
	);

#ifdef MATA_AFA_SUPPORT
	/**
	 * The method for mintermization of alternating finite automaton using
	 * a given set of minterms
	 * @param res The resulting mintermized automaton
	 * @param aut Automaton to be mintermized
	 * @param minterms_of_source For each source BDD, the indices of minterms below it
	 */
	void minterms_to_aut_afa(
		IntermediateAut& res, const IntermediateAut& aut, const std::vector<std::vector<size_t>>& minterms_of_source
	);
#endif

	/// The manager starts with a small cache, which CUDD grows on demand: allocating and clearing the default one
	///  dominates the mintermization of a small automaton.
	Mintermization() : bdd_mng_(0, 0, CUDD_UNIQUE_SLOTS, INITIAL_CACHE_SLOTS) {}
	// A copy would keep pointers to the keys of the original's maps in @c added_symbols_ and @c added_formulas_.
	Mintermization(const Mintermization&) = delete;
	Mintermization& operator=(const Mintermization&) = delete;
	Mintermization(Mintermization&&) = default;
	Mintermization& operator=(Mintermization&&) = default;

  private:
	static constexpr unsigned INITIAL_CACHE_SLOTS{4'096};
};

} // namespace mata
#endif
