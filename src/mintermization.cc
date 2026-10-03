/*
 * mintermization.hh -- Mintermization of automaton.
 *
 * It transforms an automaton with a bitvector formula used a symbol to mintermized version of the automaton.
 */

#include "mata/parser/mintermization.hh"
#include "mata/utils/assert.hh"

#include <ranges>

namespace {
const mata::FormulaGraph* detect_state_part(const mata::FormulaGraph* node) {
	if (node->node.is_state()) { return node; }

	std::vector<const mata::FormulaGraph*> worklist{node};
	while (!worklist.empty()) {
		const auto act_node = worklist.back();
		MATA_ASSERT(act_node != nullptr);
		worklist.pop_back();
		if (act_node->children.size() != 2) { continue; }

		if (act_node->children.front().node.is_and() && act_node->children[1].node.is_state()) {
			return act_node; // ... & a1 & q1 ... & qn
		} else if (act_node->children.front().node.is_state() && act_node->children[1].node.is_and()) {
			return act_node; // ... & a1 & q1 ... & qn
		} else if (act_node->children.front().node.is_state() && act_node->children[1].node.is_state()) {
			return act_node; // ... & a1 & q1 & q2
		} else if (act_node->children.front().node.is_state() && act_node->children[1].node.is_state()) {
			return act_node; // ... & a1 & q1 & q2
		} else if (act_node->node.is_operator() && act_node->children[1].node.is_state()) {
			return &act_node->children[1]; // a1 & q1
		} else if (act_node->children.front().node.is_state() && act_node->node.is_operator()) {
			return &act_node->children.front(); // a1 & q1
		} else {
			for (const mata::FormulaGraph& child : act_node->children) { worklist.push_back(&child); }
		}
	}

	return nullptr;
}
} // namespace
void mata::Mintermization::add_source_bdd(const BDD& bdd) {
	if (source_bdd_to_index_.contains(bdd)) {
		return;  // Already registered
	}
	bdds_.insert(bdd);  // Keep bdds_ for compatibility
	source_bdd_to_index_[bdd] = source_bdds_.size();
	source_bdds_.push_back(bdd);
}


void mata::Mintermization::trans_to_bdd_nfa(const IntermediateAut& aut) {
	MATA_ASSERT(aut.is_nfa());

	for (const auto& trans : aut.transitions) {
		// Foreach transition create a BDD
		const auto& symbol_part = aut.get_symbol_part_of_transition(trans);
		MATA_ASSERT(
			symbol_part.node.is_operator() || symbol_part.children.empty(),
			"Symbol part must be either formula or single symbol"
		);
		const BDD bdd = graph_to_bdd_nfa(symbol_part);
		if (bdd.IsZero()) { continue; }
		add_source_bdd(bdd);
		trans_to_bddvar_[&symbol_part] = bdd;
	}
}

void mata::Mintermization::trans_to_bdd_afa(const IntermediateAut& aut) {
	MATA_ASSERT(aut.is_afa());

	for (const auto& [formula_node, formula_graph] : aut.transitions) {
		lhs_to_disjuncts_and_states_[&formula_node] = std::vector<DisjunctStatesPair>();
		if (formula_graph.node.is_state()) { // node from state to state
			lhs_to_disjuncts_and_states_[&formula_node].emplace_back(&formula_graph, &formula_graph);
		}
		// split transition to disjuncts
		const FormulaGraph* act_graph = &formula_graph;

		if (!formula_graph.node.is_state() && act_graph->node.is_operator() &&
			act_graph->node.operator_type != FormulaNode::OperatorType::Or) { // there are no disjuncts
			lhs_to_disjuncts_and_states_[&formula_node].emplace_back(act_graph, detect_state_part(act_graph));
		} else if (!formula_graph.node.is_state()) {
			while (act_graph->node.is_operator() && act_graph->node.operator_type == FormulaNode::OperatorType::Or) {
				// map lhs to disjunct and its state formula. The content of disjunct is right son of actual graph
				// since the left one is a rest of formula
				lhs_to_disjuncts_and_states_[&formula_node].emplace_back(
					&act_graph->children[1], detect_state_part(&act_graph->children[1])
				);
				act_graph = &(act_graph->children.front());
			}

			// take care of last disjunct
			lhs_to_disjuncts_and_states_[&formula_node].emplace_back(act_graph, detect_state_part(act_graph));
		}

		// Foreach disjunct create a BDD
		for (const auto& [disjunct_lhs, disjunct_rhs] : lhs_to_disjuncts_and_states_[&formula_node]) {
			// create bdd for the whole disjunct
			const auto bdd = (disjunct_lhs == disjunct_rhs) ? // disjunct contains only states
								 OptionalBdd(bdd_mng_.bddOne())
															: // transition from state to states -> add true as symbol
								 graph_to_bdd_afa(*disjunct_lhs);
			MATA_ASSERT(bdd.type == OptionalBdd::Type::BddE);
			if (bdd.val.IsZero()) { continue; }
			trans_to_bddvar_[disjunct_lhs] = bdd.val;
			add_source_bdd(bdd.val);
		}
	}
}

std::unordered_set<BDD> mata::Mintermization::compute_minterms(const std::unordered_set<BDD>& source_bdds) const {
	std::unordered_set<BDD> stack{bdd_mng_.bddOne()};
	for (const BDD& b : source_bdds) {
		std::unordered_set<BDD> next;
		/**
		 * TODO: Possible optimization - we can remember which transition belongs to the currently processed bdds
		 * and mintermize automaton somehow directly here. However, it would be better to do such optimization
		 * in copy of this function and this one keep clean and straightforward.
		 */
		for (const auto& minterm : stack) {
			if (BDD b1 = minterm * b; !b1.IsZero()) { next.insert(b1); }
			if (BDD b0 = minterm * !b; !b0.IsZero()) { next.insert(b0); }
		}
		stack = next;
	}

	return stack;
}

mata::Mintermization::MintermPartition mata::Mintermization::compute_minterms_with_incidence(
	const std::vector<BDD>& source_bdds
) const {
	if (source_bdds.empty()) {
		return {std::vector<BDD>{bdd_mng_.bddOne()}, std::vector<std::vector<size_t>>{}};
	}

	// Helper lambda to create an incidence representation
	auto make_incidence = [&]() -> std::variant<uint64_t, std::vector<size_t>> {
		if (source_bdds.size() <= 64) {
			return uint64_t(0);
		} else {
			return std::vector<size_t>();
		}
	};

	auto set_bit = [](auto& incidence, size_t idx) {
		if (std::holds_alternative<uint64_t>(incidence)) {
			std::get<uint64_t>(incidence) |= (1ULL << idx);
		} else {
			auto& vec = std::get<std::vector<size_t>>(incidence);
			if (vec.empty() || vec.back() != idx) {
				vec.push_back(idx);
			}
		}
	};

	auto get_mask = [](const auto& incidence) -> uint64_t {
		if (std::holds_alternative<uint64_t>(incidence)) {
			return std::get<uint64_t>(incidence);
		}
		return 0;
	};

	auto get_vec = [](const auto& incidence) -> std::vector<size_t> {
		if (std::holds_alternative<std::vector<size_t>>(incidence)) {
			return std::get<std::vector<size_t>>(incidence);
		}
		return {};
	};

	// Region: BDD value + incidence (which sources cover it)
	struct Region {
		BDD value;
		std::variant<uint64_t, std::vector<size_t>> incidence;
	};

	std::vector<Region> current;
	current.push_back({bdd_mng_.bddOne(), make_incidence()});

	// Refinement loop
	for (size_t i = 0; i < source_bdds.size(); ++i) {
		const BDD& source = source_bdds[i];
		std::vector<Region> next;

		for (auto& region : current) {
			// Positive child: inside source (covered by source i)
			if (BDD b1 = region.value * source; !b1.IsZero()) {
				auto new_incidence = region.incidence;
				set_bit(new_incidence, i);
				next.push_back({b1, new_incidence});
			}

			// Negative child: outside source (incidence unchanged)
			if (BDD b0 = region.value * !source; !b0.IsZero()) {
				next.push_back({b0, region.incidence});
			}
		}

		current = std::move(next);
	}

	// Build result
	MintermPartition result;
	result.minterms = std::vector<BDD>();
	result.minterms_of_source.resize(source_bdds.size());

	for (size_t minterm_idx = 0; minterm_idx < current.size(); ++minterm_idx) {
		result.minterms.push_back(current[minterm_idx].value);

		// Invert incidence: for each source covered by this minterm, add minterm_idx to minterms_of_source[i]
		if (source_bdds.size() <= 64) {
			uint64_t mask = get_mask(current[minterm_idx].incidence);
			for (size_t i = 0; i < source_bdds.size(); ++i) {
				if ((mask & (1ULL << i)) != 0) {
					result.minterms_of_source[i].push_back(minterm_idx);
				}
			}
		} else {
			auto vec = get_vec(current[minterm_idx].incidence);
			for (size_t idx : vec) {
				result.minterms_of_source[idx].push_back(minterm_idx);
			}
		}
	}

	return result;
}

mata::Mintermization::OptionalBdd mata::Mintermization::graph_to_bdd_afa(const FormulaGraph& graph) {
	if (const FormulaNode& node = graph.node; node.is_operand()) {
		if (node.is_state()) { return OptionalBdd(OptionalBdd::Type::NothingE); }
		if (symbol_to_bddvar_.contains(node.name)) {
			return OptionalBdd(symbol_to_bddvar_.at(node.name));
		} else {
			const BDD res =
				(node.is_true()) ? bdd_mng_.bddOne() : (node.is_false() ? bdd_mng_.bddZero() : bdd_mng_.bddVar());
			symbol_to_bddvar_[node.name] = res;
			return OptionalBdd(res);
		}
	} else if (node.is_operator()) {
		if (node.operator_type == FormulaNode::OperatorType::And) {
			MATA_ASSERT(graph.children.size() == 2);
			const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
			const OptionalBdd op2 = graph_to_bdd_afa(graph.children[1]);
			return op1 * op2;
		} else if (node.operator_type == FormulaNode::OperatorType::Or) {
			MATA_ASSERT(graph.children.size() == 2);
			const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
			const OptionalBdd op2 = graph_to_bdd_afa(graph.children[1]);
			return op1 + op2;
		} else if (node.operator_type == FormulaNode::OperatorType::Neg) {
			MATA_ASSERT(graph.children.size() == 1);
			const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
			return !op1;
		} else {
			MATA_ASSERT(false, "Unknown type of operation. It should conjunction, disjunction, or negation.");
		}
	}

	MATA_ASSERT(false);
	return {};
}

BDD mata::Mintermization::graph_to_bdd_nfa(const FormulaGraph& graph) {
	if (const FormulaNode& node = graph.node; node.is_operand()) {
		if (symbol_to_bddvar_.contains(node.name)) {
			return symbol_to_bddvar_.at(node.name);
		} else {
			BDD res = (node.is_true()) ? bdd_mng_.bddOne() : (node.is_false() ? bdd_mng_.bddZero() : bdd_mng_.bddVar());
			symbol_to_bddvar_[node.name] = res;
			return res;
		}
	} else if (node.is_operator()) {
		if (node.operator_type == FormulaNode::OperatorType::And) {
			MATA_ASSERT(graph.children.size() == 2);
			const BDD op1 = graph_to_bdd_nfa(graph.children[0]);
			const BDD op2 = graph_to_bdd_nfa(graph.children[1]);
			return op1 * op2;
		} else if (node.operator_type == FormulaNode::OperatorType::Or) {
			MATA_ASSERT(graph.children.size() == 2);
			const BDD op1 = graph_to_bdd_nfa(graph.children[0]);
			const BDD op2 = graph_to_bdd_nfa(graph.children[1]);
			return op1 + op2;
		} else if (node.operator_type == FormulaNode::OperatorType::Neg) {
			MATA_ASSERT(graph.children.size() == 1);
			const BDD op1 = graph_to_bdd_nfa(graph.children[0]);
			return !op1;
		} else {
			MATA_ASSERT(false);
		}
	}

	MATA_ASSERT(false);
	return {};
}

void mata::Mintermization::minterms_to_aut_nfa(
	IntermediateAut& res, const IntermediateAut& aut, const std::vector<std::vector<size_t>>& minterms_of_source
) {
	for (const auto& [formula_node, formula_graph] : aut.transitions) {
		// for each t=(q1,s,q2)
		const auto& symbol_part = formula_graph.children[0];

		if (!trans_to_bddvar_.contains(&symbol_part)) {
			continue; // Transition had zero bdd so it was not added to map
		}
		const BDD& bdd = trans_to_bddvar_[&symbol_part];

		// Look up the source index for this BDD
		if (!source_bdd_to_index_.contains(bdd)) {
			continue; // Transition BDD not found in sources (should not happen)
		}
		size_t source_idx = source_bdd_to_index_.at(bdd);

		// Emit edges for each minterm below this source
		for (size_t minterm_idx : minterms_of_source[source_idx]) {
			IntermediateAut::parse_transition(
				res, {formula_node.raw, std::to_string(minterm_idx), formula_graph.children[1].node.raw}
			);
		}
	}
}

void mata::Mintermization::minterms_to_aut_afa(
	IntermediateAut& res, const IntermediateAut& aut, const std::vector<std::vector<size_t>>& minterms_of_source
) {
	for (const auto& formula_node : aut.transitions | std::views::keys) {
		for (const auto& [disjunct, formula_graph] : lhs_to_disjuncts_and_states_[&formula_node]) {
			// for each t=(q1,s,q2)
			if (!trans_to_bddvar_.contains(disjunct)) {
				continue; // Transition had zero bdd so it was not added to map
			}
			const BDD& bdd = trans_to_bddvar_[disjunct];

			// Look up the source index for this BDD
			if (!source_bdd_to_index_.contains(bdd)) {
				continue; // Transition BDD not found in sources (should not happen)
			}
			size_t source_idx = source_bdd_to_index_.at(bdd);

			// Emit edges for each minterm below this source
			for (size_t minterm_idx : minterms_of_source[source_idx]) {
				const auto str_symbol = std::to_string(minterm_idx);
				FormulaNode node_symbol(
					FormulaNode::Type::Operand, str_symbol, str_symbol, FormulaNode::OperandType::Symbol
				);
				if (formula_graph != nullptr) {
					res.add_transition(formula_node, node_symbol, *formula_graph);
				} else { // transition without state on the right-handed side
					res.add_transition(formula_node, node_symbol);
				}
			}
		}
	}
}

mata::IntermediateAut mata::Mintermization::mintermize(const IntermediateAut& aut) {
	return mintermize(std::vector<const IntermediateAut*>{&aut})[0];
}

std::vector<mata::IntermediateAut> mata::Mintermization::mintermize(const std::vector<const IntermediateAut*>& auts) {
	// Initialize source BDD tracking structures
	source_bdds_.clear();
	source_bdd_to_index_.clear();

	for (const IntermediateAut* aut : auts) {
		if ((!aut->is_nfa() && !aut->is_afa()) || aut->alphabet_type != IntermediateAut::AlphabetType::Bitvector) {
			throw std::runtime_error("We currently support mintermization only for NFA and AFA with bitvectors");
		}

		aut->is_nfa() ? trans_to_bdd_nfa(*aut) : trans_to_bdd_afa(*aut);
	}

	// Build minterm tree over BDDs with incidence tracking
	const auto partition = compute_minterms_with_incidence(source_bdds_);

	std::vector<IntermediateAut> res;
	for (const IntermediateAut* aut : auts) {
		IntermediateAut mintermized_aut = *aut;
		mintermized_aut.alphabet_type = IntermediateAut::AlphabetType::Explicit;
		mintermized_aut.transitions.clear();

		if (aut->is_nfa()) {
			minterms_to_aut_nfa(mintermized_aut, *aut, partition.minterms_of_source);
		} else if (aut->is_afa()) {
			minterms_to_aut_afa(mintermized_aut, *aut, partition.minterms_of_source);
		}

		res.push_back(mintermized_aut);
	}

	return res;
}

std::vector<mata::IntermediateAut> mata::Mintermization::mintermize(const std::vector<IntermediateAut>& auts) {
	std::vector<const IntermediateAut*> auts_pointers;
	for (const IntermediateAut& aut : auts) { auts_pointers.push_back(&aut); }
	return mintermize(auts_pointers);
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator*(const OptionalBdd& b) const {
	if (this->type == Type::NothingE) {
		return b;
	} else if (b.type == Type::NothingE) {
		return *this;
	} else {
		return OptionalBdd{Type::BddE, this->val * b.val};
	}
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator+(const OptionalBdd& b) const {
	if (this->type == Type::NothingE) {
		return b;
	} else if (b.type == Type::NothingE) {
		return *this;
	} else {
		return OptionalBdd{Type::BddE, this->val + b.val};
	}
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator!() const {
	if (this->type == Type::NothingE) {
		return OptionalBdd(Type::NothingE);
	} else {
		return OptionalBdd{Type::BddE, !this->val};
	}
}
