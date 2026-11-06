/** @file
 * @brief Determinization by the "Four Russians" subset construction (algorithms::determinize_four_russians()).
 *
 * This works by splitting the NFA into smaller blocks that are easier for the computer to memoize, which leads to a
 * speedup of the subset construction, especially for NFAs with many states and few symbols.
 */

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"

using namespace mata::nfa;
using mata::Symbol;
using mata::nfa::algorithms::FourRussiansOptions;
using mata::nfa::algorithms::FourRussiansReduction;

namespace {

using SubsetMap = std::unordered_map<StateSet, State>;
using MacrostateDiscover = std::optional<std::function<bool(const Nfa&, State, const StateSet&)>>;

// ===== Sets of states and small helpers.

constexpr unsigned WORD_BITS = 64;
/// Largest block size: one word.
constexpr unsigned MAX_BLOCK_SIZE = WORD_BITS;

/// No id. Ids are 32-bit to keep the tables small (8 bytes per hash slot); 2^32 sets would take at least 160 GiB.
constexpr uint32_t NO_ID = std::numeric_limits<uint32_t>::max();
constexpr State NO_STATE = Limits::max_state;
constexpr uint64_t GOLDEN = 0x9e'37'79'b9'7f'4a'7c'15ULL; ///< Multiplier for hashing (2^64 / golden ratio).

constexpr size_t words_for(const size_t num_of_states) { return (num_of_states + WORD_BITS - 1) / WORD_BITS; }

/// One non-zero word of a set of states: bit i stands for the state 64 * @c word + i.
struct Chunk {
	uint64_t bits;
	uint64_t word;

	bool operator==(const Chunk&) const = default;
};

/// A set of states: its chunks sorted by word, without zero chunks.
using ChunkSpan = std::span<const Chunk>;

constexpr Chunk chunk_of(const State q) { return Chunk{uint64_t{1} << (q % WORD_BITS), q / WORD_BITS}; }

constexpr State state_at(const uint64_t word, const unsigned bit) { return static_cast<State>(word * WORD_BITS + bit); }

/// Positions of the set bits of a word, lowest first; this also functions as an iterator.
struct SetBits {
	uint64_t rest;

	SetBits begin() const { return *this; }

	std::default_sentinel_t end() const { return {}; }

	unsigned operator*() const { return static_cast<unsigned>(std::countr_zero(rest)); }

	SetBits& operator++() {
		rest &= rest - 1; // Clears the lowest set bit.
		return *this;
	}

	bool operator==(std::default_sentinel_t) const { return rest == 0; }
};

/// Add @p q to the set at the end of @p chunks (starting at @p start); states come in increasing order.
inline void push_state(std::vector<Chunk>& chunks, const size_t start, const State q) {
	const Chunk chunk = chunk_of(q);
	if (chunks.size() > start && chunks.back().word == chunk.word) {
		chunks.back().bits |= chunk.bits;
	} else {
		chunks.push_back(chunk);
	}
}

inline bool intersects(const ChunkSpan states, const std::vector<uint64_t>& bits) {
	return std::ranges::any_of(states, [&](const Chunk& chunk) { return (bits[chunk.word] & chunk.bits) != 0; });
}

constexpr auto unite_chunks = [](const Chunk& lhs, const Chunk& rhs) { return Chunk{lhs.bits | rhs.bits, lhs.word}; };

/// Convert @p index to a 32-bit id; throws on overflow.
inline uint32_t to_id(const size_t index) {
	if (index >= size_t{NO_ID}) {
		throw std::length_error("determinize_four_russians: too many sets of states or memoized entries");
	}
	return static_cast<uint32_t>(index);
}

inline unsigned checked_block_size(const unsigned block_size) {
	if (block_size > MAX_BLOCK_SIZE || (block_size != 0 && !std::has_single_bit(block_size))) {
		throw std::invalid_argument(
			"determinize_four_russians: block size must be 0 (automatic) or a power of two between 1 and 64, got " +
			std::to_string(block_size)
		);
	}
	return block_size;
}

/// Cost of sorting @p num_of_items items (n log n), to compare with a linear scan.
inline size_t sort_cost(const size_t num_of_items) {
	return num_of_items * static_cast<size_t>(std::bit_width(num_of_items));
}

/// A simple implementation of merge sort.
template <typename T, typename Key, typename Combine>
void merge_sorted(
	const std::span<const T> lhs,
	const std::span<const T> rhs,
	std::vector<T>& out,
	const Key key,
	const Combine combine
) {
	auto lhs_it = lhs.begin();
	auto rhs_it = rhs.begin();

	// Take the smaller key first until one side runs out, then the rest of the other.
	while (lhs_it != lhs.end() && rhs_it != rhs.end()) {
		if (std::invoke(key, *lhs_it) < std::invoke(key, *rhs_it)) {
			out.push_back(*lhs_it++);
		} else if (std::invoke(key, *rhs_it) < std::invoke(key, *lhs_it)) {
			out.push_back(*rhs_it++);
		} else {
			out.push_back(combine(*lhs_it++, *rhs_it++));
		}
	}

	for (; lhs_it != lhs.end(); ++lhs_it) { out.push_back(*lhs_it); }
	for (; rhs_it != rhs.end(); ++rhs_it) { out.push_back(*rhs_it); }
}

// Faster than `emplace_back` in place: inlined into the output loops, it makes SymbolPost call the out-of-line
// SmallOrdVector constructor for every transition.
inline void append_transition(StatePost& state_post, const Symbol symbol, const State target) {
	state_post.emplace_back(symbol, target);
}

// ===== Symbol classes.

using SymbolIndex = uint32_t; ///< Index of a symbol in SymbolClasses::symbols().
using ClassId = uint32_t; ///< A symbol class, numbered by its smallest symbol.

/// Sorted symbols of @p delta
std::vector<Symbol> used_symbols(const Delta& delta) {
	size_t num_of_symbol_posts = 0;
	Symbol max_symbol = 0;
	// Count the symbol posts and find the largest symbol.
	for (const StatePost& state_post : delta) {
		if (state_post.empty()) { continue; }
		num_of_symbol_posts += state_post.size();
		max_symbol = std::max(max_symbol, std::prev(state_post.end())->symbol); // Symbol posts are sorted.
	}

	std::vector<Symbol> symbols;
	// Not words_for(max_symbol + 1), which overflows for the largest symbols when size_t has 32 bits.
	const size_t num_of_words = size_t{max_symbol / WORD_BITS} + 1;
	// collected in a bit vector when that is cheaper than sorting
	if (num_of_words <= num_of_symbol_posts) {
		// Mark the used symbols in a bit vector, then read them out in order.
		std::vector<uint64_t> used(num_of_words, 0);
		for (const StatePost& state_post : delta) {
			for (const SymbolPost& symbol_post : state_post) {
				used[symbol_post.symbol / WORD_BITS] |= uint64_t{1} << (symbol_post.symbol % WORD_BITS);
			}
		}
		for (size_t word = 0; word < num_of_words; ++word) {
			for (const unsigned bit : SetBits{used[word]}) {
				symbols.push_back(static_cast<Symbol>(word * WORD_BITS + bit));
			}
		}
	} else {
		// Collect all symbols, then sort them and remove the duplicates.
		symbols.reserve(num_of_symbol_posts);
		for (const StatePost& state_post : delta) {
			for (const SymbolPost& symbol_post : state_post) { symbols.push_back(symbol_post.symbol); }
		}
		std::sort(symbols.begin(), symbols.end());
		symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
	}

	return symbols;
}

using SymbolIt = std::vector<Symbol>::const_iterator;

/// First position in [@p from, @p end) not less than @p symbol: doubling steps from @p from, then binary search.
inline SymbolIt lower_bound_from(const SymbolIt from, const SymbolIt end, const Symbol symbol) {
	const auto size = end - from;
	if (size == 0 || !(*from < symbol)) { return from; }
	// Invariant: from[bound / 2] < symbol.
	std::ptrdiff_t bound = 1;
	while (bound < size && from[bound] < symbol) { bound *= 2; }
	return std::lower_bound(from + bound / 2 + 1, from + std::min(bound + 1, size), symbol);
}

/// Call @p f(index, targets) for every symbol post of @p state_post with targets; index is the position of the symbol
///  in @p symbols.
template <typename F>
void for_each_symbol_post(const StatePost& state_post, const std::vector<Symbol>& symbols, const F f) {
	auto symbol_it = symbols.cbegin();
	for (const SymbolPost& symbol_post : state_post) {
		if (symbol_post.targets.empty()) { continue; }
		// The symbols increase, so search on from the previous one.
		symbol_it = lower_bound_from(symbol_it, symbols.cend(), symbol_post.symbol);
		f(static_cast<SymbolIndex>(symbol_it - symbols.cbegin()), symbol_post.targets.span());
		++symbol_it;
	}
}

/// Partition of the symbols into classes, which can only be split.
class Partition {
  public:
	/// All symbols in one class.
	explicit Partition(const size_t num_of_symbols) : class_of_(num_of_symbols, 0), class_size_(1, num_of_symbols) {}

	ClassId class_of(const SymbolIndex symbol) const { return class_of_[symbol]; }

	size_t size_of(const ClassId class_id) const { return class_size_[class_id]; }

	bool all_singletons() const { return num_of_classes_ == class_of_.size(); }

	ClassId new_class() {
		class_size_.push_back(0);
		return to_id(class_size_.size() - 1);
	}

	void move(const SymbolIndex symbol, const ClassId to) {
		if (--class_size_[class_of_[symbol]] == 0) { --num_of_classes_; }
		if (class_size_[to]++ == 0) { ++num_of_classes_; }
		class_of_[symbol] = to;
	}

	std::vector<ClassId> into_classes() && { return std::move(class_of_); }

  private:
	std::vector<ClassId> class_of_;
	std::vector<size_t> class_size_;
	size_t num_of_classes_ = 1; ///< The number of non-empty classes.
};

/// A symbol with targets in the state being processed.
struct Occurrence {
	ClassId class_id;
	size_t hash; ///< Hash of the targets, computed only when needed.
	SymbolIndex symbol;
	std::span<const State> targets;
};

/// Partition refinement.
/// Symbols a and b share a class iff delta(q, a) = delta(q, b) for every state q.
std::vector<ClassId> refine_classes(const Delta& delta, const std::vector<Symbol>& symbols) {
	Partition partition(symbols.size());
	std::vector<Occurrence> occurrences;
	// Split the classes by the targets in each state in turn.
	for (const StatePost& state_post : delta) {
		// Classes can only split, so stop once all are single symbols.
		if (partition.all_singletons()) { break; }

		occurrences.clear();
		for_each_symbol_post(state_post, symbols, [&](const SymbolIndex symbol, const std::span<const State> targets) {
			const ClassId class_id = partition.class_of(symbol);
			if (partition.size_of(class_id) > 1) { occurrences.push_back(Occurrence{class_id, 0, symbol, targets}); }
		});
		std::ranges::sort(occurrences, {}, &Occurrence::class_id);
		const auto same_class = [](const Occurrence& lhs, const Occurrence& rhs) {
			return lhs.class_id == rhs.class_id;
		};
		// Split the class of each run of symbols by their targets in this state.
		for (const std::span<Occurrence> run : occurrences | std::views::chunk_by(same_class)) {
			if (run.size() == 1) {
				// Separate it from the rest of its class.
				partition.move(run[0].symbol, partition.new_class());
				continue;
			}

			// Nothing to split: the whole class has the same targets (the common case, so checked before hashing).
			if (run.size() == partition.size_of(run[0].class_id) &&
				std::ranges::all_of(run.subspan(1), [&](const Occurrence& occurrence) {
					return std::ranges::equal(occurrence.targets, run[0].targets);
				})) {
				continue;
			}

			for (Occurrence& occurrence : run) {
				occurrence.hash = mata::utils::hash_range(occurrence.targets.begin(), occurrence.targets.end());
			}
			// Symbols with the same targets become adjacent.
			std::ranges::sort(run, {}, &Occurrence::hash);

			// Each symbol joins an earlier symbol with the same targets, or starts a new class.
			for (size_t index = 0; index < run.size(); ++index) {
				ClassId to = NO_ID;
				for (size_t earlier = index; earlier-- > 0 && run[earlier].hash == run[index].hash;) {
					if (std::ranges::equal(run[earlier].targets, run[index].targets)) {
						to = partition.class_of(run[earlier].symbol);
						break;
					}
				}
				partition.move(run[index].symbol, to == NO_ID ? partition.new_class() : to);
			}
		}
	}
	return std::move(partition).into_classes();
}

/// Symbols of a transition relation, grouped into classes of symbols with the same transitions in every state.
/// Like mintermization, but for explicit alphabets.
/// Classes are numbered by their smallest symbol.
class SymbolClasses {
  public:
	/// Classes of the symbols of @p delta; one class per symbol if @p compress is false.
	SymbolClasses(const Delta& delta, const bool compress) : symbols_(used_symbols(delta)) {
		std::vector<ClassId> classes(symbols_.size());
		if (compress && symbols_.size() > 1) {
			classes = refine_classes(delta, symbols_);
		} else {
			std::iota(classes.begin(), classes.end(), ClassId{0});
		}

		// Renumber the classes by their smallest symbol.
		const size_t num_of_ids = classes.empty() ? 0 : size_t{std::ranges::max(classes)} + 1;
		std::vector<ClassId> renumbered(num_of_ids, NO_ID);
		class_of_.resize(symbols_.size());
		for (SymbolIndex index = 0; index < symbols_.size(); ++index) {
			ClassId& class_id = renumbered[classes[index]];
			if (class_id == NO_ID) {
				class_id = static_cast<ClassId>(members_.size());
				members_.emplace_back();
			}
			class_of_[index] = class_id;
			members_[class_id].push_back(index);
		}
		all_singletons_ = members_.size() == symbols_.size();
	}

	const std::vector<Symbol>& symbols() const { return symbols_; }

	size_t num_of_symbols() const { return symbols_.size(); }

	size_t num_of_classes() const { return members_.size(); }

	bool all_singletons() const { return all_singletons_; }

	Symbol symbol(const SymbolIndex index) const { return symbols_[index]; }

	ClassId class_of(const SymbolIndex index) const { return class_of_[index]; }

	/// Symbols of the class @p class_id, in increasing order.
	const std::vector<SymbolIndex>& members(const ClassId class_id) const { return members_[class_id]; }

  private:
	std::vector<Symbol> symbols_;
	std::vector<ClassId> class_of_ = {};
	std::vector<std::vector<SymbolIndex>> members_ = {};
	bool all_singletons_ = true;
};

/// Numbering of the NFA states inside a construction.
/// A state gets the next number when it is first met, so states that occur together get close numbers.
/// This improves the locality of reference in the subset construction and puts a set of states in fewer words.
/// The renumbering by frequency (permute()) pays off most in long constructions.
class StateNumbering {
  public:
	explicit StateNumbering(const size_t num_of_states) : inner_of_(num_of_states, UNNUMBERED) {}

	size_t num_of_states() const { return inner_of_.size(); }

	size_t num_of_numbered() const { return outer_of_.size(); }

	/// Number of the state @p q, given on first use.
	State number(const State q) {
		if (inner_of_[q] == UNNUMBERED) {
			inner_of_[q] = static_cast<State>(outer_of_.size());
			outer_of_.push_back(q);
		}
		return inner_of_[q];
	}

	State outer(const State inner) const { return outer_of_[inner]; }

	/// Renumber: the state numbered @p order[i] gets the number i.
	void permute(const std::vector<State>& order) {
		std::vector<State> outer_of(order.size());
		for (State inner = 0; inner < order.size(); ++inner) {
			outer_of[inner] = outer_of_[order[inner]];
			inner_of_[outer_of[inner]] = inner;
		}
		outer_of_ = std::move(outer_of);
	}

	/// The set @p states (of numbers) as a set of states.
	StateSet to_outer(const ChunkSpan states) const {
		std::vector<State> outer_states;
		for (const Chunk& chunk : states) {
			for (const unsigned bit : SetBits{chunk.bits}) {
				outer_states.push_back(outer_of_[state_at(chunk.word, bit)]);
			}
		}
		std::sort(outer_states.begin(), outer_states.end());
		return StateSet(outer_states);
	}

  private:
	static constexpr State UNNUMBERED = std::numeric_limits<State>::max();

	std::vector<State> inner_of_; ///< Per outer state: its inner number, or UNNUMBERED if it has none yet.
	std::vector<State> outer_of_ = {}; ///< Per inner number: its outer state.
};

// ===== The table of posts of block patterns.

constexpr unsigned MAX_AUTO_BLOCK_SIZE = 32;

/// Block size after @p num_of_sets sets by using the largest power of two b <= MAX_AUTO_BLOCK_SIZE
/// with b * 2^b <= num_of_sets.
constexpr unsigned auto_block_size(const uint64_t num_of_sets) {
	unsigned block_size = 1;
	while (block_size < MAX_AUTO_BLOCK_SIZE && (uint64_t{2 * block_size} << (2 * block_size)) <= num_of_sets) {
		block_size *= 2;
	}
	return block_size;
}

/// Call @p f on every block pattern of @p chunk.
template <typename F> inline void for_each_block_pattern(const Chunk chunk, const unsigned block_size, const F f) {
	const uint64_t block_mask = block_size == WORD_BITS ? ~uint64_t{0} : (uint64_t{1} << block_size) - 1;
	for (uint64_t bits = chunk.bits; bits != 0;) {
		// The block starts at the lowest remaining state, rounded down to the block size.
		const unsigned block_start = static_cast<unsigned>(std::countr_zero(bits)) & ~(block_size - 1);
		const uint64_t pattern_bits = bits & (block_mask << block_start);
		bits ^= pattern_bits;
		f(Chunk{pattern_bits, chunk.word});
	}
}

using EntryId = uint32_t;

/// Targets under one class: a slice of the chunk pool.
struct TargetList {
	ClassId class_id;
	uint32_t num_of_chunks;
	size_t first_chunk;
};

/// Post of a set of states under all classes: target lists sorted by class (a slice of the list pool).
struct Entry {
	size_t first_list;
	size_t num_of_lists;
};

/// Limits of the dense union, which ORs all classes at once into a (class x word) matrix.
constexpr size_t MAX_DENSE_WORDS = 16;
constexpr size_t MAX_DENSE_CELLS = 1 << 14;

constexpr bool fits_dense_union(const size_t num_of_classes, const size_t num_of_words) {
	return num_of_words <= MAX_DENSE_WORDS && num_of_classes * num_of_words <= MAX_DENSE_CELLS;
}

/// Non-zero cell of the dense matrix that an entry sets (@c index = class * words + word).
struct Cell {
	uint64_t bits;
	uint32_t index;
};

/// Cells of an entry: a slice of the cell pool.
struct CellRun {
	size_t first_cell;
	size_t num_of_cells;
};

/// first_cell of an entry without cells.
constexpr size_t NOT_FLAT = std::numeric_limits<size_t>::max();

/// Hash map from block patterns to entry ids.
class PatternMemo {
  public:
	/// Entry id of @p pattern, or NO_ID if there is none.
	uint32_t find(const Chunk pattern) const {
		if (slots_.empty()) { return NO_ID; }
		for (size_t i = home_slot(pattern);; i = (i + 1) & slot_mask_) {
			const Slot& slot = slots_[i];
			if (slot.value == NO_ID) { return NO_ID; }
			if (slot.pattern == pattern) { return slot.value; }
		}
	}

	/// Add @p pattern, which is not in the map yet.
	void insert(const Chunk pattern, const uint32_t value) {
		if (slots_.empty()) {
			resize(INITIAL_SLOTS); // Allocated on first use.
		} else if (2 * (size_ + 1) > slots_.size()) {
			resize(2 * slots_.size());
		}
		size_t i = home_slot(pattern);
		while (slots_[i].value != NO_ID) { i = (i + 1) & slot_mask_; }
		slots_[i] = Slot{pattern, value};
		++size_;
	}

  private:
	struct Slot {
		Chunk pattern;
		uint32_t value; ///< NO_ID if the slot is empty.
	};

	static constexpr size_t INITIAL_SLOTS = 64;
	static constexpr Slot EMPTY_SLOT = {Chunk{0, 0}, NO_ID};

	std::vector<Slot> slots_ = {}; ///< A power of two.
	size_t slot_mask_ = 0; ///< The number of slots - 1.
	unsigned slot_shift_ = 0; ///< 64 - log2(the number of slots).
	size_t size_ = 0;

	size_t home_slot(const Chunk pattern) const {
		return static_cast<size_t>((((pattern.word * GOLDEN) ^ pattern.bits) * GOLDEN) >> slot_shift_);
	}

	void resize(const size_t num_of_slots) {
		const std::vector<Slot> old_slots = std::move(slots_);
		slots_.assign(num_of_slots, EMPTY_SLOT);
		slot_mask_ = num_of_slots - 1;
		slot_shift_ = static_cast<unsigned>(std::countl_zero(uint64_t{slot_mask_})); // The hash has 64 bits.
		size_ = 0;
		for (const Slot& slot : old_slots) {
			if (slot.value != NO_ID) { insert(slot.pattern, slot.value); }
		}
	}
};

/// Entries (posts under all classes) of single states and of block patterns.
/// The entry of a pattern is built from the entries of its two halves when the pattern first occurs.
/// Entries never change, so they share target lists.
/// For the dense union, entries are also kept flat for better locality.
class EntryTable {
  public:
	/// Entries of the states of @p delta (or of their numbers in @p numbering).
	EntryTable(
		const Delta& delta, const SymbolClasses& classes, const size_t num_of_states, StateNumbering* const numbering
	)
		: delta_(delta),
		  classes_(classes),
		  num_of_words_(words_for(num_of_states)),
		  flatten_(fits_dense_union(classes.num_of_classes(), num_of_words_)),
		  numbering_(numbering),
		  state_entry_(num_of_states, NO_ID),
		  class_stamp_(classes.num_of_classes(), 0) {}

	EntryTable(const EntryTable&) = delete;
	EntryTable& operator=(const EntryTable&) = delete;

	std::span<const TargetList> lists(const EntryId id) const {
		const Entry entry = entries_[id];
		return {list_pool_.data() + entry.first_list, entry.num_of_lists};
	}

	ChunkSpan chunks(const TargetList& list) const {
		return {chunk_pool_.data() + list.first_chunk, list.num_of_chunks};
	}

	/// Whether entries are kept flat (for the dense union).
	bool flattens() const { return flatten_; }

	/// Cells of the entry @p id; first_cell is NOT_FLAT if it has none.
	CellRun cell_run(const EntryId id) const { return cell_runs_[id]; }

	std::span<const Cell> cells(const CellRun run) const {
		return {cell_pool_.data() + run.first_cell, run.num_of_cells};
	}

	/// Entry of @p pattern, built when the pattern first occurs.
	EntryId entry_of(const Chunk pattern) {
		if (std::has_single_bit(pattern.bits)) {
			const State q = state_at(pattern.word, static_cast<unsigned>(std::countr_zero(pattern.bits)));
			return state_entry_[q] != NO_ID ? state_entry_[q] : build_state_entry(q);
		}
		// (Not std::optional: GCC passes that through memory here.)
		const uint32_t found = memo_.find(pattern);
		return found != NO_ID ? found : build_pattern_entry(pattern);
	}

  private:
	/// Memory limit of the cells: at most this many cells per list and chunk, plus MIN_CELL_POOL.
	static constexpr size_t MAX_CELLS_PER_POOL_ITEM = 2;
	static constexpr size_t MIN_CELL_POOL = 1 << 12;

	const Delta& delta_;
	const SymbolClasses& classes_;
	const size_t num_of_words_; ///< Of a set of states.
	const bool flatten_;
	StateNumbering* const numbering_; ///< Or nullptr: the states of delta_.
	std::vector<State> numbered_targets_ = {}; ///< Scratch for build_state_entry().
	std::vector<Chunk> chunk_pool_ = {};
	std::vector<TargetList> list_pool_ = {};
	std::vector<Entry> entries_ = {};
	std::vector<Cell> cell_pool_ = {}; ///< Cells of the flat entries.
	std::vector<CellRun> cell_runs_ = {}; ///< Per entry: its cells.
	std::vector<EntryId> state_entry_; ///< Per state: its entry, or NO_ID.
	PatternMemo memo_ = {};
	std::vector<State> class_stamp_; ///< Per class: 1 + the last state that added its list.

	EntryId build_state_entry(const State q) {
		const size_t first_list = list_pool_.size();
		const StatePost& post = delta_[numbering_ == nullptr ? q : numbering_->outer(q)];
		for_each_symbol_post(
			post, classes_.symbols(), [&](const SymbolIndex symbol, const std::span<const State> targets) {
				// The symbols of a class have the same targets: add one list per class.
				const ClassId class_id = classes_.class_of(symbol);
				if (class_stamp_[class_id] == q + 1) { return; }
				class_stamp_[class_id] = q + 1;

				const size_t first_chunk = chunk_pool_.size();
				if (numbering_ == nullptr) {
					for (const State target : targets) { push_state(chunk_pool_, first_chunk, target); }
				} else if (targets.size() == 1) {
					push_state(chunk_pool_, first_chunk, numbering_->number(targets.front()));
				} else {
					numbered_targets_.clear();
					for (const State target : targets) { numbered_targets_.push_back(numbering_->number(target)); }
					if (!std::is_sorted(numbered_targets_.begin(), numbered_targets_.end())) {
						std::sort(numbered_targets_.begin(), numbered_targets_.end());
					}
					for (const State target : numbered_targets_) { push_state(chunk_pool_, first_chunk, target); }
				}
				list_pool_.push_back(
					TargetList{class_id, static_cast<uint32_t>(chunk_pool_.size() - first_chunk), first_chunk}
				);
			}
		);
		// Sort the lists by class (they come in symbol order).
		const auto new_lists = list_pool_.begin() + static_cast<std::ptrdiff_t>(first_list);
		if (!std::ranges::is_sorted(new_lists, list_pool_.end(), {}, &TargetList::class_id)) {
			std::ranges::sort(new_lists, list_pool_.end(), {}, &TargetList::class_id);
		}
		state_entry_[q] = add_entry(first_list);
		return state_entry_[q];
	}

	/// Add the entry made of the lists from @p first_list on, with its cells if entries are flat (unless over the
	///  memory limit).
	EntryId add_entry(const size_t first_list) {
		entries_.push_back(Entry{first_list, list_pool_.size() - first_list});
		const EntryId id = to_id(entries_.size() - 1);
		if (!flatten_) { return id; }

		size_t num_of_cells = 0;
		for (const TargetList& list : lists(id)) { num_of_cells += list.num_of_chunks; }
		const size_t max_cells = MAX_CELLS_PER_POOL_ITEM * (list_pool_.size() + chunk_pool_.size()) + MIN_CELL_POOL;
		if (cell_pool_.size() + num_of_cells > max_cells) {
			cell_runs_.push_back(CellRun{NOT_FLAT, 0});
			return id;
		}
		const size_t first_cell = cell_pool_.size();
		// One cell per chunk, in the row of its class.
		for (const TargetList& list : lists(id)) {
			const size_t row = size_t{list.class_id} * num_of_words_;
			for (const Chunk& chunk : chunks(list)) {
				cell_pool_.push_back(Cell{chunk.bits, static_cast<uint32_t>(row + chunk.word)});
			}
		}
		cell_runs_.push_back(CellRun{first_cell, num_of_cells});
		return id;
	}

	/// Build the entry of @p pattern (at least two states) from the entries of its halves, and keep it.
	EntryId build_pattern_entry(const Chunk pattern) {
		// Split in the middle of the smallest aligned block that holds the pattern: the highest bit in which its lowest
		//  and highest state differ gives the middle.
		const unsigned lowest = static_cast<unsigned>(std::countr_zero(pattern.bits));
		const unsigned highest = WORD_BITS - 1 - static_cast<unsigned>(std::countl_zero(pattern.bits));
		const unsigned half_log = static_cast<unsigned>(std::bit_width(lowest ^ highest)) - 1;
		const unsigned middle = (highest >> half_log) << half_log;
		const uint64_t low_bits = pattern.bits & ((uint64_t{1} << middle) - 1);
		const EntryId low = entry_of(Chunk{low_bits, pattern.word});
		const EntryId high = entry_of(Chunk{pattern.bits ^ low_bits, pattern.word});
		// The merges read the pool they append to: make room first (growing geometrically), so their spans stay valid.
		const auto reserve_extra = [](auto& pool, const size_t extra) {
			if (pool.size() + extra > pool.capacity()) {
				pool.reserve(std::max(pool.capacity() * 2, pool.size() + extra));
			}
		};
		reserve_extra(list_pool_, entries_[low].num_of_lists + entries_[high].num_of_lists);
		const size_t first_list = list_pool_.size();
		// A class in only one of the halves keeps (shares) its target list.
		merge_sorted(
			lists(low), lists(high), list_pool_, &TargetList::class_id,
			[this, reserve_extra](const TargetList lhs, const TargetList rhs) {
				reserve_extra(chunk_pool_, size_t{lhs.num_of_chunks} + rhs.num_of_chunks);
				const size_t first_chunk = chunk_pool_.size();
				merge_sorted(chunks(lhs), chunks(rhs), chunk_pool_, &Chunk::word, unite_chunks);
				return TargetList{lhs.class_id, static_cast<uint32_t>(chunk_pool_.size() - first_chunk), first_chunk};
			}
		);
		const EntryId merged = add_entry(first_list);
		memo_.insert(pattern, merged);
		return merged;
	}
};

/// Unites entries class by class in a dense (class x word) matrix for small automata, in per-class buckets otherwise.
class SuccessorUnion {
  public:
	SuccessorUnion(const EntryTable& entries, const size_t num_of_classes, const size_t num_of_words)
		: entries_(entries),
		  num_of_classes_(num_of_classes),
		  num_of_words_(num_of_words),
		  bucket_head_(num_of_classes, NO_ID) {
		if (fits_dense_union(num_of_classes, num_of_words)) {
			dense_accumulator_.assign(num_of_classes * num_of_words, 0);
			all_classes_.resize(num_of_classes);
			std::iota(all_classes_.begin(), all_classes_.end(), ClassId{0});
		} else {
			word_accumulator_.assign(num_of_words, 0);
		}
	}

	/// Call @p emit(class_id, targets) for every post of the union of the entries @p entry_ids, by class, until it
	///  returns false. Returns false if stopped. The targets are valid until the next call.
	template <typename Emit> bool for_each_union(const std::span<const EntryId> entry_ids, Emit&& emit) {
		if (entry_ids.size() == 1) {
			// A single entry is the union.
			for (const TargetList& list : entries_.lists(entry_ids[0])) {
				if (!emit(list.class_id, entries_.chunks(list))) { return false; }
			}
			return true;
		}
		if (dense_accumulator_.empty()) { return unite_in_buckets(entry_ids, emit); }
		if (entries_.flattens()) { return unite_cells(entry_ids, emit); }

		// OR all target lists into the (class x word) matrix. Mark the classes with targets, unless there are more
		// lists
		//  than cells (then read all rows).
		size_t num_of_lists = 0;
		for (const EntryId id : entry_ids) { num_of_lists += entries_.lists(id).size(); }
		const bool mark_classes = dense_accumulator_.size() > num_of_lists;
		unite_lists_densely(entry_ids, mark_classes);
		return read_out_rows(mark_classes ? std::span<const ClassId>{touched_classes_} : all_classes_, emit);
	}

  private:
	const EntryTable& entries_;
	const size_t num_of_classes_;
	const size_t num_of_words_;
	/// Per class: the first link of its bucket, or NO_ID. A bucket is a linked list: link i holds link_lists_[i] and
	///  continues at link_next_[i].
	std::vector<uint32_t> bucket_head_;
	std::vector<const TargetList*> link_lists_ = {};
	std::vector<uint32_t> link_next_ = {};
	std::vector<ClassId> touched_classes_ = {}; ///< Classes with target lists now.
	std::vector<uint64_t> dense_accumulator_ = {}; ///< (class x word) bits, or empty if too large.
	std::vector<ClassId> all_classes_ = {}; ///< All classes, if dense_accumulator_ is used.
	std::vector<uint64_t> word_accumulator_ = {}; ///< Per-word bits for buckets of three or more lists.
	std::vector<uint64_t> touched_words_ = {}; ///< Non-zero words of word_accumulator_.
	/// Unions of the current posts (reserved up front, so they do not move).
	std::vector<Chunk> union_chunks_ = {};

	uint64_t* dense_row(const ClassId class_id) { return &dense_accumulator_[class_id * num_of_words_]; }

	/// OR the target lists of @p entry_ids into the matrix, collecting the touched classes if @p mark_classes.
	void unite_lists_densely(const std::span<const EntryId> entry_ids, const bool mark_classes) {
		touched_classes_.clear();
		for (const EntryId id : entry_ids) {
			for (const TargetList& list : entries_.lists(id)) {
				if (mark_classes && bucket_head_[list.class_id] == NO_ID) {
					bucket_head_[list.class_id] = 0; // Only marks the class as touched.
					touched_classes_.push_back(list.class_id);
				}
				uint64_t* const row = dense_row(list.class_id);
				for (const Chunk& chunk : entries_.chunks(list)) { row[chunk.word] |= chunk.bits; }
			}
		}
		if (mark_classes) { sort_touched_classes(); }
	}

	/// The dense union of flat entries: OR their cells in one loop, or go list by list if they set few cells.
	template <typename Emit> bool unite_cells(const std::span<const EntryId> entry_ids, Emit& emit) {
		// An entry without cells counts its lists. (Not branch-free: that was slower.)
		size_t num_of_cells = 0;
		for (const EntryId id : entry_ids) {
			const CellRun run = entries_.cell_run(id);
			num_of_cells += run.first_cell != NOT_FLAT ? run.num_of_cells : entries_.lists(id).size();
		}
		if (2 * num_of_cells < dense_accumulator_.size()) {
			unite_lists_densely(entry_ids, true);
			return read_out_rows(touched_classes_, emit);
		}

		uint64_t* const matrix = dense_accumulator_.data();
		// OR the cells of each entry, or its lists if it has no cells.
		for (const EntryId id : entry_ids) {
			const CellRun run = entries_.cell_run(id);
			if (run.first_cell != NOT_FLAT) {
				for (const Cell& cell : entries_.cells(run)) { matrix[cell.index] |= cell.bits; }
			} else {
				for (const TargetList& list : entries_.lists(id)) {
					uint64_t* const row = dense_row(list.class_id);
					for (const Chunk& chunk : entries_.chunks(list)) { row[chunk.word] |= chunk.bits; }
				}
			}
		}
		return read_out_rows(all_classes_, emit);
	}

	/// Call @p emit for the non-empty rows of @p classes until it returns false, and clear all rows.
	template <typename Emit> bool read_out_rows(const std::span<const ClassId> classes, Emit& emit) {
		union_chunks_.clear();
		union_chunks_.reserve(classes.size() * num_of_words_);
		bool proceed = true;
		for (const ClassId class_id : classes) {
			bucket_head_[class_id] = NO_ID;
			uint64_t* const row = dense_row(class_id);
			const size_t first_chunk = union_chunks_.size();
			for (size_t word = 0; word < num_of_words_; ++word) {
				if (row[word] != 0) {
					union_chunks_.push_back(Chunk{row[word], word});
					row[word] = 0;
				}
			}
			if (proceed && union_chunks_.size() > first_chunk) {
				proceed = emit(class_id, ChunkSpan{union_chunks_}.subspan(first_chunk));
			}
		}
		return proceed;
	}

	/// Collect the lists in per-class buckets and unite each: one list as it is, two merged, more ORed into
	///  word_accumulator_.
	template <typename Emit> bool unite_in_buckets(const std::span<const EntryId> entry_ids, Emit& emit) {
		touched_classes_.clear();
		link_lists_.clear();
		link_next_.clear();
		size_t num_of_chunks = 0;
		// Put every list into the bucket of its class.
		for (const EntryId id : entry_ids) {
			for (const TargetList& list : entries_.lists(id)) {
				if (bucket_head_[list.class_id] == NO_ID) { touched_classes_.push_back(list.class_id); }
				link_lists_.push_back(&list); // The entry table does not change meanwhile.
				link_next_.push_back(bucket_head_[list.class_id]);
				bucket_head_[list.class_id] = static_cast<uint32_t>(link_next_.size() - 1);
				num_of_chunks += list.num_of_chunks;
			}
		}
		sort_touched_classes();

		union_chunks_.clear();
		union_chunks_.reserve(num_of_chunks); // A union has at most the chunks of its lists.
		bool proceed = true;
		// Unite each bucket in class order.
		for (const ClassId class_id : touched_classes_) {
			const uint32_t first = bucket_head_[class_id];
			bucket_head_[class_id] = NO_ID;
			if (!proceed) { continue; } // If stopped, only reset the buckets.
			const uint32_t second = link_next_[first];
			if (second == NO_ID) {
				proceed = emit(class_id, entries_.chunks(*link_lists_[first]));
			} else if (link_next_[second] == NO_ID) {
				const size_t first_chunk = union_chunks_.size();
				merge_sorted(
					entries_.chunks(*link_lists_[first]), entries_.chunks(*link_lists_[second]), union_chunks_,
					&Chunk::word, unite_chunks
				);
				proceed = emit(class_id, ChunkSpan{union_chunks_}.subspan(first_chunk));
			} else {
				proceed = emit(class_id, unite_bucket(first));
			}
		}
		return proceed;
	}

	/// Unite the lists of the bucket starting at the link @p first in word_accumulator_.
	ChunkSpan unite_bucket(const uint32_t first) {
		touched_words_.clear();
		uint64_t min_word = std::numeric_limits<uint64_t>::max();
		uint64_t max_word = 0;
		// OR the lists into word_accumulator_, noting the touched words and their range.
		for (uint32_t link = first; link != NO_ID; link = link_next_[link]) {
			for (const Chunk& chunk : entries_.chunks(*link_lists_[link])) {
				if (word_accumulator_[chunk.word] == 0) {
					touched_words_.push_back(chunk.word);
					min_word = std::min(min_word, chunk.word);
					max_word = std::max(max_word, chunk.word);
				}
				word_accumulator_[chunk.word] |= chunk.bits;
			}
		}

		// Read the touched words out in order by a scan of their range or by a sort, whichever is cheaper.
		const size_t first_chunk = union_chunks_.size();
		const auto take = [this](const uint64_t word) {
			union_chunks_.push_back(Chunk{word_accumulator_[word], word});
			word_accumulator_[word] = 0;
		};
		const size_t num_of_touched = touched_words_.size();

		if (max_word - min_word + 1 <= std::max(8 * num_of_touched, sort_cost(num_of_touched))) {
			for (uint64_t word = min_word; word <= max_word; ++word) {
				if (word_accumulator_[word] != 0) { take(word); }
			}
		} else {
			std::sort(touched_words_.begin(), touched_words_.end());
			for (const uint64_t word : touched_words_) { take(word); }
		}

		return ChunkSpan{union_chunks_}.subspan(first_chunk);
	}

	/// Sort touched_classes_, or rebuild it by a scan if that is cheaper.
	void sort_touched_classes() {
		if (sort_cost(touched_classes_.size()) <= num_of_classes_) {
			std::sort(touched_classes_.begin(), touched_classes_.end());
			return;
		}
		touched_classes_.clear();
		for (ClassId class_id = 0; class_id < num_of_classes_; ++class_id) {
			if (bucket_head_[class_id] != NO_ID) { touched_classes_.push_back(class_id); }
		}
	}
};

/// Posts of sets of states under every symbol class from a table of block patterns.
/// These sets are cut into patterns of block_size() states, and the post of each pattern is computed once and kept.
/// The relation and the classes must outlive the table (the table keeps references to them).
class PostTable {
  public:
	/// Posts in @p delta under @p classes. With @p numbering, the sets use the numbers of the states in it.
	PostTable(
		const Delta& delta,
		const SymbolClasses& classes,
		const size_t num_of_states,
		StateNumbering* const numbering = nullptr
	)
		: entries_(delta, classes, num_of_states, numbering),
		  successors_(entries_, classes.num_of_classes(), words_for(num_of_states)) {}

	PostTable(const PostTable&) = delete;
	PostTable& operator=(const PostTable&) = delete;

	unsigned block_size() const { return block_size_; }

	/// Number of entries united by the last call, a measure of its work.
	size_t num_of_entries() const { return entry_ids_.size(); }

	/// Set the block size: a power of two up to 64 (1 means no table).
	void set_block_size(const unsigned block_size) { block_size_ = block_size; }

	/// Call @p emit(class_id, targets) for every post of @p states, by class, until it returns false. Returns false if
	///  stopped. The targets are valid until the next call and do not point into @p states, which is read before the
	///  first call, so @p emit may move it.
	template <typename Emit> bool for_each_post(const ChunkSpan states, Emit&& emit) {
		entry_ids_.clear();
		// The entry of every block pattern of the set.
		for (const Chunk& chunk : states) {
			for_each_block_pattern(chunk, block_size_, [this](const Chunk pattern) {
				entry_ids_.push_back(entries_.entry_of(pattern));
			});
		}
		return successors_.for_each_union(entry_ids_, emit);
	}

  private:
	EntryTable entries_;
	SuccessorUnion successors_;
	std::vector<EntryId> entry_ids_ = {}; ///< Entries of the current set.
	unsigned block_size_ = 1;
};

// ===== The table of sets of states, and the output buffer.

/// Sets of states, numbered in the order they are added.
/// Stored in one vector and found through a hash table of their ids, for speed.
class StateSetTable {
  public:
	size_t size() const { return offsets_.size() - 1; }

	/// Memory of the hash table, in bytes.
	size_t table_bytes() const { return table_.size() * sizeof(Slot); }

	/// The set @p id. Valid until the next insertion.
	ChunkSpan chunks(const size_t id) const { return {chunks_.data() + offsets_[id], offsets_[id + 1] - offsets_[id]}; }

	void reserve(const size_t num_of_sets, const size_t num_of_chunks) {
		chunks_.reserve(num_of_chunks);
		offsets_.reserve(num_of_sets + 1);
		if (2 * num_of_sets > table_.size()) { rehash(std::bit_ceil(2 * num_of_sets)); }
	}

	struct Key {
		ChunkSpan states;
		uint32_t hash;
	};

	/// Key of @p states. It also prefetches the slot.
	Key key_of(const ChunkSpan states) const {
		// Hash the states, in two independent chains of multiplications for speed.
		uint64_t bits_hash = states.size() * GOLDEN;
		uint64_t words_hash = mata::utils::HASH_SEED;
		for (const Chunk& chunk : states) {
			bits_hash = (std::rotl(bits_hash, 23) ^ chunk.bits) * GOLDEN;
			words_hash = (std::rotl(words_hash, 23) ^ chunk.word) * GOLDEN;
		}
		const auto hash = static_cast<uint32_t>(mata::utils::hash_mix(bits_hash ^ std::rotl(words_hash, 32)));

		// The slot is likely a cache miss: fetch it now, so the miss overlaps with the work before its lookup.
#if defined(__GNUC__) || defined(__clang__) // A builtin of GCC and Clang (standard C++ has no prefetch)
		__builtin_prefetch(&table_[hash & (table_.size() - 1)]);
#endif
		return Key{states, hash};
	}

	/// Id of @p states, and whether it is new. @p states must not point into the table.
	std::pair<uint32_t, bool> find_or_insert(const ChunkSpan states) { return find_or_insert(key_of(states)); }

	std::pair<uint32_t, bool> find_or_insert(const Key& key) {
		const size_t slot_mask = table_.size() - 1;
		size_t slot = key.hash & slot_mask;
		// Linear probing, until the set or an empty slot.
		for (; table_[slot].id != NO_ID; slot = (slot + 1) & slot_mask) {
			const Slot found = table_[slot];
			if (found.hash == key.hash && std::ranges::equal(chunks(found.id), key.states)) {
				return {found.id, false};
			}
		}

		const uint32_t id = to_id(size());
		// Not chunks_.insert() (GCC keeps it out of line, slower); C++23 append_range() would do, but GCC 14 lacks it.
		for (const Chunk& chunk : key.states) { chunks_.push_back(chunk); }
		offsets_.push_back(chunks_.size());
		table_[slot] = Slot{id, key.hash};
		if (2 * size() > table_.size()) { rehash(2 * table_.size()); }
		return {id, true};
	}

  private:
	/// A slot: an id and its hash, so that most probes need not read the sets.
	struct Slot {
		uint32_t id; ///< NO_ID if the slot is empty.
		uint32_t hash;
	};

	static constexpr size_t INITIAL_SLOTS = 64;
	static constexpr Slot EMPTY_SLOT = {NO_ID, 0};

	std::vector<Chunk> chunks_ = {};
	std::vector<size_t> offsets_ = {0}; ///< Set i is chunks_[offsets_[i] .. offsets_[i + 1]).
	std::vector<Slot> table_ = std::vector<Slot>(INITIAL_SLOTS, EMPTY_SLOT); ///< A power of two.

	void rehash(const size_t num_of_slots) {
		const std::vector<Slot> old_table = std::move(table_);
		table_.assign(num_of_slots, EMPTY_SLOT);
		const size_t slot_mask = table_.size() - 1;
		for (const Slot& entry : old_table) {
			if (entry.id == NO_ID) { continue; }
			size_t slot = entry.hash & slot_mask;
			while (table_[slot].id != NO_ID) { slot = (slot + 1) & slot_mask; }
			table_[slot] = entry;
		}
	}
};

/// Transitions of one state, collected by class and written to a Delta by symbol.
class TransitionBuffer {
  public:
	explicit TransitionBuffer(const SymbolClasses& classes)
		: classes_(classes),
		  target_of_class_(classes.num_of_classes(), NO_STATE) {}

	/// Add a transition under @p class_id (classes in increasing order).
	void push(const ClassId class_id, const State target) { pending_.emplace_back(class_id, target); }

	/// Append the collected transitions of @p source to @p delta, and empty the buffer.
	void flush(Delta& delta, const State source) {
		if (pending_.empty()) { return; }
		StatePost& state_post = delta.mutable_state_post(source);
		if (classes_.all_singletons()) {
			// Class i is the symbol with index i.
			state_post.reserve(state_post.size() + pending_.size());
			for (const auto& [class_id, target] : pending_) {
				append_transition(state_post, classes_.symbol(class_id), target);
			}
		} else {
			append_expanded(state_post);
		}
		pending_.clear();
	}

  private:
	const SymbolClasses& classes_;
	std::vector<std::pair<ClassId, State>> pending_ = {};
	std::vector<std::pair<Symbol, State>> expanded_ = {}; ///< Scratch of append_expanded().
	std::vector<State> target_of_class_; ///< Scratch of append_expanded(); NO_STATE: no transition.

	/// Append the transitions, expanded to symbols, in symbol order: by a sort or by a scan of all symbols, whichever
	///  is cheaper.
	void append_expanded(StatePost& state_post) {
		size_t num_of_expanded = 0;
		for (const auto& [class_id, target] : pending_) { num_of_expanded += classes_.members(class_id).size(); }
		state_post.reserve(state_post.size() + num_of_expanded);

		if (sort_cost(num_of_expanded) <= classes_.num_of_symbols()) {
			expanded_.clear();
			// Expand every class to its symbols, then sort by symbol.
			for (const auto& [class_id, target] : pending_) {
				for (const SymbolIndex index : classes_.members(class_id)) {
					expanded_.emplace_back(classes_.symbol(index), target);
				}
			}
			std::sort(expanded_.begin(), expanded_.end());
			for (const auto& [symbol, target] : expanded_) { append_transition(state_post, symbol, target); }
			return;
		}

		// Mark the target of each class, take the symbols in order, then clear the marks.
		for (const auto& [class_id, target] : pending_) { target_of_class_[class_id] = target; }
		for (SymbolIndex index = 0; index < classes_.num_of_symbols(); ++index) {
			const State target = target_of_class_[classes_.class_of(index)];
			if (target != NO_STATE) { append_transition(state_post, classes_.symbol(index), target); }
		}
		for (const auto& [class_id, target] : pending_) { target_of_class_[class_id] = NO_STATE; }
	}
};

// ===== Renumbering of the NFA states for locality.

/// Every stride-th set of a StateSetTable.
struct SetSample {
	const StateSetTable& sets;
	size_t stride;

	size_t size() const { return (sets.size() + stride - 1) / stride; }

	template <typename F> void for_each_set(const F f) const {
		for (size_t id = 0; id < sets.size(); id += stride) { f(sets.chunks(id)); }
	}
};

/// Bit vector that collects a set of states in any order and gives it out as chunks.
class DenseWords {
  public:
	explicit DenseWords(const size_t num_of_states) : bits_(words_for(num_of_states), 0) {}

	void add(const Chunk chunk) {
		if (bits_[chunk.word] == 0) { touched_.push_back(chunk.word); }
		bits_[chunk.word] |= chunk.bits;
	}

	/// Call @p f on the non-zero words (in any order) and clear them.
	template <typename F> void take_each(const F f) {
		for (const uint64_t word : touched_) {
			f(Chunk{bits_[word], word});
			bits_[word] = 0;
		}
		touched_.clear();
	}

	/// Append the non-zero words to @p out in order and clear them.
	void take_sorted(std::vector<Chunk>& out) {
		std::sort(touched_.begin(), touched_.end());
		take_each([&](const Chunk chunk) { out.push_back(chunk); });
	}

  private:
	std::vector<uint64_t> bits_;
	std::vector<uint64_t> touched_ = {};
};

/// Call @p f(number[q]) for every state q of @p states.
template <typename F> void for_each_renumbered(const ChunkSpan states, const std::vector<State>& number, const F f) {
	for (const Chunk& chunk : states) {
		for (const unsigned bit : SetBits{chunk.bits}) { f(number[state_at(chunk.word, bit)]); }
	}
}

/// Chunks plus block patterns of the sets of @p sample: an estimate of their work.
size_t locality_cost(const SetSample& sample, const unsigned block_size) {
	size_t cost = 0;
	sample.for_each_set([&](const ChunkSpan states) {
		for (const Chunk& chunk : states) {
			cost += 1;
			for_each_block_pattern(chunk, block_size, [&](const Chunk) { ++cost; });
		}
	});
	return cost;
}

/// locality_cost() with every state q renumbered to @p number[q].
size_t locality_cost(
	const SetSample& sample, const unsigned block_size, const std::vector<State>& number, const size_t num_of_numbers
) {
	size_t cost = 0;
	DenseWords words(num_of_numbers);
	sample.for_each_set([&](const ChunkSpan states) {
		// One bit per block, so the popcount of a word is its number of patterns.
		for_each_renumbered(states, number, [&](const State q) {
			words.add(Chunk{uint64_t{1} << ((q % WORD_BITS) / block_size), q / WORD_BITS});
		});
		words.take_each([&](const Chunk chunk) { cost += 1 + static_cast<size_t>(std::popcount(chunk.bits)); });
	});
	return cost;
}

/// Lower bound of locality_cost() over all numberings.
size_t minimal_locality_cost(const SetSample& sample, const unsigned block_size) {
	size_t cost = 0;
	sample.for_each_set([&](const ChunkSpan states) {
		size_t num_of_states = 0;
		for (const Chunk& chunk : states) { num_of_states += static_cast<size_t>(std::popcount(chunk.bits)); }
		cost += (num_of_states + WORD_BITS - 1) / WORD_BITS + (num_of_states + block_size - 1) / block_size;
	});
	return cost;
}

/// States of the sets of @p sample, the most frequent first (ties: the first seen first).
std::vector<State> order_by_frequency(const SetSample& sample, const size_t num_of_states) {
	std::vector<uint32_t> frequency(num_of_states, 0);
	std::vector<State> order;
	sample.for_each_set([&](const ChunkSpan states) {
		for (const Chunk& chunk : states) {
			for (const unsigned bit : SetBits{chunk.bits}) {
				const State q = state_at(chunk.word, bit);
				if (frequency[q]++ == 0) { order.push_back(q); }
			}
		}
	});
	std::ranges::stable_sort(order, std::ranges::greater{}, [&](const State q) { return frequency[q]; });
	return order;
}

/// A permutation of the state numbers for renumbering the states of a StateSetTable to improve locality of the sets.
class Reordering {
  public:
	/// The states of @p order get the numbers 0, 1, ..., the other states the next numbers.
	Reordering(std::vector<State> order, const size_t num_of_states)
		: old_of_(std::move(order)),
		  new_of_(num_of_states, NO_STATE) {
		for (State q = 0; q < old_of_.size(); ++q) { new_of_[old_of_[q]] = q; }
		for (State q = 0; q < num_of_states; ++q) {
			if (new_of_[q] == NO_STATE) {
				new_of_[q] = static_cast<State>(old_of_.size());
				old_of_.push_back(q);
			}
		}
	}

	/// Old number of each new number.
	const std::vector<State>& order() const { return old_of_; }

	/// New number of each old number.
	const std::vector<State>& new_numbers() const { return new_of_; }

	/// @p sets with renumbered states (same ids).
	StateSetTable renumber(const StateSetTable& sets) const {
		size_t num_of_chunks = 0; // An estimate.
		for (size_t id = 0; id < sets.size(); ++id) { num_of_chunks += sets.chunks(id).size(); }
		StateSetTable renumbered;
		renumbered.reserve(sets.size(), num_of_chunks);
		std::vector<Chunk> chunks;
		DenseWords words(new_of_.size());
		// Renumber each set in id order; its chunks are sorted by word again.
		for (size_t id = 0; id < sets.size(); ++id) {
			for_each_renumbered(sets.chunks(id), new_of_, [&](const State q) { words.add(chunk_of(q)); });
			chunks.clear();
			words.take_sorted(chunks);
			renumbered.find_or_insert(chunks); // Distinct sets stay distinct: same id.
		}
		return renumbered;
	}

  private:
	std::vector<State> old_of_;
	std::vector<State> new_of_;
};

// ===== The subset construction.

/// The subset construction using the Four Russians speedup.
/// Macrostates are numbered (as DFA states) and processed in the order they are found.
/// Their posts come from a PostTable.
class FourRussiansDeterminizer {
  public:
	FourRussiansDeterminizer(const Nfa& aut, const FourRussiansOptions& options, const MacrostateDiscover& discover)
		: aut_(aut),
		  discover_(discover),
		  fixed_block_size_(checked_block_size(options.block_size)),
		  // A discovery callback needs the transitions in symbol order: one class per symbol then.
		  classes_(aut.delta, !discover.has_value()),
		  numbering_(aut.num_of_states()),
		  post_(std::in_place, aut.delta, classes_, numbering_.num_of_states(), &numbering_),
		  transitions_(classes_),
		  final_bits_(words_for(aut.num_of_states()), 0) {
		post_->set_block_size(fixed_block_size_ == 0 ? 1 : fixed_block_size_);
	}

	FourRussiansDeterminizer(const FourRussiansDeterminizer&) = delete;
	FourRussiansDeterminizer& operator=(const FourRussiansDeterminizer&) = delete;

	Nfa run(SubsetMap* const subset_map) {
		subset_map_ = subset_map;
		std::vector<State> initial_states(aut_.initial.begin(), aut_.initial.end());
		std::sort(initial_states.begin(), initial_states.end());
		std::vector<Chunk> chunks;
		// Numbered in increasing order, so the numbers increase, too.
		for (const State q : initial_states) { push_state(chunks, 0, numbering_.number(q)); }
		const State initial = find_or_add_macrostate(macrostates_.key_of(chunks)).first;
		result_.initial.insert(initial);

		if (!aut_.delta.empty() && report_discovery(initial)) {
			bool next_computed = false;
			// Expand the macrostates in the order they are found.
			for (size_t current = 0; current < macrostates_.size(); ++current) {
				if (macrostates_.size() >= renumber_at_) {
					try_renumbering(current);
					next_computed = false; // Computed with the old numbers.
				}
				if (!next_computed && lookup_ == Lookup::Direct) {
					if (!expand_directly(current)) { break; }
					continue;
				}
				Successors& successors = successors_[current % 2];
				if (!next_computed) { compute_successors(current, successors, lookup_ == Lookup::Pipelined); }
				// Pipelining: compute the successors of the next macrostate before looking up the current ones, so that
				//  their cache misses overlap (the successors are copied for that).
				next_computed = lookup_ == Lookup::Pipelined && current + 1 < macrostates_.size();
				if (next_computed) { compute_successors(current + 1, successors_[(current + 1) % 2], true); }
				lookup_ = choose_lookup(successors.num_of_chunks, successors.posts.size());
				if (!expand(current, successors)) { break; }
			}
		}

		if (subset_map != nullptr) {
			subset_map->reserve(subset_map->size() + macrostates_.size());
			for (size_t id = 0; id < macrostates_.size(); ++id) {
				(*subset_map)[outer_states(id)] = static_cast<State>(id);
			}
		}
		return std::move(result_);
	}

  private:
	const Nfa& aut_;
	const MacrostateDiscover& discover_;
	SubsetMap* subset_map_ = nullptr; ///< Also filled at each discovery, so that the callback can read it.
	const unsigned fixed_block_size_; ///< 0: automatic.
	SymbolClasses classes_;
	/// Numbers of the NFA states inside the construction, permuted by try_renumbering().
	StateNumbering numbering_;
	std::optional<PostTable> post_; ///< Optional to be rebuilt by a renumbering.
	StateSetTable macrostates_ = {};

	/// A post of a macrostate with its hash.
	struct Successor {
		ClassId class_id;
		StateSetTable::Key targets;
	};

	/// The posts of a macrostate, hashed (and copied in pipelined mode).
	struct Successors {
		std::vector<Successor> posts = {};
		size_t num_of_chunks = 0; ///< Of all targets.
		std::vector<Chunk> chunks = {}; ///< Copied targets (pipelined mode).
	};

	/// How the successors are looked up in the macrostate table: direct (one by one, for small tables and light
	///  unions), batched (all hashed and prefetched first) or pipelined (also overlapping the next macrostate, for
	///  large tables).
	enum class Lookup { Direct, Batched, Pipelined };

	static constexpr size_t PREFETCH_MIN_TABLE_BYTES = size_t{1} << 20;
	static constexpr size_t DIRECT_MAX_ENTRIES = 32;
	static constexpr size_t PIPELINING_MAX_CHUNKS_PER_POST = 8;

	std::array<Successors, 2> successors_ = {}; ///< Of the current macrostate and of the next one.
	Lookup lookup_ = Lookup::Direct; ///< For the next macrostate (choose_lookup()).
	TransitionBuffer transitions_;
	std::vector<uint64_t> final_bits_; ///< Final NFA states, as a bit vector.
	State num_of_final_bits_ = 0; ///< States numbered below this have their bits set.
	size_t renumber_at_ = RENUMBER_AT; ///< When to try the next renumbering, or NEVER.
	Nfa result_ = Nfa();

	/// Add the bits of the final states numbered since the last call.
	void add_final_bits() {
		for (; num_of_final_bits_ < numbering_.num_of_numbered(); ++num_of_final_bits_) {
			if (aut_.final.contains(numbering_.outer(num_of_final_bits_))) {
				final_bits_[num_of_final_bits_ / WORD_BITS] |= chunk_of(num_of_final_bits_).bits;
			}
		}
	}

	/// Macrostate @p id as a set of NFA states.
	StateSet outer_states(const size_t id) const { return numbering_.to_outer(macrostates_.chunks(id)); }

	/// A renumbering is tried at RENUMBER_AT macrostates, then RENUMBER_GROWTH times later, up to LAST_RENUMBER_AT.
	static constexpr size_t RENUMBER_AT = 2'048;
	static constexpr size_t RENUMBER_GROWTH = 8;
	static constexpr size_t LAST_RENUMBER_AT = RENUMBER_AT * RENUMBER_GROWTH;
	/// Macrostates sampled for a renumbering.
	static constexpr size_t RENUMBER_SAMPLE = 1'024;
	/// A renumbering must save at least 1/RENUMBER_MIN_SAVING of the estimated work.
	static constexpr size_t RENUMBER_MIN_SAVING = 8;
	/// A cheap pilot on every RENUMBER_PILOT_STRIDE-th sampled macrostate must first save 1/RENUMBER_PILOT_MIN_SAVING.
	static constexpr size_t RENUMBER_PILOT_STRIDE = 4;
	static constexpr size_t RENUMBER_PILOT_MIN_SAVING = 10;
	/// Work of a macrostate that the numbering does not change, in units of locality_cost().
	static constexpr size_t MACROSTATE_COST = 6;
	static constexpr size_t NEVER = std::numeric_limits<size_t>::max();

	/// Renumber the NFA states by how often they occur in the macrostates, if that saves enough work. The macrostates
	///  keep their ids.
	void try_renumbering(const size_t num_of_expanded) {
		renumber_at_ = macrostates_.size() < LAST_RENUMBER_AT ? macrostates_.size() * RENUMBER_GROWTH : NEVER;
		size_t num_of_transitions = 0;

		for (State q = 0; q < num_of_expanded; ++q) { num_of_transitions += result_.delta[q].size(); }

		const size_t stride = std::max<size_t>(1, macrostates_.size() / RENUMBER_SAMPLE);
		const SetSample pilot{macrostates_, RENUMBER_PILOT_STRIDE * stride};

		if (!reordering_saving(pilot, num_of_transitions, num_of_expanded, RENUMBER_PILOT_MIN_SAVING)) { return; }

		const SetSample sample{macrostates_, stride};
		const std::optional<Reordering> step =
			reordering_saving(sample, num_of_transitions, num_of_expanded, RENUMBER_MIN_SAVING);

		if (!step.has_value()) { return; }

		// Continue the construction over the new numbers.
		const unsigned block_size = post_->block_size();
		macrostates_ = step->renumber(macrostates_);
		numbering_.permute(step->order());
		post_.emplace(aut_.delta, classes_, numbering_.num_of_states(), &numbering_);
		post_->set_block_size(block_size);
		std::ranges::fill(final_bits_, 0);
		num_of_final_bits_ = 0;
		add_final_bits();
	}

	/// Renumbering by frequency in @p sample, if it saves at least 1/@p min_saving of the estimated work.
	std::optional<Reordering> reordering_saving(
		const SetSample& sample, const size_t num_of_transitions, const size_t num_of_expanded, const size_t min_saving
	) const {
		const unsigned block_size = post_->block_size();
		const size_t fixed_cost = sample.size() * MACROSTATE_COST +
								  num_of_transitions * sample.size() / (4 * std::max<size_t>(1, num_of_expanded));
		const size_t cost = fixed_cost + locality_cost(sample, block_size);
		const auto saves_enough = [&](const size_t new_cost) {
			return min_saving * new_cost <= (min_saving - 1) * cost;
		};
		// No numbering could save enough:
		if (!saves_enough(fixed_cost + minimal_locality_cost(sample, block_size))) { return std::nullopt; }
		const size_t num_of_numbered = numbering_.num_of_numbered();
		Reordering step(order_by_frequency(sample, num_of_numbered), num_of_numbered);
		const size_t new_cost = fixed_cost + locality_cost(sample, block_size, step.new_numbers(), num_of_numbered);
		if (!saves_enough(new_cost)) { return std::nullopt; }
		return step;
	}

	/// Set the automatic block size, unless the block size is fixed.
	void update_block_size() {
		if (fixed_block_size_ == 0) { post_->set_block_size(auto_block_size(macrostates_.size())); }
	}

	/// Compute the successors of @p macrostate, copying their targets if @p copy.
	void compute_successors(const size_t macrostate, Successors& successors, const bool copy) {
		update_block_size();
		successors.posts.clear();
		successors.num_of_chunks = 0;
		post_->for_each_post(macrostates_.chunks(macrostate), [&](const ClassId class_id, const ChunkSpan targets) {
			successors.posts.push_back(Successor{class_id, StateSetTable::Key{targets, 0}}); // Hashed below.
			successors.num_of_chunks += targets.size();
			return true;
		});
		if (copy) {
			successors.chunks.clear();
			successors.chunks.reserve(successors.num_of_chunks); // Keys point into it: it must not move.
		}
		// Copy the targets if @p copy, then hash them (which prefetches their slots).
		for (Successor& successor : successors.posts) {
			ChunkSpan targets = successor.targets.states;
			if (copy) {
				const size_t first_chunk = successors.chunks.size();
				successors.chunks.insert(successors.chunks.end(), targets.begin(), targets.end());
				targets = ChunkSpan{successors.chunks}.subspan(first_chunk);
			}
			successor.targets = macrostates_.key_of(targets);
		}
	}

	/// Add the transitions of @p current, looking up each successor as it comes.
	/// Returns false if the callback stops.
	bool expand_directly(const size_t current) {
		update_block_size();
		size_t num_of_posts = 0;
		size_t num_of_chunks = 0;
		// (The macrostate is read before any new macrostate is added.)
		const bool proceed =
			post_->for_each_post(macrostates_.chunks(current), [&](const ClassId class_id, const ChunkSpan targets) {
				++num_of_posts;
				num_of_chunks += targets.size();
				return add_transition(current, class_id, macrostates_.key_of(targets));
			});
		transitions_.flush(result_.delta, static_cast<State>(current));
		lookup_ = choose_lookup(num_of_chunks, num_of_posts);
		return proceed;
	}

	/// Lookup mode for the next macrostate.
	Lookup choose_lookup(const size_t num_of_chunks, const size_t num_of_posts) const {
		if (macrostates_.table_bytes() < PREFETCH_MIN_TABLE_BYTES) {
			return post_->num_of_entries() <= DIRECT_MAX_ENTRIES ? Lookup::Direct : Lookup::Batched;
		}
		return num_of_chunks <= PIPELINING_MAX_CHUNKS_PER_POST * num_of_posts ? Lookup::Pipelined : Lookup::Batched;
	}

	/// Add the transitions of @p current to @p successors. Returns false if the callback stops.
	bool expand(const size_t current, const Successors& successors) {
		bool proceed = true;
		for (const Successor& successor : successors.posts) {
			proceed = add_transition(current, successor.class_id, successor.targets);
			if (!proceed) { break; }
		}
		transitions_.flush(result_.delta, static_cast<State>(current));
		return proceed;
	}

	/// Add the transition from @p source under @p class_id to the macrostate of @p targets. Returns false if the
	///  callback stops.
	bool add_transition(const size_t source, const ClassId class_id, const StateSetTable::Key& targets) {
		const auto [target, is_new] = find_or_add_macrostate(targets);
		transitions_.push(class_id, target);

		if (!is_new || !discover_.has_value()) { return true; }
		transitions_.flush(result_.delta, static_cast<State>(source)); // The callback sees the DFA built so far.
		return report_discovery(target);
	}

	/// DFA state of the macrostate of @p key, and whether it is new.
	std::pair<State, bool> find_or_add_macrostate(const StateSetTable::Key& key) {
		const auto [id, is_new] = macrostates_.find_or_insert(key);
		if (!is_new) { return {id, false}; }

		const State state = result_.add_state(id); // Macrostate ids are the DFA states.
		add_final_bits(); // Of the states numbered for this post.
		if (intersects(key.states, final_bits_)) { result_.final.insert(state); }

		return {state, true};
	}

	/// Tell the callback, if any, about the new macrostate, which is in the subset map already. Returns false if it
	///  stops.
	bool report_discovery(const State macrostate) const {
		if (!discover_.has_value()) { return true; }
		StateSet states = outer_states(macrostate);
		if (subset_map_ != nullptr) { (*subset_map_)[states] = macrostate; }
		return (*discover_)(result_, macrostate, states);
	}
};

/// Subset construction of a deterministic @p aut: a copy of its reachable part, in O(n + m).
Nfa copy_deterministic(const Nfa& aut, SubsetMap* const subset_map) {
	// Reachable states in the order found, and the inverse map.
	std::vector<State> nfa_state = {*aut.initial.begin()};
	std::vector<State> dfa_state(aut.num_of_states(), NO_STATE);
	dfa_state[nfa_state[0]] = 0;
	for (size_t current = 0; current < nfa_state.size(); ++current) {
		for (const SymbolPost& symbol_post : aut.delta[nfa_state[current]]) {
			const State target = symbol_post.targets.front();
			if (dfa_state[target] == NO_STATE) {
				dfa_state[target] = static_cast<State>(nfa_state.size());
				nfa_state.push_back(target);
			}
		}
	}

	const size_t num_of_states = nfa_state.size();
	Nfa result(num_of_states);
	result.initial.insert(0);
	// Copy the final states and the transitions, renumbered.
	for (State state = 0; state < num_of_states; ++state) {
		if (aut.final.contains(nfa_state[state])) { result.final.insert(state); }
		const StatePost& nfa_post = aut.delta[nfa_state[state]];
		StatePost& dfa_post = result.delta.mutable_state_post(state);
		dfa_post.reserve(nfa_post.size());
		for (const SymbolPost& symbol_post : nfa_post) {
			append_transition(dfa_post, symbol_post.symbol, dfa_state[symbol_post.targets.front()]);
		}
	}
	if (subset_map != nullptr) {
		for (State state = 0; state < num_of_states; ++state) { (*subset_map)[StateSet{nfa_state[state]}] = state; }
	}
	return result;
}

// ===== The reductions.

/// Removes from a set of states the states that another state of the set dominates.
/// Pruned macrostates still give a DFA for the same language (van Glabbeek and Ploeger, 2008).
class SimulationPruner {
  public:
	/// Pruning by the dominators that @p for_each_dominator(q, add) adds for every state q in increasing order.
	/// Domination must be a strict partial order, and p may dominate q only if L(q) is a subset of L(p).
	template <typename ForEachDominator>
	SimulationPruner(const size_t num_of_states, const ForEachDominator for_each_dominator)
		: dominated_(words_for(num_of_states), 0),
		  members_(words_for(num_of_states), 0) {
		dominator_offsets_.reserve(num_of_states + 1);
		dominator_offsets_.push_back(0);
		// The dominators of each state; a state with some is dominated.
		for (State q = 0; q < num_of_states; ++q) {
			const size_t first = dominator_chunks_.size();
			for_each_dominator(q, [&](const State p) { push_state(dominator_chunks_, first, p); });
			if (dominator_chunks_.size() > first) { dominated_[q / WORD_BITS] |= chunk_of(q).bits; }
			dominator_offsets_.push_back(dominator_chunks_.size());
		}
	}

	bool prunes_nothing() const {
		return std::ranges::all_of(dominated_, [](const uint64_t bits) { return bits == 0; });
	}

	/// @p states without its dominated states.
	/// It is @p states itself if nothing changes, otherwise valid until the next call.
	ChunkSpan prune(const ChunkSpan states) {
		// Most sets have no state with a dominator.
		return intersects(states, dominated_) ? prune_candidates(states) : states;
	}

  private:
	std::vector<uint64_t> dominated_; ///< States with a dominator.
	/// Dominators of q are dominator_chunks_[dominator_offsets_[q] .. dominator_offsets_[q + 1]).
	std::vector<size_t> dominator_offsets_ = {};
	std::vector<Chunk> dominator_chunks_ = {};
	std::vector<uint64_t> members_; ///< The set being pruned; all zero between calls.
	std::vector<Chunk> pruned_ = {};

	ChunkSpan dominators(const State q) const {
		return {dominator_chunks_.data() + dominator_offsets_[q], dominator_offsets_[q + 1] - dominator_offsets_[q]};
	}

	ChunkSpan prune_candidates(const ChunkSpan states) {
		for (const Chunk& chunk : states) { members_[chunk.word] = chunk.bits; }
		pruned_.clear();
		// Keep the states without a dominator in the set (marked in members_).
		for (const Chunk& chunk : states) {
			uint64_t bits = chunk.bits;
			// Domination is transitive, so checking against the whole set is enough.
			for (const unsigned bit : SetBits{chunk.bits & dominated_[chunk.word]}) {
				if (intersects(dominators(state_at(chunk.word, bit)), members_)) { bits &= ~(uint64_t{1} << bit); }
			}
			if (bits != 0) { pruned_.push_back(Chunk{bits, chunk.word}); }
		}
		for (const Chunk& chunk : states) { members_[chunk.word] = 0; }
		return pruned_;
	}
};

/// Simply disables pruning.
struct NoPruning {
	static ChunkSpan prune(const ChunkSpan states) { return states; }
};

/// Subset construction with every macrostate pruned by a Pruner.
/// It could be done inside FourRussiansDeterminizer with less redundant code, but is kept apart for GCC's inlining.
template <typename Pruner> class PrunedDeterminizer {
  public:
	PrunedDeterminizer(const Nfa& aut, const FourRussiansOptions& options, Pruner& pruner, SymbolClasses classes)
		: aut_(aut),
		  pruner_(pruner),
		  fixed_block_size_(checked_block_size(options.block_size)),
		  classes_(std::move(classes)),
		  post_(aut.delta, classes_, aut.num_of_states()),
		  transitions_(classes_),
		  final_bits_(words_for(aut.num_of_states()), 0) {
		post_.set_block_size(fixed_block_size_ == 0 ? 1 : fixed_block_size_);
		for (const State q : aut.final) { final_bits_[q / WORD_BITS] |= chunk_of(q).bits; }
	}

	PrunedDeterminizer(const PrunedDeterminizer&) = delete;
	PrunedDeterminizer& operator=(const PrunedDeterminizer&) = delete;

	/// The DFA, or nothing after more than @p max_macrostates macrostates.
	std::optional<Nfa> run(const size_t max_macrostates, SubsetMap* const subset_map) {
		std::vector<State> initial_states(aut_.initial.begin(), aut_.initial.end());
		std::sort(initial_states.begin(), initial_states.end());
		std::vector<Chunk> chunks;
		for (const State q : initial_states) { push_state(chunks, 0, q); }
		result_.initial.insert(find_or_add_macrostate(pruner_.prune(chunks)));

		// Expand the macrostates in the order they are found, up to @p max_macrostates.
		for (size_t current = 0; current < macrostates_.size() && !aut_.delta.empty(); ++current) {
			if (macrostates_.size() > max_macrostates) { return std::nullopt; }
			if (fixed_block_size_ == 0) { post_.set_block_size(auto_block_size(macrostates_.size())); }
			// (The macrostate is read before any new macrostate is added.)
			post_.for_each_post(macrostates_.chunks(current), [&](const ClassId class_id, const ChunkSpan targets) {
				transitions_.push(class_id, find_or_add_macrostate(pruner_.prune(targets)));
				return true;
			});
			transitions_.flush(result_.delta, static_cast<State>(current));
		}

		if (subset_map != nullptr) {
			for (size_t id = 0; id < macrostates_.size(); ++id) {
				StateSet states;
				for (const Chunk& chunk : macrostates_.chunks(id)) {
					for (const unsigned bit : SetBits{chunk.bits}) { states.push_back(state_at(chunk.word, bit)); }
				}
				(*subset_map)[std::move(states)] = static_cast<State>(id);
			}
		}
		return std::move(result_);
	}

  private:
	const Nfa& aut_;
	Pruner& pruner_;
	const unsigned fixed_block_size_; ///< 0: automatic.
	SymbolClasses classes_;
	PostTable post_;
	StateSetTable macrostates_ = {};
	TransitionBuffer transitions_;
	std::vector<uint64_t> final_bits_; ///< Final NFA states, as a bit vector.
	Nfa result_ = Nfa();

	/// DFA state of the macrostate @p states (added if new).
	State find_or_add_macrostate(const ChunkSpan states) {
		const auto [id, is_new] = macrostates_.find_or_insert(states);
		if (!is_new) { return id; }
		const State state = result_.add_state(id); // Macrostate ids are the DFA states.
		if (intersects(states, final_bits_)) { result_.final.insert(state); }
		return state;
	}
};

/// Pruned subset construction, giving up after more than @p max_macrostates macrostates. (A function of its own: GCC
///  then inlines both constructions here, which is faster.)
std::optional<Nfa> determinize_pruned(
	const Nfa& aut,
	SymbolClasses classes,
	SimulationPruner& pruner,
	const FourRussiansOptions& options,
	SubsetMap* subset_map,
	const size_t max_macrostates
) {
	if (pruner.prunes_nothing()) { // Saves the check of every set.
		NoPruning no_pruning;
		return PrunedDeterminizer{aut, options, no_pruning, std::move(classes)}.run(max_macrostates, subset_map);
	}
	return PrunedDeterminizer{aut, options, pruner, std::move(classes)}.run(max_macrostates, subset_map);
}

/// Part of an automaton with the original state of each state.
struct PartialAutomaton {
	Nfa aut;
	std::vector<State> original; ///< Increasing.
};

/// The useful states of @p aut, renumbered in increasing order.
PartialAutomaton useful_part(const Nfa& aut, const mata::BoolVector& useful) {
	const size_t num_of_states = aut.num_of_states();
	std::vector<State> renamed(num_of_states, NO_STATE);
	PartialAutomaton result{Nfa(), {}};
	// Number the useful states in increasing order.
	for (State q = 0; q < num_of_states; ++q) {
		if (useful[q]) {
			renamed[q] = static_cast<State>(result.original.size());
			result.original.push_back(q);
		}
	}
	result.aut = Nfa(result.original.size());
	std::vector<State> targets;
	// Copy the transitions between useful states.
	for (State q = 0; q < result.original.size(); ++q) {
		StatePost& state_post = result.aut.delta.mutable_state_post(q);
		for (const SymbolPost& symbol_post : aut.delta[result.original[q]]) {
			targets.clear();
			for (const State target : symbol_post.targets) {
				if (renamed[target] != NO_STATE) { targets.push_back(renamed[target]); } // (Increasing.)
			}
			if (!targets.empty()) {
				state_post.push_back(SymbolPost(symbol_post.symbol, std::span<const State>(targets)));
			}
		}
	}
	for (const State q : aut.initial) {
		if (renamed[q] != NO_STATE) { result.aut.initial.insert(renamed[q]); }
	}
	for (const State q : aut.final) {
		if (renamed[q] != NO_STATE) { result.aut.final.insert(renamed[q]); }
	}
	return result;
}

/// @p aut with only the transitions under the first symbol of each class (same bisimulation and simulation).
Nfa restrict_to_class_representatives(const Nfa& aut, const SymbolClasses& classes) {
	Nfa restricted(aut.num_of_states(), aut.initial, aut.final);
	for (State q = 0; q < aut.num_of_states(); ++q) {
		StatePost& restricted_post = restricted.delta.mutable_state_post(q);
		for_each_symbol_post(
			aut.delta[q], classes.symbols(), [&](const SymbolIndex symbol, const std::span<const State> targets) {
				if (classes.members(classes.class_of(symbol)).front() == symbol) {
					restricted_post.push_back(SymbolPost(classes.symbol(symbol), targets));
				}
			}
		);
	}
	return restricted;
}

/// Partition of the states into blocks, numbered by their smallest states.
struct Blocks {
	std::vector<State> block_of;
	std::vector<State> first_state; ///< The smallest state of every block.
};

/// Coarsest forward bisimulation of @p aut, by refining signatures; each round recomputes only the states whose
///  successors moved.
Blocks bisimulation(const Nfa& aut) {
	const size_t num_of_states = aut.num_of_states();
	std::vector<std::vector<State>> predecessors(num_of_states);
	for (State q = 0; q < num_of_states; ++q) {
		for (const SymbolPost& symbol_post : aut.delta[q]) {
			for (const State target : symbol_post.targets) { predecessors[target].push_back(q); }
		}
	}

	std::vector<State> block_of(num_of_states, 0);
	for (const State q : aut.final) { block_of[q] = 1; }
	std::vector<size_t> block_size = {num_of_states - aut.final.size(), aut.final.size()};
	// Invariant: clean states have the signature of their block.
	std::vector<std::vector<uint64_t>> block_signature(2);
	std::vector<std::vector<uint64_t>> signature(num_of_states);
	std::vector<State> dirty(num_of_states);
	std::iota(dirty.begin(), dirty.end(), State{0});
	std::vector<bool> is_dirty(num_of_states, true);
	std::vector<std::pair<size_t, State>> hashed; // (hash of the signature, state) of the dirty states.
	std::vector<State> next_dirty;
	std::vector<std::pair<size_t, size_t>> groups; // Of equal signatures in a block: (first index in the run, size).
	std::vector<size_t> group_of;
	std::vector<State> new_block;

	// Per block: the last symbol post that added it (to add each block once).
	std::vector<uint64_t> block_stamp(num_of_states + 2, 0);
	uint64_t stamp = 0;
	const auto compute_signature = [&](const State q) {
		std::vector<uint64_t>& state_signature = signature[q];
		state_signature.clear();
		// The (symbol, target block) pairs of q, each once, sorted.
		for (const SymbolPost& symbol_post : aut.delta[q]) {
			const size_t first = state_signature.size();
			++stamp;
			for (const State target : symbol_post.targets) {
				if (block_stamp[block_of[target]] == stamp) { continue; }
				block_stamp[block_of[target]] = stamp;
				state_signature.push_back((uint64_t{symbol_post.symbol} << 32) | block_of[target]);
			}
			std::sort(state_signature.begin() + static_cast<std::ptrdiff_t>(first), state_signature.end());
		}
		return mata::utils::hash_range(state_signature.begin(), state_signature.end());
	};

	// Split the block of the dirty states in @p run by their signatures.
	const auto split_block = [&](const std::span<std::pair<size_t, State>> run) {
		const State block = block_of[run[0].second];
		groups.clear();
		group_of.resize(run.size());
		// Group the states with equal signatures (equal hashes are adjacent).
		for (size_t i = 0; i < run.size(); ++i) {
			size_t group = groups.size();
			for (size_t candidate = groups.size();
				 candidate-- > 0 && run[groups[candidate].first].first == run[i].first;) {
				if (signature[run[groups[candidate].first].second] == signature[run[i].second]) {
					group = candidate;
					break;
				}
			}
			if (group == groups.size()) { groups.emplace_back(i, 0); }
			++groups[group].second;
			group_of[i] = group;
		}
		// The group with the signature of the clean states keeps the block; without clean states, the largest group.
		size_t keeper = groups.size();
		if (run.size() < block_size[block]) {
			for (size_t candidate = 0; candidate < groups.size(); ++candidate) {
				if (signature[run[groups[candidate].first].second] == block_signature[block]) { keeper = candidate; }
			}
		} else {
			keeper = 0;
			for (size_t candidate = 1; candidate < groups.size(); ++candidate) {
				if (groups[candidate].second > groups[keeper].second) { keeper = candidate; }
			}
			block_signature[block] = signature[run[groups[keeper].first].second];
		}
		new_block.assign(groups.size(), block);
		// Every group but the keeper gets a new block.
		for (size_t candidate = 0; candidate < groups.size(); ++candidate) {
			if (candidate == keeper) { continue; }
			new_block[candidate] = to_id(block_size.size());
			block_size.push_back(0);
			block_signature.push_back(signature[run[groups[candidate].first].second]);
		}
		// Move the states to their new blocks; their predecessors become dirty.
		for (size_t i = 0; i < run.size(); ++i) {
			const State target_block = new_block[group_of[i]];
			if (target_block == block) { continue; }
			const State q = run[i].second;
			--block_size[block];
			++block_size[target_block];
			block_of[q] = target_block;
			for (const State p : predecessors[q]) { next_dirty.push_back(p); }
		}
	};

	// Split the blocks of the dirty states until no state is dirty.
	while (!dirty.empty()) {
		hashed.clear();
		for (const State q : dirty) { hashed.emplace_back(compute_signature(q), q); }
		// The dirty states of a block become adjacent, sorted by hash.
		std::ranges::sort(hashed, {}, [&](const auto& entry) {
			return std::pair{block_of[entry.second], entry.first};
		});
		next_dirty.clear();
		const auto same_block = [&](const auto& lhs, const auto& rhs) {
			return block_of[lhs.second] == block_of[rhs.second];
		};
		for (const auto run : hashed | std::views::chunk_by(same_block)) { split_block(run); }
		for (const State q : dirty) { is_dirty[q] = false; }
		dirty.clear();
		for (const State q : next_dirty) {
			if (!is_dirty[q]) {
				is_dirty[q] = true;
				dirty.push_back(q);
			}
		}
	}

	// Number the blocks by their smallest states.
	Blocks blocks{std::vector<State>(num_of_states), {}};
	std::vector<State> renumbered(block_size.size(), NO_STATE);
	for (State q = 0; q < num_of_states; ++q) {
		State& block = renumbered[block_of[q]];
		if (block == NO_STATE) {
			block = to_id(blocks.first_state.size());
			blocks.first_state.push_back(q);
		}
		blocks.block_of[q] = block;
	}
	return blocks;
}

/// @p aut with each block merged into one state, which has the transitions of its first state.
Nfa block_automaton(const Nfa& aut, const Blocks& blocks) {
	const size_t num_of_blocks = blocks.first_state.size();
	Nfa result(num_of_blocks);
	std::vector<State> targets;
	for (State block = 0; block < num_of_blocks; ++block) {
		for (const SymbolPost& symbol_post : aut.delta[blocks.first_state[block]]) {
			targets.clear();
			for (const State target : symbol_post.targets) { targets.push_back(blocks.block_of[target]); }
			result.delta.mutable_state_post(block).push_back(
				SymbolPost(symbol_post.symbol, SmallStateSet(targets.begin(), targets.end()))
			);
		}
	}
	for (const State q : aut.initial) { result.initial.insert(blocks.block_of[q]); }
	for (const State q : aut.final) { result.final.insert(blocks.block_of[q]); }
	return result;
}

/// Per state q of an automaton: the states that simulate q (q included), in increasing order.
using Simulators = Simlib::Util::BinaryRelation::IndexType;

/// Simulators of the states of @p aut by the forward simulation.
Simulators simulators(const Nfa& aut) {
	const Simlib::Util::BinaryRelation simulation =
		algorithms::compute_relation(aut, {{"relation", "simulation"}, {"direction", "forward"}});
	Simulators index;
	simulation.build_index(index); // index[q] = {p | simulation.get(q, p)}.
	return index;
}

/// Simulators from universal states only (O(m)): they simulate every state. A state is universal if it is final and has
///  a universal successor under every symbol.
Simulators universal_simulators(const Nfa& aut) {
	const size_t num_of_symbols = aut.delta.get_used_symbols().size();
	std::vector<bool> is_universal(aut.num_of_states(), false);
	for (const State q : aut.final) { is_universal[q] = true; }
	std::vector<State> universal(aut.final.begin(), aut.final.end());
	// Drop the candidates without a universal successor under some symbol, until none is dropped.
	for (bool changed = true; changed;) {
		changed = false;
		std::erase_if(universal, [&](const State q) {
			size_t num_of_covered = 0;
			for (const SymbolPost& symbol_post : aut.delta[q]) {
				num_of_covered +=
					std::ranges::any_of(symbol_post.targets, [&](const State target) { return is_universal[target]; });
			}
			if (num_of_covered == num_of_symbols) { return false; }
			is_universal[q] = false;
			changed = true;
			return true;
		});
	}
	std::sort(universal.begin(), universal.end());

	Simulators simulators_of(aut.num_of_states());
	// Each state is simulated by itself and by the first universal state.
	for (State q = 0; q < aut.num_of_states(); ++q) {
		if (universal.empty() || q == universal.front()) {
			simulators_of[q] = {q};
		} else {
			simulators_of[q] = {std::min(q, universal.front()), std::max(q, universal.front())};
		}
	}
	if (!universal.empty()) {
		// Universal states are equivalent.
		simulators_of[universal.front()].assign(universal.begin(), universal.end());
	}
	return simulators_of;
}

/// Automaton reduced for determinize_reduced(), with its pruner.
struct ReducedAutomaton {
	/// Transitions only under the first symbol of each class.
	Nfa aut;
	SymbolClasses classes;
	/// Per state: the original state it stands for (increasing).
	std::vector<State> original;
	SimulationPruner pruner;
};

/// Reduce @p aut for determinize_reduced(): trim it, keep one symbol per class, merge bisimilar states (and, with
///  Simulation, states that simulate each other), and build the pruner.
ReducedAutomaton reduce_for_determinization(const Nfa& aut, const FourRussiansReduction reduction) {
	// The trimmed automaton (a copy only if some state is useless).
	const mata::BoolVector useful = aut.get_useful_states();
	const bool all_useful = useful.count() == useful.size();
	const PartialAutomaton trimmed = all_useful ? PartialAutomaton{Nfa(), {}} : useful_part(aut, useful);
	const Nfa& trimmed_aut = all_useful ? aut : trimmed.aut;

	SymbolClasses classes(trimmed_aut.delta, true);
	const Nfa restricted = classes.all_singletons() ? Nfa() : restrict_to_class_representatives(trimmed_aut, classes);
	const Nfa& compressed = classes.all_singletons() ? trimmed_aut : restricted;
	const Blocks blocks = bisimulation(compressed);
	const size_t num_of_blocks = blocks.first_state.size();

	// Simulators of every block.
	const Nfa block_aut = block_automaton(compressed, blocks);
	const Simulators simulators_of = reduction == FourRussiansReduction::Simulation && num_of_blocks > 0
										 ? simulators(block_aut)
										 : universal_simulators(block_aut);

	// Classes of blocks that simulate each other, numbered by their smallest blocks (heads).
	std::vector<State> class_of(num_of_blocks, NO_STATE);
	std::vector<State> heads;
	for (State block = 0; block < num_of_blocks; ++block) {
		if (class_of[block] != NO_STATE) { continue; }
		class_of[block] = to_id(heads.size());
		heads.push_back(block);
		for (const size_t other : simulators_of[block]) {
			if (other > block && std::ranges::binary_search(simulators_of[other], block)) {
				class_of[other] = class_of[block];
			}
		}
	}
	const size_t num_of_classes = heads.size();

	// A class dominates another iff its head simulates the other head.
	SimulationPruner pruner(num_of_classes, [&](const State class_id, const auto add_dominator) {
		for (const size_t block : simulators_of[heads[class_id]]) {
			if (heads[class_of[block]] == block && class_of[block] != class_id) { add_dominator(class_of[block]); }
		}
	});

	// The quotient: each class gets the pruned transitions of the first state of its head.
	ReducedAutomaton result{
		Nfa(num_of_classes), std::move(classes), std::vector<State>(num_of_classes), std::move(pruner)
	};
	const auto class_of_state = [&](const State q) { return class_of[blocks.block_of[q]]; };
	std::vector<State> target_classes;
	std::vector<Chunk> targets;
	for (State class_id = 0; class_id < num_of_classes; ++class_id) {
		const State representative = blocks.first_state[heads[class_id]];
		result.original[class_id] = all_useful ? representative : trimmed.original[representative];
		for (const SymbolPost& symbol_post : compressed.delta[representative]) {
			target_classes.clear();
			for (const State target : symbol_post.targets) { target_classes.push_back(class_of_state(target)); }
			std::sort(target_classes.begin(), target_classes.end());
			targets.clear();
			for (const State target_class : target_classes) { push_state(targets, 0, target_class); }
			target_classes.clear();
			for (const Chunk& chunk : result.pruner.prune(targets)) {
				for (const unsigned bit : SetBits{chunk.bits}) { target_classes.push_back(state_at(chunk.word, bit)); }
			}
			result.aut.delta.mutable_state_post(class_id).push_back(
				SymbolPost(symbol_post.symbol, std::span<const State>(target_classes))
			);
		}
	}
	for (const State q : compressed.initial) { result.aut.initial.insert(class_of_state(q)); }
	for (const State q : compressed.final) { result.aut.final.insert(class_of_state(q)); }
	return result;
}

/// Determinize @p reduced, or give up after more than @p max_macrostates macrostates.
std::optional<Nfa> determinize_reduced_automaton(
	ReducedAutomaton& reduced,
	const FourRussiansOptions& options,
	SubsetMap* const subset_map,
	const size_t max_macrostates = std::numeric_limits<size_t>::max()
) {
	// The construction writes the transitions for all symbols of a class.
	SubsetMap reduced_map;
	std::optional<Nfa> result = determinize_pruned(
		reduced.aut, std::move(reduced.classes), reduced.pruner, options,
		subset_map == nullptr ? nullptr : &reduced_map, max_macrostates
	);
	if (result.has_value() && subset_map != nullptr) {
		std::vector<State> states;
		// Map the macrostates back to the original states.
		for (const auto& [macrostate, state] : reduced_map) {
			states.clear();
			for (const State q : macrostate) { states.push_back(reduced.original[q]); } // (Increasing.)
			(*subset_map)[StateSet(states)] = state;
		}
	}
	return result;
}

/// Scales the estimated cost of the simulation to macrostates (calibrated on nfa-bench).
constexpr double ADAPTIVE_BUDGET_FACTOR = 0.1;
constexpr size_t ADAPTIVE_MIN_BUDGET = 1'000;
/// Keeps the budget in the range of size_t when it has 32 bits (more macrostates than fit in memory there).
constexpr double ADAPTIVE_MAX_BUDGET = 1e9;

/// determinize_four_russians() with a reduction.
Nfa determinize_reduced(
	const Nfa& aut, const FourRussiansOptions& options, SubsetMap* const subset_map, const MacrostateDiscover& discover
) {
	if (discover.has_value()) {
		throw std::invalid_argument("determinize_four_russians: a reduction cannot have a discovery callback");
	}
	if (options.reduction != FourRussiansReduction::Adaptive) {
		ReducedAutomaton reduced = reduce_for_determinization(aut, options.reduction);
		return *determinize_reduced_automaton(reduced, options, subset_map);
	}
	// Like ski rental: use the cheap reduction until it has cost about as much as the simulation.
	ReducedAutomaton cheap = reduce_for_determinization(aut, FourRussiansReduction::Bisimulation);
	// The budget in macrostates: about what the simulation would cost.
	const double num_of_states = static_cast<double>(cheap.aut.num_of_states());
	const double num_of_transitions = static_cast<double>(cheap.aut.delta.num_of_transitions());
	const double simulation_cost = num_of_transitions * std::sqrt(num_of_transitions) + num_of_states * num_of_states;
	// At least 1: the reduction of an empty language has no states and no symbol classes.
	const double macrostate_cost =
		std::max(static_cast<double>(cheap.classes.num_of_classes() + words_for(cheap.aut.num_of_states())), 1.0);
	const double scaled_cost =
		std::min(ADAPTIVE_BUDGET_FACTOR * simulation_cost / macrostate_cost, ADAPTIVE_MAX_BUDGET);
	const size_t budget = static_cast<size_t>(scaled_cost) + ADAPTIVE_MIN_BUDGET;
	if (std::optional<Nfa> result = determinize_reduced_automaton(cheap, options, subset_map, budget)) {
		return std::move(*result);
	}
	ReducedAutomaton reduced = reduce_for_determinization(aut, FourRussiansReduction::Simulation);
	return *determinize_reduced_automaton(reduced, options, subset_map);
}

} // namespace

Nfa mata::nfa::algorithms::determinize_four_russians(
	const Nfa& aut,
	const FourRussiansOptions& options,
	std::unordered_map<StateSet, State>* subset_map,
	std::optional<std::function<bool(const Nfa&, State, const StateSet&)>> macrostate_discover,
	const mata::utils::OrdVector<Symbol>* const restrict_to_symbols
) {
	const auto is_other = [&](const SymbolPost& symbol_post) {
		return !restrict_to_symbols->contains(symbol_post.symbol);
	};
	const auto has_other = [&](const StatePost& state_post) {
		return std::any_of(state_post.begin(), state_post.end(), is_other);
	};
	if (restrict_to_symbols != nullptr && std::any_of(aut.delta.begin(), aut.delta.end(), has_other)) {
		// Determinize a copy without the transitions over the other symbols.
		Nfa restricted = aut;
		for (State state = 0; state < restricted.delta.num_of_states(); ++state) {
			StatePost& state_post = restricted.delta.mutable_state_post(state);
			state_post.erase(std::remove_if(state_post.begin(), state_post.end(), is_other), state_post.end());
		}
		return determinize_four_russians(restricted, options, subset_map, std::move(macrostate_discover));
	}
	if (options.reduction != FourRussiansReduction::None) {
		return determinize_reduced(aut, options, subset_map, macrostate_discover);
	}
	if (!macrostate_discover.has_value() && aut.is_deterministic()) {
		checked_block_size(options.block_size);
		return copy_deterministic(aut, subset_map);
	}
	return FourRussiansDeterminizer{aut, options, macrostate_discover}.run(subset_map);
}
