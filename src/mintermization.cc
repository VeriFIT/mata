/*
 * mintermization.hh -- Mintermization of automaton.
 *
 * It transforms an automaton with a bitvector formula used a symbol to mintermized version of the automaton.
 */

#include "mata/parser/mintermization.hh"
#include "mata/utils/assert.hh"

#include <algorithm>
#include <ranges>

// TODO(afa): Re-add the alternating automata support. The AFA code is kept, but not compiled: `MATA_AFA_SUPPORT` is
//  never defined.
#ifdef MATA_AFA_SUPPORT
namespace {
const mata::FormulaGraph* detect_state_part(const mata::FormulaGraph* node) {
	if (node->node.is_state()) { return node; }

	std::vector<const mata::FormulaGraph*> worklist{node};
	while (!worklist.empty()) {
		const auto* const act_node = worklist.back();
		MATA_ASSERT(act_node != nullptr);
		worklist.pop_back();
		if (act_node->children.size() != 2) { continue; }

		if (act_node->children.front().node.is_and() && act_node->children[1].node.is_state()) {
			return act_node; // ... & a1 & q1 ... & qn
		}
		if (act_node->children.front().node.is_state() && act_node->children[1].node.is_and()) {
			return act_node; // ... & a1 & q1 ... & qn
		}
		if (act_node->children.front().node.is_state() && act_node->children[1].node.is_state()) {
			return act_node; // ... & a1 & q1 & q2
		}
		if (act_node->children.front().node.is_state() && act_node->children[1].node.is_state()) {
			return act_node; // ... & a1 & q1 & q2
		}
		if (act_node->node.is_operator() && act_node->children[1].node.is_state()) {
			return &act_node->children[1]; // a1 & q1
		}
		if (act_node->children.front().node.is_state() && act_node->node.is_operator()) {
			return &act_node->children.front(); // a1 & q1
		}
		for (const mata::FormulaGraph& child : act_node->children) { worklist.push_back(&child); }
	}

	return nullptr;
}
} // namespace
#endif

void mata::Mintermization::add_source_bdd(const BDD& bdd) {
	if (source_bdd_to_index_.contains(bdd)) {
		return; // Already registered
	}
	source_bdd_to_index_[bdd] = source_bdds_.size();
	source_bdds_.push_back(bdd);
}

void mata::Mintermization::trans_to_bdd_nfa(const IntermediateAut& aut) {
	MATA_ASSERT(aut.is_nfa());

	for (const auto& trans : aut.transitions) {
		// Foreach transition create a BDD
		const FormulaGraphView symbol_part{aut.get_symbol_part_of_transition(trans)};
		const BDD bdd = graph_to_bdd_nfa(symbol_part);
		if (bdd.IsZero()) { continue; }
		add_source_bdd(bdd);
		trans_to_bddvar_[&symbol_part.node()] = bdd;
	}
}

#ifdef MATA_AFA_SUPPORT
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
#endif

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
			if (BDD const b1 = minterm * b; !b1.IsZero()) { next.insert(b1); }
			if (BDD const b0 = minterm * !b; !b0.IsZero()) { next.insert(b0); }
		}
		stack = next;
	}

	return stack;
}

mata::Mintermization::MintermPartition
	mata::Mintermization::compute_minterms_with_incidence(const std::vector<BDD>& source_bdds) const {
	if (source_bdds.empty()) {
		return {
			.minterms = std::vector<BDD>{bdd_mng_.bddOne()}, .minterms_of_source = std::vector<std::vector<size_t>>{}
		};
	}

	// Helper lambda to create an incidence representation
	auto make_incidence = [&]() -> std::variant<uint64_t, std::vector<size_t>> {
		if (source_bdds.size() <= 64) { return uint64_t(0); }
		return std::vector<size_t>();
	};

	auto set_bit = [](auto& incidence, size_t idx) {
		if (std::holds_alternative<uint64_t>(incidence)) {
			std::get<uint64_t>(incidence) |= (1ULL << idx);
		} else {
			auto& vec = std::get<std::vector<size_t>>(incidence);
			if (vec.empty() || vec.back() != idx) { vec.push_back(idx); }
		}
	};

	auto get_mask = [](const auto& incidence) -> uint64_t {
		if (std::holds_alternative<uint64_t>(incidence)) { return std::get<uint64_t>(incidence); }
		return 0;
	};

	// Region: BDD value + incidence (which sources cover it)
	struct Region {
		BDD value;
		std::variant<uint64_t, std::vector<size_t>> incidence;
	};

	std::vector<Region> current;
	current.push_back({bdd_mng_.bddOne(), make_incidence()});

	// Refinement loop. Two Leq tests per (region, source) pair: a region is usually disjoint from the source or
	//  inside it, and only a region that properly overlaps it pays for the two products. Disjointness is tested
	//  first because most sources cut out most regions.
	for (size_t i = 0; i < source_bdds.size(); ++i) {
		const BDD& source = source_bdds[i];
		const BDD not_source = !source;
		std::vector<Region> next;

		for (auto& region : current) {
			if (region.value <= not_source) {
				// Disjoint: the positive child is empty, the negative one is the region itself.
				next.push_back(std::move(region));
				continue;
			}
			if (region.value <= source) {
				// The whole region is inside the source: only the positive child survives, and it is the region.
				next.push_back(std::move(region));
				set_bit(next.back().incidence, i);
				continue;
			}
			if (const BDD b1 = region.value * source; !b1.IsZero()) {
				auto new_incidence = region.incidence;
				set_bit(new_incidence, i);
				next.push_back({b1, std::move(new_incidence)});
			}

			// Negative child: outside source (incidence unchanged, moved: last use of the region)
			if (const BDD b0 = region.value * not_source; !b0.IsZero()) {
				next.push_back({b0, std::move(region.incidence)});
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
			const uint64_t mask = get_mask(current[minterm_idx].incidence);
			for (size_t i = 0; i < source_bdds.size(); ++i) {
				if ((mask & (1ULL << i)) != 0) { result.minterms_of_source[i].push_back(minterm_idx); }
			}
		} else {
			const auto& vec = std::get<std::vector<size_t>>(current[minterm_idx].incidence);
			for (const size_t idx : vec) { result.minterms_of_source[idx].push_back(minterm_idx); }
		}
	}

	return result;
}

#ifdef MATA_AFA_SUPPORT
mata::Mintermization::OptionalBdd mata::Mintermization::graph_to_bdd_afa(const FormulaGraph& graph) {
	const FormulaNode& node = graph.node;
	if (node.is_operand()) {
		if (node.is_state()) { return OptionalBdd(OptionalBdd::Type::NothingE); }
		return OptionalBdd(symbol_bdd(
			node.name(), node.is_true() ? std::optional{true} : (node.is_false() ? std::optional{false} : std::nullopt)
		));
	}

	if (node.is_operator()) {
		if (node.operator_type == FormulaNode::OperatorType::And) {
			MATA_ASSERT(graph.children.size() == 2);
			const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
			const OptionalBdd op2 = graph_to_bdd_afa(graph.children[1]);
			return op1 * op2;
		}
		if (node.operator_type == FormulaNode::OperatorType::Or) {
			MATA_ASSERT(graph.children.size() == 2);
			const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
			const OptionalBdd op2 = graph_to_bdd_afa(graph.children[1]);
			return op1 + op2;
		}
		if (node.operator_type == FormulaNode::OperatorType::Neg) {
			MATA_ASSERT(graph.children.size() == 1);
			const OptionalBdd op1 = graph_to_bdd_afa(graph.children[0]);
			return !op1;
		}
		MATA_ASSERT(false, "Unknown type of operation. It should conjunction, disjunction, or negation.");
	}

	MATA_ASSERT(false);
	return {};
}
#endif

const BDD& mata::Mintermization::symbol_bdd(const std::string_view name, const std::optional<bool> constant) {
	if (const auto it{symbol_to_bddvar_.find(name)}; it != symbol_to_bddvar_.end()) { return it->second; }
	const BDD res{constant.has_value() ? (*constant ? bdd_mng_.bddOne() : bdd_mng_.bddZero()) : bdd_mng_.bddVar()};
	const auto it{symbol_to_bddvar_.emplace(name, res).first};
	added_symbols_.push_back(&it->first);
	return it->second;
}

void mata::Mintermization::rollback(const Checkpoint& checkpoint) {
	for (size_t i{checkpoint.num_of_symbols}; i < added_symbols_.size(); ++i) {
		symbol_to_bddvar_.erase(symbol_to_bddvar_.find(*added_symbols_[i]));
	}
	added_symbols_.resize(checkpoint.num_of_symbols);
	for (size_t i{checkpoint.num_of_formulas}; i < added_formulas_.size(); ++i) {
		formula_to_bdd_.erase(formula_to_bdd_.find(*added_formulas_[i]));
	}
	added_formulas_.resize(checkpoint.num_of_formulas);
}

namespace {
/// Appends to @p key a serialization of the formula @p graph that determines its BDD: node kinds, names, and the
///  shape of the tree in postfix order. Each node starts with a backslash and its type byte, which differs between
///  operands and operators, and the name of an operand follows with its backslashes doubled, so no name can mimic
///  the start of another node.
void append_formula_key(std::string& key, const mata::FormulaGraphView graph) {
	constexpr char OPERATOR_FLAG{0x40};
	for (const mata::FormulaGraphEntry& entry : graph.entries()) {
		const mata::FormulaNode& node{entry.node};
		key.push_back('\\');
		if (!node.is_operand()) {
			key.push_back(static_cast<char>(static_cast<char>(node.operator_type) | OPERATOR_FLAG));
			continue;
		}
		key.push_back(static_cast<char>(node.operand_type));
		for (const char c : node.name()) {
			key.push_back(c);
			if (c == '\\') { key.push_back('\\'); } // Escape the escape byte.
		}
	}
}
} // namespace

BDD mata::Mintermization::graph_to_bdd_nfa(const FormulaGraphView graph) {
	// A single operand has its own lookup.
	if (graph.node().is_operand()) { return convert_graph_to_bdd_nfa(graph); }
	formula_key_.clear();
	append_formula_key(formula_key_, graph);
	if (const auto it{formula_to_bdd_.find(formula_key_)}; it != formula_to_bdd_.end()) { return it->second; }
	const BDD bdd{convert_graph_to_bdd_nfa(graph)};
	added_formulas_.push_back(&formula_to_bdd_.emplace(formula_key_, bdd).first->first);
	return bdd;
}

BDD mata::Mintermization::convert_graph_to_bdd_nfa(const FormulaGraphView graph) {
	// The nodes in postfix order meet the operands from left to right, as a recursive descent would: the operands
	//  meeting a symbol first create its variable, which fixes its index.
	std::vector<BDD>& operands{operand_bdds_};
	operands.clear(); // Left over when a conversion threw.
	const std::span<const FormulaGraphEntry> entries{graph.entries()};
	for (size_t i{0}; i < entries.size(); ++i) {
		const FormulaNode& node{entries[i].node};
		if (node.is_operand()) {
			const BDD& bdd{symbol_bdd(
				node.name(),
				node.is_true() ? std::optional{true} : (node.is_false() ? std::optional{false} : std::nullopt)
			)};
			// The negations of the operand, folded in, spare a reassignment each; BDD has no move.
			bool negated{false};
			for (; i + 1 < entries.size() && entries[i + 1].node.is_neg(); ++i) { negated = !negated; }
			if (negated) {
				operands.push_back(!bdd);
			} else {
				operands.push_back(bdd);
			}
		} else if (node.is_neg()) {
			operands.back() = !operands.back();
		} else {
			BDD& lhs{operands[operands.size() - 2]};
			const BDD& rhs{operands.back()};
			if (node.is_and()) {
				lhs *= rhs;
			} else {
				MATA_ASSERT(node.operator_type == FormulaNode::OperatorType::Or);
				lhs += rhs;
			}
			operands.pop_back();
		}
	}
	if (operands.size() != 1) { // Only the view of an empty graph, whose single node is no operand.
		MATA_ASSERT(false);
		operands.clear();
		return {};
	}
	BDD bdd{operands.back()};
	operands.clear();
	return bdd;
}

namespace {
/// One leaf per minterm holding its symbol, copied into every transition over that symbol.
std::vector<mata::FormulaGraph> symbol_leaves(const size_t num_of_minterms, mata::NameStore& names) {
	std::vector<mata::FormulaGraph> leaves{};
	leaves.reserve(num_of_minterms);
	for (size_t symbol{0}; symbol < num_of_minterms; ++symbol) {
		leaves.emplace_back(
			mata::FormulaNode{
				mata::FormulaNode::Type::Operand, names.store(std::to_string(symbol)),
				mata::FormulaNode::OperandType::Symbol
			}
		);
	}
	return leaves;
}

/// The number of minterms that lie below some source BDD, that is, the symbols a conversion can emit.
size_t num_of_symbols(const std::vector<std::vector<size_t>>& minterms_of_source) {
	size_t num_of_symbols{0};
	for (const std::vector<size_t>& minterms : minterms_of_source) {
		if (!minterms.empty()) { num_of_symbols = std::max(num_of_symbols, minterms.back() + 1); }
	}
	return num_of_symbols;
}

/// The ascending numbers of the minterms below the source BDD @p bdd, or @c nullptr if @p bdd is not a source BDD
///  (should not happen).
const std::vector<size_t>* minterms_below(
	const BDD& bdd,
	const std::unordered_map<BDD, size_t>& source_bdd_to_index,
	const std::vector<std::vector<size_t>>& minterms_of_source
) {
	const auto source{source_bdd_to_index.find(bdd)};
	return source == source_bdd_to_index.end() ? nullptr : &minterms_of_source[source->second];
}
} // namespace

void mata::Mintermization::minterms_to_aut_nfa(
	IntermediateAut& res, const IntermediateAut& aut, const std::vector<std::vector<size_t>>& minterms_of_source
) {
	const std::vector<FormulaGraph> symbols{symbol_leaves(num_of_symbols(minterms_of_source), *res.name_store)};
	const FormulaNode conjunction{FormulaNode::Type::Operator, "&", FormulaNode::OperatorType::And};
	// The minterms below the BDD of the symbol part of a transition, or nullptr if the transition had a zero BDD and
	//  so was not added to the map.
	const auto minterms_of_transition{[&](const FormulaGraph& formula_graph) -> const std::vector<size_t>* {
		const auto bdd{trans_to_bddvar_.find(&formula_graph.children()[0].node())};
		if (bdd == trans_to_bddvar_.end()) { return nullptr; }
		return minterms_below(bdd->second, source_bdd_to_index_, minterms_of_source);
	}};

	// Two passes: the first one sizes the result, the second one fills it.
	size_t num_of_transitions{0};
	for (const FormulaGraph& formula_graph : aut.transitions | std::views::values) {
		if (const std::vector<size_t>* minterms{minterms_of_transition(formula_graph)}; minterms != nullptr) {
			num_of_transitions += minterms->size();
		}
	}
	res.transitions.reserve(res.transitions.size() + num_of_transitions);

	for (const auto& [formula_node, formula_graph] : aut.transitions) {
		// for each t=(q1,s,q2)
		const std::vector<size_t>* minterms{minterms_of_transition(formula_graph)};
		if (minterms == nullptr) { continue; }
		// For each minterm x below BDD_s, add q1,x,q2 to transitions.
		const FormulaGraphView target{formula_graph.children()[1]};
		for (const size_t symbol : *minterms) {
			res.transitions.emplace_back(formula_node, FormulaGraph{conjunction, symbols[symbol], target});
		}
	}
}

#ifdef MATA_AFA_SUPPORT
void mata::Mintermization::minterms_to_aut_afa(
	IntermediateAut& res, const IntermediateAut& aut, const std::vector<std::vector<size_t>>& minterms_of_source
) {
	const std::vector<FormulaGraph> symbols{symbol_leaves(num_of_symbols(minterms_of_source), *res.name_store)};
	const FormulaNode conjunction{FormulaNode::Type::Operator, "&", FormulaNode::OperatorType::And};
	for (const auto& formula_node : aut.transitions | std::views::keys) {
		for (const auto& [disjunct, formula_graph] : lhs_to_disjuncts_and_states_[&formula_node]) {
			// for each t=(q1,s,q2)
			const auto bdd{trans_to_bddvar_.find(disjunct)};
			if (bdd == trans_to_bddvar_.end()) {
				continue; // Transition had zero bdd so it was not added to map
			}
			const std::vector<size_t>* minterms{minterms_below(bdd->second, source_bdd_to_index_, minterms_of_source)};
			if (minterms == nullptr) { continue; }
			// For each minterm x below BDD_s, add q1,x,q2 to transitions.
			for (const size_t symbol : *minterms) {
				if (formula_graph != nullptr) {
					FormulaGraph graph{conjunction};
					graph.children.push_back(symbols[symbol]);
					graph.children.push_back(*formula_graph);
					res.transitions.emplace_back(formula_node, std::move(graph));
				} else { // transition without state on the right-handed side
					res.transitions.emplace_back(formula_node, symbols[symbol]);
				}
			}
		}
	}
}
#endif

mata::IntermediateAut mata::Mintermization::mintermize(const IntermediateAut& aut) {
	return std::move(mintermize(std::vector<const IntermediateAut*>{&aut})[0]);
}

/**
 * Mintermize all input automata over one shared minterm partition, preserving non-transition metadata while replacing
 * bitvector transitions with explicit symbols.
 */
std::vector<mata::IntermediateAut> mata::Mintermization::mintermize(const std::vector<const IntermediateAut*>& auts) {
	// The state of an earlier call would add its BDDs to the partition, and its maps are keyed by addresses into
	//  automata that may be gone. The symbol variables stay: the same name keeps its variable across calls.
	source_bdds_.clear();
	source_bdd_to_index_.clear();
	trans_to_bddvar_.clear();
#ifdef MATA_AFA_SUPPORT
	lhs_to_disjuncts_and_states_.clear();
#endif

	for (const IntermediateAut* aut : auts) {
#ifdef MATA_AFA_SUPPORT
		if ((!aut->is_nfa() && !aut->is_afa()) || aut->alphabet_type != IntermediateAut::AlphabetType::Bitvector) {
			throw std::runtime_error("We currently support mintermization only for NFA and AFA with bitvectors");
		}

		aut->is_nfa() ? trans_to_bdd_nfa(*aut) : trans_to_bdd_afa(*aut);
#else
		if (!aut->is_nfa() || aut->alphabet_type != IntermediateAut::AlphabetType::Bitvector) {
			throw std::runtime_error("We currently support mintermization only for NFA with bitvectors");
		}

		trans_to_bdd_nfa(*aut);
#endif
	}

	// Build minterm tree over BDDs with incidence tracking
	const auto partition = compute_minterms_with_incidence(source_bdds_);

	std::vector<IntermediateAut> res;
	for (const IntermediateAut* aut : auts) {
		// The transitions are replaced below; copying their formula trees only to clear them dominates large inputs.
		IntermediateAut mintermized_aut;
		// The formula and transition copies below keep the names of their nodes as views into the source automaton,
		//  so sharing its name storages keeps them valid for as long as the mintermized automaton lives.
		mintermized_aut.name_store = aut->name_store;
		mintermized_aut.section_names = aut->section_names;
		mintermized_aut.state_naming = aut->state_naming;
		mintermized_aut.symbol_naming = aut->symbol_naming;
		mintermized_aut.node_naming = aut->node_naming;
		mintermized_aut.alphabet_type = IntermediateAut::AlphabetType::Explicit;
		mintermized_aut.automaton_type = aut->automaton_type;
		mintermized_aut.states_names = aut->states_names;
		mintermized_aut.symbols_names = aut->symbols_names;
		mintermized_aut.nodes_names = aut->nodes_names;
		mintermized_aut.initial_formula = aut->initial_formula;
		mintermized_aut.final_formula = aut->final_formula;
		mintermized_aut.initial_enumerated = aut->initial_enumerated;
		mintermized_aut.final_enumerated = aut->final_enumerated;

		if (aut->is_nfa()) {
			minterms_to_aut_nfa(mintermized_aut, *aut, partition.minterms_of_source);
#ifdef MATA_AFA_SUPPORT
		} else if (aut->is_afa()) {
			minterms_to_aut_afa(mintermized_aut, *aut, partition.minterms_of_source);
#endif
		}

		res.push_back(std::move(mintermized_aut));
	}

	return res;
}

std::vector<mata::IntermediateAut> mata::Mintermization::mintermize(const std::vector<IntermediateAut>& auts) {
	std::vector<const IntermediateAut*> auts_pointers;
	auts_pointers.reserve(auts.size());
	for (const IntermediateAut& aut : auts) { auts_pointers.push_back(&aut); }
	return mintermize(auts_pointers);
}

#ifdef MATA_AFA_SUPPORT
mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator*(const OptionalBdd& b) const {
	if (this->type == Type::NothingE) { return b; }
	if (b.type == Type::NothingE) { return *this; }
	return OptionalBdd{Type::BddE, this->val * b.val};
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator+(const OptionalBdd& b) const {
	if (this->type == Type::NothingE) { return b; }
	if (b.type == Type::NothingE) { return *this; }
	return OptionalBdd{Type::BddE, this->val + b.val};
}

mata::Mintermization::OptionalBdd mata::Mintermization::OptionalBdd::operator!() const {
	if (this->type == Type::NothingE) { return OptionalBdd(Type::NothingE); }
	return OptionalBdd{Type::BddE, !this->val};
}
#endif
