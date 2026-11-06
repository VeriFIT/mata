/* nft-incl.cc -- NFT language inclusion
 */

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

// MATA headers
#include "mata/nfa/algorithms.hh"
#include "mata/nft/algorithms.hh"
#include "mata/nft/nft.hh"

#include "../nfa/antichain-inclusion.hh"
#include "nft-internal.hh"

using namespace mata::nft;
using namespace mata::utils;
using mata::Symbol;

namespace {
using mata::nfa::internal::Node;
using mata::nfa::internal::ProcessedAntichain;
using mata::nfa::internal::Step;

/// The replacements of DONT_CARE that the unwinding of @p lhs and @p rhs uses: the symbols of @p alphabet, or the
///  symbols of both transducers without DONT_CARE (unless DONT_CARE is their only symbol).
OrdVector<Symbol> unwinding_symbols(const Nft& lhs, const Nft& rhs, const mata::Alphabet* const alphabet) {
	if (alphabet != nullptr) { return alphabet->get_alphabet_symbols(); }
	OrdVector<Symbol> symbols{create_alphabet(lhs, rhs).get_alphabet_symbols()};
	if (symbols.contains(DONT_CARE) && symbols.size() > 1) { symbols.erase(DONT_CARE); }
	return symbols;
}

/**
 * Whether unwinding @p nft with @p symbols spells DONT_CARE out over them: on a DONT_CARE transition, or on the links
 *  that pad a jump under AppendDontCares. Only then is the lazy exploration used: otherwise the unwound transducer is
 *  at most linear in the jump lengths, and the NFA inclusion on it is faster per explored node.
 */
bool spells_out_dont_care(const Nft& nft, const OrdVector<Symbol>& symbols, const JumpMode jump_mode) {
	if (symbols == OrdVector<Symbol>{DONT_CARE}) { return false; }
	if (jump_mode == JumpMode::AppendDontCares && nft.contains_jump_transitions()) { return true; }
	return std::ranges::any_of(nft.delta, [](const StatePost& state_post) {
		return state_post.find(DONT_CARE) != state_post.end();
	});
}

/*
 * Lazy exploration of jumps for the antichain inclusion (#791).
 *
 * The eager reference unwinds both operands by Nft::unwind_jumps(symbols, jump_mode) and runs the NFA antichain
 *  inclusion on the results read as flat automata, where EPSILON is an ordinary letter. Unwinding rewrites each
 *  transition s -a-> d, with ls = level(s), ld = level(d) and N levels, by its unsigned span
 *  diff = (ld == 0 ? N - ls : ld - ls):
 *  - diff == 0 (ld == ls != 0): one link over the letter a, kept as it is (also a DONT_CARE);
 *  - diff == 1: one link over expand(a);
 *  - diff > 1: a chain of k links through k - 1 fresh states, k = diff when ld == 0 or ld > ls, and k = 2 when
 *    0 < ld < ls (the span wraps around). The first link reads expand(a); every later one reads expand(a) under
 *    RepeatSymbol and NoJump, and expand(DONT_CARE) under AppendDontCares.
 *  Here expand(x) = symbols when x is DONT_CARE and symbols != {DONT_CARE}, and {x} otherwise: each link of a DONT_CARE
 *  reads any symbol independently, also under RepeatSymbol, and with no symbols it reads nothing.
 *
 * Cursor: instead of the fresh states, a cursor {state d, remaining links r >= 1, label} stands for the position inside
 *  a chain. Relative to {state, current_level, stop_level, mode, symbol}, r counts the levels from current_level to
 *  stop_level (the level of d) and the label is what (mode, symbol) read on every remaining link: expand(a) under
 *  RepeatSymbol and NoJump, expand(DONT_CARE) under AppendDontCares. A cursor has exactly one move: it reads its label
 *  and goes to {d, r - 1, label}, or to d when r == 1. It is never final and takes no other move, as the fresh states
 *  of the eager chain; EPSILON is a letter of a link like any other, so there is no closure to take. Chains of
 *  different transitions with the same target, remaining length and label share a cursor: the states they replace
 *  have the same future, so every language stays the same.
 *
 * A link reads a concrete letter c or the wildcard expand(DONT_CARE) = symbols, stored as the letter DONT_CARE (when
 *  symbols == {DONT_CARE}, DONT_CARE is a concrete letter and there is no wildcard). A concrete c of the smaller side
 *  meets the bigger's moves over c and, when c is in symbols, its wildcard moves; nothing is spelled out over symbols.
 *  A wildcard of the smaller side reads every letter of symbols: when some letter of symbols has no concrete bigger
 *  move, the successor over it (the bigger's wildcard targets only) is a subset of the successor over any other
 *  letter, so it alone is explored; otherwise there is one successor per letter, the letters being exactly the
 *  concrete ones of the bigger moves, and the wildcard move is merged with the smaller's concrete move over each.
 *  The distances to a final state, which prune hopeless nodes and complete counterexamples, weigh a transition by
 *  its number of links.
 *
 * The search is the NFA antichain inclusion over these virtual states, with the same synchronized iteration over the
 *  moves of a macrostate and the same processed antichain: a node pairs one virtual state of the smaller transducer,
 *  that is an original state or a cursor (the phase), with a set of virtual states of the bigger one, and subsumption
 *  only compares nodes with the same virtual smaller state.
 *
 * Left to the eager reference, where the unwinding is not uniform: symbols holding DONT_CARE next to other symbols
 *  (a one-level DONT_CARE transition loses its DONT_CARE replacement, as the added and the removed transition
 *  coincide, but a chain link keeps it), and a DONT_CARE transition that stays on its level (a literal DONT_CARE next
 *  to the wildcard ones).
 */
class LazyUnwinding {
  public:
	static constexpr size_t unreachable{std::numeric_limits<size_t>::max()};

	LazyUnwinding(const Nft& nft, const OrdVector<Symbol>& symbols, const JumpMode jump_mode)
		: nft_{nft},
		  symbols_{symbols},
		  jump_mode_{jump_mode},
		  has_wildcard_{symbols != OrdVector<Symbol>{DONT_CARE}},
		  exact_{!has_wildcard_ || !symbols.contains(DONT_CARE)},
		  num_of_original_states_{nft.num_of_states()},
		  distances_(num_of_original_states_, unreachable),
		  original_moves_(num_of_original_states_),
		  original_expanded_(num_of_original_states_, false) {
		compute_distances();
	}

	/// Whether the lazy exploration reads the transducer exactly as the unwinding does; see above for the exceptions.
	bool is_exact() const { return exact_; }

	/// Does some link read the wildcard?
	bool has_wildcard_links() const { return has_wildcard_links_; }
	/// Is @p symbol the wildcard reading every replacement symbol?
	bool is_wildcard(const Symbol symbol) const { return has_wildcard_ && symbol == DONT_CARE; }
	/// Does the wildcard read the concrete letter @p symbol?
	bool wildcard_reads(const Symbol symbol) const { return has_wildcard_ && symbols_.contains(symbol); }
	const OrdVector<Symbol>& symbols() const { return symbols_; }

	bool is_cursor(const State state) const { return state >= num_of_original_states_; }
	size_t num_of_states() const { return num_of_original_states_ + cursors_.size(); }
	const mata::utils::SparseSet<State>& initial() const { return nft_.initial; }
	State original_or_none(const State state) const { return is_cursor(state) ? Limits::max_state : state; }

	/// Length of a shortest run from @p state to a final state, or @c unreachable.
	size_t distance(const State state) const { return distances_[state]; }

	/// The moves of a state: symbol posts sorted by symbol, the wildcard as DONT_CARE.
	using Moves = std::ranges::subrange<StatePost::const_iterator>;

	/// Computes the moves of @p state unless they are known. Interning the cursors they reach invalidates every
	///  range that moves() returned before.
	void expand(const State state) {
		if (is_cursor(state)) {
			const size_t cursor_idx{state - num_of_original_states_};
			if (cursor_expanded_[cursor_idx]) { return; }
			const Cursor cursor{cursors_[cursor_idx]};
			const State next{
				cursor.remaining == 1 ? cursor.target : intern(cursor.target, cursor.remaining - 1, cursor.label)
			};
			cursor_moves_[cursor_idx].push_back(next);
			cursor_expanded_[cursor_idx] = true;
			return;
		}
		if (original_expanded_[state]) { return; }
		std::vector<std::pair<Symbol, State>> moves{};
		for (const SymbolPost& symbol_post : nft_.delta[state]) {
			for (const State target : symbol_post.targets) {
				const Link link{link_of(state, symbol_post.symbol, target)};
				if (link.length == 0) { continue; }
				moves.emplace_back(link.first, link.length == 1 ? target : intern(target, link.length - 1, link.rest));
			}
		}
		std::ranges::sort(moves);
		StatePost& state_post{original_moves_[state]};
		for (auto move{moves.begin()}; move != moves.end(); ++move) {
			if (state_post.empty() || state_post.back().symbol != move->first) {
				state_post.push_back(SymbolPost{move->first});
			}
			if (move != moves.begin() && *(move - 1) == *move) { continue; }
			state_post.back().push_back(move->second);
		}
		original_expanded_[state] = true;
	}

	/// The moves of the expanded @p state.
	Moves moves(const State state) const {
		if (is_cursor(state)) {
			const auto post{cursor_moves_.cbegin() + static_cast<std::ptrdiff_t>(state - num_of_original_states_)};
			return Moves{post, post + 1};
		}
		return Moves{original_moves_[state].begin(), original_moves_[state].end()};
	}

	/// The moves over @p symbol in @p moves, or their end.
	static StatePost::const_iterator find(const Moves moves, const Symbol symbol) {
		const auto post{std::ranges::lower_bound(moves, symbol, {}, &SymbolPost::symbol)};
		return post != moves.end() && post->symbol == symbol ? post : moves.end();
	}

	/// Appends to @p run a shortest run from @p state to a final state, cursors on the path as @c Limits::max_state.
	void append_shortest_accepting_run(State state, Run& run) {
		run.path.push_back(original_or_none(state));
		for (size_t dist{distance(state)}; dist != 0 && dist != unreachable; --dist) {
			bool advanced{false};
			expand(state);
			for (const SymbolPost& symbol_post : moves(state)) {
				for (const State target : symbol_post.targets) {
					if (distance(target) + 1 != dist) { continue; }
					run.word.push_back(is_wildcard(symbol_post.symbol) ? symbols_.front() : symbol_post.symbol);
					run.path.push_back(original_or_none(target));
					state = target;
					advanced = true;
					break;
				}
				if (advanced) { break; }
			}
		}
	}

  private:
	struct Cursor {
		State target;
		size_t remaining;
		Symbol label;
	};
	struct CursorKey {
		State target;
		size_t remaining;
		Symbol label;
		bool operator==(const CursorKey&) const = default;
	};
	struct CursorKeyHash {
		size_t operator()(const CursorKey& key) const {
			size_t hash{std::hash<Symbol>{}(key.label)};
			hash ^= std::hash<size_t>{}(key.remaining) + 0x9e'37'79'b9'7f'4a'7c'15ULL + (hash << 6) + (hash >> 2);
			hash ^= std::hash<State>{}(key.target) + 0x9e'37'79'b9'7f'4a'7c'15ULL + (hash << 6) + (hash >> 2);
			return hash;
		}
	};
	/// The unwound form of one transition: @c length links, the first over @c first and the others over @c rest;
	///  @c length 0 when some link reads nothing.
	struct Link {
		size_t length;
		Symbol first;
		Symbol rest;
	};

	Link link_of(const State source, const Symbol symbol, const State target) const {
		// The same unsigned arithmetic as in Nft::unwind_jumps_inplace().
		const Level source_level{nft_.levels[source]};
		const Level target_level{nft_.levels[target]};
		const Level diff{
			target_level == 0 ? static_cast<Level>(nft_.levels.num_of_levels) - source_level
							  : target_level - source_level
		};
		Link link{.length = 1, .first = symbol, .rest = symbol};
		if (diff > 1) {
			link.length = target_level == 0 || target_level > source_level ? diff : 2;
			if (jump_mode_ == JumpMode::AppendDontCares) { link.rest = DONT_CARE; }
		}
		const bool reads_nothing{
			symbols_.empty() && has_wildcard_ &&
			(link.first == DONT_CARE || (link.length > 1 && link.rest == DONT_CARE))
		};
		if (reads_nothing) { link.length = 0; }
		return link;
	}

	State intern(const State target, const size_t remaining, const Symbol label) {
		const CursorKey key{.target = target, .remaining = remaining, .label = label};
		if (const auto it{cursor_ids_.find(key)}; it != cursor_ids_.end()) { return it->second; }
		const State id{mata::nfa::to_state(num_of_states())};
		cursor_ids_.emplace(key, id);
		cursors_.push_back(Cursor{.target = target, .remaining = remaining, .label = label});
		cursor_moves_.emplace_back(label);
		cursor_expanded_.push_back(false);
		// The distances of the original states are final by now: the constructor computes them.
		distances_.push_back(distances_[target] == unreachable ? unreachable : remaining + distances_[target]);
		return id;
	}

	/// Shortest weighted distances of the original states to a final state, a chain weighing its number of links.
	///  Also finds whether any link reads the wildcard, and the DONT_CARE transitions that stay on their level, which
	///  make the lazy reading inexact.
	void compute_distances() {
		std::vector<std::vector<std::pair<State, size_t>>> predecessors(num_of_original_states_);
		for (State source{0}; source < num_of_original_states_; ++source) {
			for (const SymbolPost& symbol_post : nft_.delta[source]) {
				for (const State target : symbol_post.targets) {
					if (has_wildcard_ && symbol_post.symbol == DONT_CARE &&
						nft_.levels[source] == nft_.levels[target] && nft_.levels[target] != 0) {
						exact_ = false;
					}
					const Link link{link_of(source, symbol_post.symbol, target)};
					if (link.length == 0) { continue; }
					predecessors[target].emplace_back(source, link.length);
					has_wildcard_links_ =
						has_wildcard_links_ || is_wildcard(link.first) || (link.length > 1 && is_wildcard(link.rest));
				}
			}
		}
		using Entry = std::pair<size_t, State>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue{};
		for (const State final_state : nft_.final) {
			distances_[final_state] = 0;
			queue.emplace(0, final_state);
		}
		while (!queue.empty()) {
			const auto [distance, state]{queue.top()};
			queue.pop();
			if (distance != distances_[state]) { continue; }
			for (const auto& [source, length] : predecessors[state]) {
				if (distance + length < distances_[source]) {
					distances_[source] = distance + length;
					queue.emplace(distance + length, source);
				}
			}
		}
	}

	const Nft& nft_;
	const OrdVector<Symbol>& symbols_;
	const JumpMode jump_mode_;
	/// DONT_CARE is the wildcard unless it is its own sole replacement, and then a concrete letter.
	const bool has_wildcard_;
	bool has_wildcard_links_{false};
	bool exact_;
	const size_t num_of_original_states_;
	/// Indexed by the original states and then by the cursors.
	std::vector<size_t> distances_;
	std::vector<Cursor> cursors_{};
	std::unordered_map<CursorKey, State, CursorKeyHash> cursor_ids_{};
	std::vector<StatePost> original_moves_;
	std::vector<bool> original_expanded_;
	/// The one move of each cursor; its target is set when the cursor is expanded.
	std::vector<SymbolPost> cursor_moves_{};
	std::vector<bool> cursor_expanded_{};
};

/// The antichain inclusion of @p smaller in @p bigger over their lazily unwound jumps; see @c LazyUnwinding.
bool is_included_lazily(LazyUnwinding& smaller, LazyUnwinding& bigger, Run* const cex) {
	constexpr size_t unreachable{LazyUnwinding::unreachable};
	if (cex != nullptr) {
		cex->word.clear();
		cex->path.clear();
	}

	std::vector<Node> nodes{};
	std::vector<Step> steps{}; // Indexed like @c nodes; empty unless @p cex is requested.
	std::vector<size_t> worklist{};
	std::vector<ProcessedAntichain> processed(smaller.num_of_states());
	auto processed_of = [&](const State state) -> ProcessedAntichain& {
		if (processed.size() <= state) { processed.resize(smaller.num_of_states()); }
		return processed[state];
	};

	auto drop_useless_bigger_states = [&](StateSet& set) {
		set.erase_if([&](const State state) { return bigger.distance(state) == unreachable; });
	};
	auto min_dst = [&](const StateSet& set) {
		size_t min{unreachable};
		for (const State state : set) { min = std::min(min, bigger.distance(state)); }
		return min;
	};

	StateSet bigger_initial{bigger.initial()};
	drop_useless_bigger_states(bigger_initial);
	const size_t bigger_initial_min_dst{min_dst(bigger_initial)};
	for (const State state : smaller.initial()) {
		const size_t smaller_dst{smaller.distance(state)};
		if (smaller_dst == unreachable) { continue; }
		if (smaller_dst < bigger_initial_min_dst) {
			if (cex != nullptr) { smaller.append_shortest_accepting_run(state, *cex); }
			return false;
		}
		const size_t node_idx{nodes.size()};
		nodes.push_back(
			Node{
				.smaller_state = state,
				.bigger_set = bigger_initial,
				.signature = mata::nfa::algorithms::macrostate_signature(bigger_initial),
				.dominated = false,
			}
		);
		if (cex != nullptr) { steps.push_back(Step{.parent = node_idx, .symbol = 0}); }
		worklist.push_back(node_idx);
		processed_of(state).insert(nodes, node_idx);
	}

	// Explores the moves over @p symbol of smaller_state to @p smaller_targets with the bigger successor
	//  @p bigger_succ; false when it finds a counterexample.
	const auto explore = [&](const size_t node_idx, const Symbol symbol, StateSet bigger_succ,
							 const auto& smaller_targets) {
		drop_useless_bigger_states(bigger_succ);
		const size_t bigger_succ_min_dst{min_dst(bigger_succ)};
		const uint64_t bigger_succ_signature{mata::nfa::algorithms::macrostate_signature(bigger_succ)};
		for (auto target{smaller_targets.begin()}; target != smaller_targets.end(); ++target) {
			const State smaller_succ{*target};
			const size_t smaller_succ_dst{smaller.distance(smaller_succ)};
			if (smaller_succ_dst == unreachable) { continue; }

			if (smaller_succ_dst < bigger_succ_min_dst) {
				if (cex != nullptr) {
					cex->word.push_back(symbol);
					cex->path.push_back(nodes[node_idx].smaller_state);
					mata::nfa::internal::trace_back(nodes, steps, node_idx, *cex);
					for (State& state : cex->path) { state = smaller.original_or_none(state); }
					smaller.append_shortest_accepting_run(smaller_succ, *cex);
				}
				return false;
			}

			ProcessedAntichain& antichain{processed_of(smaller_succ)};
			// 'bigger_succ' is moved from only for the last target, after which the loop ends.
			// NOLINTNEXTLINE(bugprone-use-after-move)
			if (antichain.subsumes(nodes, bigger_succ, bigger_succ_signature)) { continue; }
			antichain.prune(nodes, bigger_succ, bigger_succ_signature);
			const size_t succ_idx{nodes.size()};
			// The last target of the move is the last reader of bigger_succ.
			const bool last_target{std::next(target) == smaller_targets.end()};
			nodes.push_back(
				Node{
					.smaller_state = smaller_succ,
					// NOLINTNEXTLINE(clang-analyzer-cplusplus.Move): see 'bigger_succ' above.
					.bigger_set = last_target ? std::move(bigger_succ) : bigger_succ,
					.signature = bigger_succ_signature,
					.dominated = false,
				}
			);
			if (cex != nullptr) { steps.push_back(Step{.parent = node_idx, .symbol = symbol}); }
			antichain.insert(nodes, succ_idx);
			worklist.push_back(succ_idx);
		}
		return true;
	};

	SynchronizedExistentialSymbolPostIterator sync_iterator;
	std::vector<State> targets{};
	std::vector<Symbol> read_letters{};
	std::vector<LazyUnwinding::Moves> bigger_moves{};
	while (!worklist.empty()) {
		const size_t node_idx{worklist.back()};
		worklist.pop_back();
		if (nodes[node_idx].dominated) { continue; }
		const State smaller_state{nodes[node_idx].smaller_state};
		// Valid until a counterexample is completed from the smaller transducer, after which the search returns.
		smaller.expand(smaller_state);
		const LazyUnwinding::Moves smaller_moves{smaller.moves(smaller_state)};
		// Read the macrostate once: exploring a successor that dominates this node releases its macrostate.
		bigger_moves.clear();
		for (const State state : nodes[node_idx].bigger_set) { bigger.expand(state); }
		for (const State state : nodes[node_idx].bigger_set) { bigger_moves.push_back(bigger.moves(state)); }
		const auto reset_sync_iterator = [&] {
			sync_iterator.reset();
			for (const LazyUnwinding::Moves& moves : bigger_moves) {
				sync_iterator.push_back(moves.begin(), moves.end());
			}
		};

		// The targets of the wildcard moves of the macrostate.
		targets.clear();
		if (bigger.has_wildcard_links()) {
			for (const LazyUnwinding::Moves& moves : bigger_moves) {
				if (const auto post{LazyUnwinding::find(moves, DONT_CARE)}; post != moves.end()) {
					targets.insert(targets.end(), post->targets.begin(), post->targets.end());
				}
			}
		}
		const StateSet wildcard_succ{targets};
		// The successor over the concrete letter of @p smaller_move, which the iterator is synchronized with.
		const auto successor_over = [&](const Symbol symbol, const bool synchronized) {
			StateSet bigger_succ{synchronized ? sync_iterator.unify_targets() : StateSet{}};
			if (!wildcard_succ.empty() && bigger.wildcard_reads(symbol)) {
				bigger_succ = StateSet::set_union(bigger_succ, wildcard_succ);
			}
			return bigger_succ;
		};

		// A wildcard move of the smaller state reads every replacement symbol. When every one of them has a concrete
		//  move in the macrostate, the wildcard move is read as one move per symbol, merged with the concrete moves of
		//  the smaller state over the same symbol; otherwise only the symbol without a concrete bigger move is read,
		//  since its successor is contained in the others.
		const auto wildcard_move{
			smaller.has_wildcard_links() ? LazyUnwinding::find(smaller_moves, DONT_CARE) : smaller_moves.end()
		};
		bool wildcard_spelled_out{false};
		if (wildcard_move != smaller_moves.end()) {
			read_letters.clear();
			for (const LazyUnwinding::Moves& moves : bigger_moves) {
				for (const SymbolPost& symbol_post : moves) {
					if (!bigger.is_wildcard(symbol_post.symbol) && smaller.symbols().contains(symbol_post.symbol)) {
						read_letters.push_back(symbol_post.symbol);
					}
				}
			}
			std::ranges::sort(read_letters);
			read_letters.erase(std::ranges::unique(read_letters).begin(), read_letters.end());
			wildcard_spelled_out = read_letters.size() == smaller.symbols().size();
		}

		reset_sync_iterator();
		for (const SymbolPost& smaller_move : smaller_moves) {
			if (smaller.is_wildcard(smaller_move.symbol)) { continue; }
			// Already read together with the spelled-out wildcard move.
			if (wildcard_spelled_out && smaller.symbols().contains(smaller_move.symbol)) { continue; }
			const bool synchronized{sync_iterator.synchronize_with(smaller_move)};
			if (!explore(
					node_idx, smaller_move.symbol, successor_over(smaller_move.symbol, synchronized),
					smaller_move.targets
				)) {
				return false;
			}
		}

		// The wildcard move is read last: the worklist is a stack, so its successors, whose macrostates are the
		// smallest
		//  when only the wildcard targets are read, are expanded first and prune the others.
		if (wildcard_move != smaller_moves.end()) {
			if (wildcard_spelled_out) {
				reset_sync_iterator();
				for (const Symbol letter : read_letters) {
					const bool synchronized{sync_iterator.synchronize_with(letter)};
					const auto concrete_move{LazyUnwinding::find(smaller_moves, letter)};
					if (!explore(
							node_idx, letter, successor_over(letter, synchronized),
							concrete_move == smaller_moves.end()
								? StateSet{wildcard_move->targets}
								: StateSet::set_union(
									  StateSet{wildcard_move->targets}, StateSet{concrete_move->targets}
								  )
						)) {
						return false;
					}
				}
			} else {
				Symbol letter{smaller.symbols().front()};
				if (cex != nullptr) {
					auto read{read_letters.begin()};
					for (const Symbol symbol : smaller.symbols()) {
						if (read == read_letters.end() || *read != symbol) {
							letter = symbol;
							break;
						}
						++read;
					}
				}
				if (!explore(node_idx, letter, wildcard_succ, wildcard_move->targets)) { return false; }
			}
		}
	}
	return true;
}
} // namespace

/// naive language inclusion check (complementation + intersection + emptiness)
bool algorithms::is_included_naive(
	const Nft& smaller,
	const Nft& bigger,
	const Alphabet* const alphabet, // TODO: this should not be needed, likewise for equivalence
	Run* cex,
	const JumpMode jump_mode
) { // {{{
	internal::validate_nft_levels(smaller, "is_included_naive");
	internal::validate_nft_levels(bigger, "is_included_naive");
	Nft bigger_cmpl;
	if (alphabet == nullptr) {
		bigger_cmpl = complement(bigger, create_alphabet(smaller, bigger));
	} else {
		bigger_cmpl = complement(bigger, *alphabet);
	}
	const Nft nft_isect = intersection(smaller, bigger_cmpl, nullptr, jump_mode);

	return nft_isect.is_lang_empty(cex);
} // is_included_naive }}}

/// language inclusion check using Antichains
// TODO, what about to construct the separator from this?
bool mata::nft::algorithms::is_included_antichains(
	const Nft& smaller,
	const Nft& bigger,
	const Alphabet* const alphabet,
	Run* cex,
	const JumpMode jump_mode
) { // {{{
	internal::validate_nft_levels(smaller, "is_included_antichains");
	internal::validate_nft_levels(bigger, "is_included_antichains");
	if (smaller.levels.num_of_levels != bigger.levels.num_of_levels) { return false; }

	const OrdVector<Symbol> symbols{unwinding_symbols(smaller, bigger, alphabet)};
	if (spells_out_dont_care(smaller, symbols, jump_mode) || spells_out_dont_care(bigger, symbols, jump_mode)) {
		LazyUnwinding smaller_unwinding{smaller, symbols, jump_mode};
		LazyUnwinding bigger_unwinding{bigger, symbols, jump_mode};
		if (smaller_unwinding.is_exact() && bigger_unwinding.is_exact()) {
			return is_included_lazily(smaller_unwinding, bigger_unwinding, cex);
		}
	}
	// No DONT_CARE to spell out, or an unwinding the lazy reading does not follow exactly: compare the flat automata.
	return nfa::algorithms::is_included_antichains(
		smaller.to_nfa_update_copy(symbols, jump_mode), bigger.to_nfa_update_copy(symbols, jump_mode), alphabet, cex
	);
} // }}}

bool mata::nft::algorithms::is_included_antichains_unwound(
	const Nft& smaller, const Nft& bigger, const Alphabet* const alphabet, Run* cex, const JumpMode jump_mode
) {
	internal::validate_nft_levels(smaller, "is_included_antichains_unwound");
	internal::validate_nft_levels(bigger, "is_included_antichains_unwound");
	if (smaller.levels.num_of_levels != bigger.levels.num_of_levels) { return false; }

	const OrdVector<Symbol> symbols{unwinding_symbols(smaller, bigger, alphabet)};
	// NFTs are not NFAs: reading the unwound transducers as flat automata has to be spelled out explicitly.
	return nfa::algorithms::is_included_antichains(
		smaller.to_nfa_update_copy(symbols, jump_mode), bigger.to_nfa_update_copy(symbols, jump_mode), alphabet, cex
	);
}

namespace {
using AlgoType = decltype(algorithms::is_included_naive)*;

AlgoType set_algorithm(const std::string& function_name, const ParameterMap& params) {
	if (!haskey(params, "algorithm")) {
		throw std::runtime_error(
			function_name +
			" requires setting the \"algorithm\" key in the \"params\" argument; "
			"received: " +
			std::to_string(params)
		);
	}

	decltype(algorithms::is_included_naive)* algo;
	if (const std::string& str_algo = params.at("algorithm"); "naive" == str_algo) {
		algo = algorithms::is_included_naive;
	} else if ("antichains" == str_algo) {
		algo = algorithms::is_included_antichains;
	} else {
		throw std::runtime_error(
			std::to_string(__func__) + " received an unknown value of the \"algorithm\" key: " + str_algo
		);
	}

	return algo;
}
} // namespace

// The dispatching method that calls the correct one based on parameters
bool mata::nft::is_included(
	const Nft& smaller,
	const Nft& bigger,
	Run* cex,
	const Alphabet* const alphabet,
	const JumpMode jump_mode,
	const ParameterMap& params
) { // {{{
	// ReSharper disable once CppLocalVariableMayBeConst
	AlgoType algo{set_algorithm(std::to_string(__func__), params)};
	return algo(smaller, bigger, alphabet, cex, jump_mode);
}

bool mata::nft::are_equivalent(
	const Nft& lhs, const Nft& rhs, const Alphabet* alphabet, const JumpMode jump_mode, const ParameterMap& params
) {
	internal::validate_nft_levels(lhs, "are_equivalent");
	internal::validate_nft_levels(rhs, "are_equivalent");
	if (lhs.levels.num_of_levels != rhs.levels.num_of_levels) { return false; }

	const OrdVector<Symbol> symbols{unwinding_symbols(lhs, rhs, alphabet)};
	const auto algorithm{params.find("algorithm")};
	if (algorithm != params.end() && algorithm->second == "antichains" &&
		(spells_out_dont_care(lhs, symbols, jump_mode) || spells_out_dont_care(rhs, symbols, jump_mode))) {
		// Both inclusions share the cursors and the distances of each operand.
		LazyUnwinding lhs_unwinding{lhs, symbols, jump_mode};
		LazyUnwinding rhs_unwinding{rhs, symbols, jump_mode};
		if (lhs_unwinding.is_exact() && rhs_unwinding.is_exact()) {
			return is_included_lazily(lhs_unwinding, rhs_unwinding, nullptr) &&
				   is_included_lazily(rhs_unwinding, lhs_unwinding, nullptr);
		}
	}

	// NFTs are not NFAs: reading the unwound transducers as flat automata has to be spelled out explicitly. EPSILON
	// on a level means "nothing on this tape" and stays a letter here, so the epsilon-unaware entry point is used.
	return nfa::algorithms::are_equivalent_epsilon_as_symbol(
		lhs.to_nfa_update_copy(symbols, jump_mode), rhs.to_nfa_update_copy(symbols, jump_mode), alphabet, params
	);
}

bool mata::nft::are_equivalent(const Nft& lhs, const Nft& rhs, const JumpMode jump_mode, const ParameterMap& params) {
	return are_equivalent(lhs, rhs, nullptr, jump_mode, params);
}
