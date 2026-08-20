/* alphabet.hh -- File containing alphabets for automata.
 */

#ifndef MATA_ALPHABET_HH
#define MATA_ALPHABET_HH

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "utils/ord-vector.hh"
#include "utils/utils.hh"

namespace mata {

using Symbol = unsigned;
using Level = unsigned;
using Word = std::vector<Symbol>;
using WordName = std::vector<std::string>;

/**
 * The abstract interface for NFA alphabets.
 */
class Alphabet {
  public:
	/// translates a string into a symbol
	virtual Symbol translate_symb(const std::string& symb) = 0;

	/**
	 * Translate sequence of symbol names to sequence of their respective values.
	 */
	virtual Word translate_word(const WordName& word_name) const {
		(void) word_name;
		throw std::runtime_error("Unimplemented");
	}

	/**
	 * @brief Translate internal @p symbol representation back to its original string name.
	 *
	 * Throws an exception when the @p symbol is missing in the alphabet.
	 * @param[in] symbol Symbol to translate.
	 * @return @p symbol original name.
	 */
	virtual std::string reverse_translate_symbol(Symbol symbol) const = 0;

	/// also translates strings to symbols
	Symbol operator[](const std::string& symb) { return this->translate_symb(symb); }

	/**
	 * @brief Get a set of all symbols in the alphabet.
	 *
	 * The result does not have to equal the list of symbols in the automaton using this alphabet.
	 */
	virtual utils::OrdVector<Symbol> get_alphabet_symbols() const { throw std::runtime_error("Unimplemented"); }

	/// complement of a set of symbols wrt the alphabet
	virtual utils::OrdVector<Symbol> get_complement(const utils::OrdVector<Symbol>& symbols) const { // {{{
		(void) symbols;
		throw std::runtime_error("Unimplemented");
	} // }}}

	virtual ~Alphabet() = default;

	/**
	 * @brief Check whether two alphabets are equal.
	 *
	 * In general, two alphabets are equal if and only if they are of the same class instance.
	 * @param other_alphabet The other alphabet to compare with for equality.
	 * @return True if equal, false otherwise.
	 */
	virtual bool is_equal(const Alphabet& other_alphabet) const { return address() == other_alphabet.address(); }

	/**
	 * @brief Check whether two alphabets are equal.
	 *
	 * In general, two alphabets are equal if and only if they are of the same class instance.
	 * @param other_alphabet The other alphabet to compare with for equality.
	 * @return True if equal, false otherwise.
	 */
	virtual bool is_equal(const Alphabet* const other_alphabet) const { return address() == other_alphabet->address(); }

	bool operator==(const Alphabet&) const = delete;

	/**
	 * Checks whether the alphabet has any symbols.
	 */
	virtual bool empty() const = 0;

	virtual void clear() { throw std::runtime_error("Unimplemented"); }

  protected:
	virtual const void* address() const { return this; }
}; // class Alphabet.

/**
 * Direct alphabet (also identity alphabet or integer alphabet) using integers as symbols.
 *
 * This alphabet presumes that all integers are valid symbols.
 * Therefore, calling member functions get_complement() and get_alphabet_symbols() makes no sense in this context and
 * the methods will throw exceptions warning about the inappropriate use of IntAlphabet. If one needs these functions,
 * they should use OnTheFlyAlphabet instead of IntAlphabet.
 */
class IntAlphabet : public Alphabet {
  public:
	IntAlphabet() : alphabet_instance_(IntAlphabetSingleton::get()) {}

	Symbol translate_symb(const std::string& symb) override;

	std::string reverse_translate_symbol(const Symbol symbol) const override { return std::to_string(symbol); }

	utils::OrdVector<Symbol> get_alphabet_symbols() const override {
		throw std::runtime_error("Nonsensical use of get_alphabet_symbols() on IntAlphabet.");
	}

	utils::OrdVector<Symbol> get_complement(const utils::OrdVector<Symbol>& symbols) const override {
		(void) symbols;
		throw std::runtime_error("Nonsensical use of get_complement() on IntAlphabet.");
	}

	IntAlphabet(const IntAlphabet&) = default;

	IntAlphabet& operator=(const IntAlphabet& int_alphabet) = delete;

	bool empty() const override { return false; }

	void clear() override { throw std::runtime_error("Nonsensical use of clear() on IntAlphabet."); }

  protected:
	const void* address() const override { return &alphabet_instance_; }

  private:
	/**
	 * Singleton class implementing integer alphabet_instance for class IntAlphabet.
	 *
	 * Users have to use IntAlphabet instead which provides interface identical to other alphabets and can be used in
	 *  places where an instance of the abstract class Alphabet is required.
	 */
	class IntAlphabetSingleton {
	  public:
		static IntAlphabetSingleton& get() {
			static IntAlphabetSingleton alphabet;
			return alphabet;
		}

		IntAlphabetSingleton(IntAlphabetSingleton&) = delete;
		IntAlphabetSingleton(IntAlphabetSingleton&&) = delete;
		IntAlphabetSingleton& operator=(const IntAlphabetSingleton&) = delete;
		IntAlphabetSingleton& operator=(IntAlphabetSingleton&&) = delete;

		~IntAlphabetSingleton() = default;

	  protected:
		IntAlphabetSingleton() = default;
	}; // class IntAlphabetSingleton.

	IntAlphabetSingleton& alphabet_instance_;
}; // class IntAlphabet.

/**
 * Enumerated alphabet using a set of integers as symbols maintaining a set of specified symbols.
 *
 * @c EnumAlphabet is a version of direct (identity) alphabet (does not give names to symbols, their name is their
 *  integer value directly). However, unlike @c IntAlphabet, @c EnumAlphabet maintains an ordered set of symbols in
 *  the alphabet.
 *
 * Therefore, calling member functions @c get_complement() and @c get_alphabet_symbols() makes sense in the context
 *  of @c EnumAlphabet and the functions give the expected results.
 *
 *  Example:
 *  @code
 *  Alphabet alphabet{ EnumAlphabet{ 0, 4, 6, 8, 9 } };
 *  CHECK(alphabet.translate_symb("6") == 6);
 *  CHECK_THROWS(alphabet.translate_symb("5")); // Throws an exception about an unknown symbol.
 *  CHECK(alphabet.get_complement({ utils::OrdVector<Symbol>{ 0, 6, 9 } }) == utils::OrdVector<Symbol>{ 4, 8 });
 *  @endcode
 */
class EnumAlphabet : public Alphabet {
  public:
	explicit EnumAlphabet() = default;
	EnumAlphabet(const EnumAlphabet& alphabet) = default;
	explicit EnumAlphabet(const EnumAlphabet* const alphabet) : EnumAlphabet(*alphabet) {}
	EnumAlphabet(EnumAlphabet&& rhs) = default;
	EnumAlphabet(utils::OrdVector<Symbol> symbols) : symbols_(std::move(symbols)) {}

	utils::OrdVector<Symbol> get_alphabet_symbols() const override { return symbols_; }
	utils::OrdVector<Symbol> get_complement(const utils::OrdVector<Symbol>& symbols) const override {
		return symbols_.difference(symbols);
	}

	std::string reverse_translate_symbol(Symbol symbol) const override;

	EnumAlphabet& operator=(const EnumAlphabet& rhs) = default;
	EnumAlphabet& operator=(EnumAlphabet&& rhs) = default;

	/**
	 * @brief Expand alphabet by symbols from the passed @p symbols.
	 *
	 * Adding a symbol name which already exists will throw an exception.
	 * @param[in] symbols Vector of symbols to add.
	 */
	void add_symbols_from(const mata::utils::OrdVector<Symbol>& symbols) { symbols_.insert(symbols); }

	/**
	 * @brief Expand alphabet by symbols from the passed @p alphabet.
	 *
	 * @param[in] symbols_to_add Vector of symbols to add.
	 */
	void add_symbols_from(const EnumAlphabet& alphabet) { symbols_.insert(alphabet.get_alphabet_symbols()); }

	EnumAlphabet(const std::initializer_list<Symbol> symbols) : EnumAlphabet(symbols.begin(), symbols.end()) {}
	template <class InputIt> EnumAlphabet(InputIt first, InputIt last) : EnumAlphabet() {
		for (; first != last; ++first) { add_new_symbol(*first); }
	}
	EnumAlphabet(const std::initializer_list<std::string> l) : EnumAlphabet(l.begin(), l.end()) {}

	Symbol translate_symb(const std::string& str) override;
	Word translate_word(const WordName& word_name) const override;

	/**
	 * @brief Add new symbol to the alphabet with the value identical to its string representation.
	 *
	 * @param[in] symbol User-space representation of the symbol.
	 * @return Result of the insertion as @c InsertionResult.
	 */
	void add_new_symbol(const std::string& symbol);

	/**
	 * @brief Add new symbol to the alphabet.
	 *
	 * @param[in] key User-space representation of the symbol.
	 * @param[in] symbol Number of the symbol to be used on transitions.
	 * @return Result of the insertion as @c InsertionResult.
	 */
	void add_new_symbol(Symbol symbol);

	/**
	 * Get the next value for a potential new symbol.
	 * @return Next Symbol value.
	 */
	Symbol get_next_value() const { return next_symbol_value_; }

	/**
	 * Get the number of existing symbols, epsilon symbols excluded.
	 * @return The number of symbols.
	 */
	size_t get_number_of_symbols() const { return symbols_.size(); }

	bool empty() const override { return symbols_.empty(); }

  private:
	mata::utils::OrdVector<Symbol> symbols_{}; ///< Map of string transition symbols to symbol values.
	Symbol next_symbol_value_{0}; ///< Next value to be used for a newly added symbol.

  public:
	/**
	 * @brief Update next symbol value when appropriate.
	 *
	 * When the newly inserted value is larger or equal to the current next symbol value, update the next symbol
	 *  value to a value one larger than the new value.
	 * @param value The value of the newly added symbol.
	 */
	void update_next_symbol_value(Symbol value);

	/**
	 * @brief Erase a symbol from the alphabet.
	 * @return Number of symbols erased (0 or 1).
	 */
	size_t erase(Symbol symbol);

	/**
	 * @brief Remove a symbol name value pair from the position @p pos from the alphabet.
	 * @return Iterator following the last removed element.
	 */
	void erase(const utils::OrdVector<Symbol>::const_iterator pos) { symbols_.erase(pos); }

	/**
	 * @brief Remove a symbol name value pair from the positions between @p first and @p last from the alphabet.
	 * @return Iterator following the last removed element.
	 */
	void erase(
		const utils::OrdVector<Symbol>::const_iterator first, const utils::OrdVector<Symbol>::const_iterator last
	) {
		symbols_.erase(first, last);
	}

	void clear() override {
		symbols_.clear();
		next_symbol_value_ = 0;
	}
}; // class EnumAlphabet.

/**
 * An alphabet constructed 'on the fly'.
 * Should be use anytime the automata have a specific names for the symbols.
 */
class OnTheFlyAlphabet : public Alphabet {
  public:
	using StringToSymbolMap = std::unordered_map<std::string, Symbol>;

	/// Result of the insertion of a new symbol.
	using InsertionResult = std::pair<StringToSymbolMap::const_iterator, bool>;

	explicit OnTheFlyAlphabet(const Symbol init_symbol = 0) : next_symbol_value_(init_symbol) {};
	OnTheFlyAlphabet(const OnTheFlyAlphabet& alphabet) = default;
	OnTheFlyAlphabet(OnTheFlyAlphabet&& alphabet) = default;
	explicit OnTheFlyAlphabet(const OnTheFlyAlphabet* const alphabet) : OnTheFlyAlphabet(*alphabet) {}
	explicit OnTheFlyAlphabet(StringToSymbolMap str_sym_map) : symbol_map_(std::move(str_sym_map)) {}

	/**
	 * Create alphabet from a list of symbol names.
	 * @param symbol_names Names for symbols on transitions.
	 * @param init_symbol Start of a sequence of values to use for new symbols.
	 */
	explicit OnTheFlyAlphabet(const std::vector<std::string>& symbol_names, const Symbol init_symbol = 0)
		: symbol_map_(),
		  next_symbol_value_(init_symbol) {
		add_symbols_from(symbol_names);
	}

	template <class InputIt> OnTheFlyAlphabet(InputIt first, InputIt last) {
		for (; first != last; ++first) { add_new_symbol(*first, next_symbol_value_); }
	}
	OnTheFlyAlphabet(std::initializer_list<std::pair<std::string, Symbol>> name_symbol_map) : symbol_map_{} {
		for (auto&& [name, symbol] : name_symbol_map) { add_new_symbol(name, symbol); }
	}

	utils::OrdVector<Symbol> get_alphabet_symbols() const override;
	utils::OrdVector<Symbol> get_complement(const utils::OrdVector<Symbol>& symbols) const override;

	std::string reverse_translate_symbol(Symbol symbol) const override;

  public:
	OnTheFlyAlphabet& operator=(const OnTheFlyAlphabet& rhs) = default;
	OnTheFlyAlphabet& operator=(OnTheFlyAlphabet&& rhs) = default;

	/**
	 * @brief Expand alphabet by symbols from the passed @p symbol_names.
	 *
	 * Adding a symbol name which already exists will throw an exception.
	 * @param[in] symbol_names Vector of symbol names.
	 */
	void add_symbols_from(const std::vector<std::string>& symbol_names);

	/**
	 * @brief Expand alphabet by symbols from the passed @p symbol_map.
	 *
	 * The value of the already existing symbols will NOT be overwritten.
	 * @param[in] new_symbol_map Map of strings to symbols.
	 */
	void add_symbols_from(const StringToSymbolMap& new_symbol_map);

	Symbol translate_symb(const std::string& str) override;

	Word translate_word(const WordName& word_name) const override;

	/**
	 * @brief Add new symbol to the alphabet with the value of @c next_symbol_value.
	 *
	 * Throws an exception when the adding fails.
	 *
	 * @param[in] key User-space representation of the symbol.
	 * @return Result of the insertion as @c InsertionResult.
	 */
	InsertionResult add_new_symbol(const std::string& key);

	/**
	 * @brief Add new symbol to the alphabet.
	 *
	 * Throws an exception when the adding fails.
	 *
	 * @param[in] key User-space representation of the symbol.
	 * @param[in] value Number of the symbol to be used on transitions.
	 * @return Result of the insertion as @c InsertionResult.
	 */
	InsertionResult add_new_symbol(const std::string& key, Symbol value);

	/**
	 * @brief Try to add symbol to the alphabet map.
	 *
	 * Does not throw an exception when the adding fails.
	 *
	 * @param[in] key User-space representation of the symbol.
	 * @param[in] value Number of the symbol to be used on transitions.
	 * @return Result of the insertion as @c InsertionResult.
	 */
	InsertionResult try_add_new_symbol(const std::string& key, Symbol value) {
		return symbol_map_.insert({key, value});
	}

	/**
	 * Get the next value for a potential new symbol.
	 * @return Next Symbol value.
	 */
	Symbol get_next_value() const { return next_symbol_value_; }

	/**
	 * Get the number of existing symbols, epsilon symbols excluded.
	 * @return The number of symbols.
	 */
	size_t get_number_of_symbols() const { return next_symbol_value_; }

	/**
	 * Get the symbol map used in the alphabet.
	 * @return Map mapping strings to symbols used internally in Mata.
	 */
	const StringToSymbolMap& get_symbol_map() const { return symbol_map_; }

	bool empty() const override { return symbol_map_.empty(); }

  private:
	StringToSymbolMap symbol_map_{}; ///< Map of string transition symbols to symbol values.
	Symbol next_symbol_value_{}; ///< Next value to be used for a newly added symbol.

  public:
	/**
	 * @brief Update next symbol value when appropriate.
	 *
	 * When the newly inserted value is larger or equal to the current next symbol value, update the next symbol
	 *  value to a value one larger than the new value.
	 * @param value The value of the newly added symbol.
	 */
	void update_next_symbol_value(Symbol value);

	/**
	 * @brief Remove a symbol name value pair specified by its @p symbol from the alphabet.
	 *
	 * @warning Complexity: O(n), where n is the number of symbols in the alphabet.
	 * @return Number of symbols removed (0 or 1).
	 */
	size_t erase(Symbol symbol);

	/**
	 * @brief Remove a symbol name value pair specified by its @p symbol_name from the alphabet.
	 * @return Number of symbols removed (0 or 1).
	 */
	size_t erase(const std::string& symbol_name);

	/**
	 * @brief Remove a symbol name value pair from the position @p pos from the alphabet.
	 */
	void erase(const StringToSymbolMap::const_iterator pos) { symbol_map_.erase(pos); }

	/**
	 * @brief Remove a symbol name value pair from the positions between @p first and @p last from the alphabet.
	 */
	void erase(const StringToSymbolMap::const_iterator first, const StringToSymbolMap::const_iterator last) {
		symbol_map_.erase(first, last);
	}

	void clear() override {
		symbol_map_.clear();
		next_symbol_value_ = 0;
	}
}; // class OnTheFlyAlphabet.

/**
 * @brief Per-level alphabets for transducer-like automata.
 *
 * Standalone (does NOT inherit from @c Alphabet) wrapper holding a vector of @c std::shared_ptr<Alphabet>, one per
 * level.
 * Operates in one of two modes (see @c Mode):
 *  - @c Global   — a single shared alphabet (typically stored in @c alphabets[0]) applies to every level. The level
 *                  argument is ignored.
 *  - @c MultiLevel — distinct per-level alphabets. The level argument is required and indexes @c alphabets.
 *
 * The underlying @c Alphabet objects are held via @c std::shared_ptr and can be shared between multiple
 *  @c AlphabetLevels instances (and other automata).
 */
class AlphabetLevels {
  public:
	/// Operating mode of the @c AlphabetLevels instance.
	enum class Mode {
		Global, ///< @c alphabets[0] is used for every level; level argument is ignored.
		MultiLevel, ///< Each level uses its own alphabet from @c alphabets[level]; level argument is required.
	};

	explicit AlphabetLevels(std::vector<std::shared_ptr<Alphabet>> alphabets = {}, Mode mode = Mode::MultiLevel)
		: alphabets_{std::move(alphabets)},
		  mode_{mode} {
		check_can_grow_to_(alphabets_.size());
	}

	/**
	 * @brief Use the same @p alphabet for every level (Global mode).
	 *
	 * Stores a single-element vector and sets @c mode to @c Global.
	 */
	explicit AlphabetLevels(std::shared_ptr<Alphabet> alphabet)
		: alphabets_{std::move(alphabet)},
		  mode_{Mode::Global} {}

	/**
	 * @brief Construct in @c Global mode, using @p alphabet as the single shared alphabet for every level.
	 *
	 * @see set_global_mode(std::shared_ptr<Alphabet>)
	 */
	static AlphabetLevels global_mode(std::shared_ptr<Alphabet> alphabet) {
		return AlphabetLevels{std::move(alphabet)};
	}

	/**
	 * @brief Construct in @c MultiLevel mode, using @p alphabets indexed by level.
	 *
	 * @see set_multi_level_mode()
	 */
	static AlphabetLevels multi_level_mode(std::vector<std::shared_ptr<Alphabet>> alphabets = {}) {
		return AlphabetLevels{std::move(alphabets), Mode::MultiLevel};
	}

	AlphabetLevels(const AlphabetLevels& other) = default;
	AlphabetLevels(AlphabetLevels&& other) noexcept = default;
	AlphabetLevels& operator=(const AlphabetLevels& other) = default;
	AlphabetLevels& operator=(AlphabetLevels&& other) noexcept = default;

	/**
	 * @brief Equality of two @c AlphabetLevels instances.
	 *
	 * Memberwise comparison of @c mode and the stored alphabet slots.
	 * Slots are @c std::shared_ptr<Alphabet> and are compared by the pointee's identity (i.e., whether both slots point
	 *  to the same underlying @c Alphabet object), not by the alphabets' contents, since @c Alphabet does not support
	 *  value equality.
	 */
	bool operator==(const AlphabetLevels& other) const = default;

	/// Current operating mode.
	Mode mode() const noexcept { return mode_; }

	/**
	 * @brief Switch to @c Global mode, keeping only the alphabet at @p level_for_kept_alphabet as the single
	 *  shared alphabet.
	 *
	 * Any other stored alphabets are dropped from @c AlphabetLevels (the @c Alphabet objects themselves survive
	 *  if still referenced elsewhere, since they are held via @c shared_ptr). Unlike @c for_level, this never
	 *  throws due to a missing alphabet: switching an empty instance to @c Global just leaves it empty.
	 *
	 * @param[in] level_for_kept_alphabet Level whose alphabet to keep. Ignored when no alphabet is currently
	 *  stored.
	 * @throws std::out_of_range If @p level_for_kept_alphabet is out of range and at least one alphabet is
	 *  stored.
	 */
	void set_global_mode(Level level_for_kept_alphabet = 0) {
		if (!alphabets_.empty()) { alphabets_ = {alphabets_.at(level_for_kept_alphabet)}; }
		mode_ = Mode::Global;
	}

	/**
	 * @brief Switch to @c Global mode, using @p alphabet as the new single shared alphabet.
	 *
	 * Replaces any previously stored alphabets regardless of how many there were. @c AlphabetLevels takes shared
	 *  ownership of @p alphabet (via @c std::shared_ptr); it may still be shared with other owners.
	 *
	 * @param[in] alphabet The alphabet to use for every level.
	 */
	void set_global_mode(std::shared_ptr<Alphabet> alphabet) {
		alphabets_ = {std::move(alphabet)};
		mode_ = Mode::Global;
	}

	/**
	 * @brief Switch to @c MultiLevel mode.
	 *
	 * Always safe: @c MultiLevel places no constraint on the number of stored alphabets. Existing alphabets are
	 *  kept as-is, now indexed by level (e.g., a single alphabet from @c Global mode becomes level 0's alphabet).
	 */
	void set_multi_level_mode() noexcept { mode_ = Mode::MultiLevel; }

	/**
	 * @brief Switch to @c MultiLevel mode, using @p alphabets as the new per-level alphabets.
	 *
	 * Replaces any previously stored alphabets regardless of how many there were.
	 *
	 * @param[in] alphabets The alphabets to use, indexed by level.
	 */
	void set_multi_level_mode(std::vector<std::shared_ptr<Alphabet>> alphabets) noexcept {
		alphabets_ = std::move(alphabets);
		mode_ = Mode::MultiLevel;
	}

	/**
	 * @brief Translate a symbol name using the alphabet for the given level.
	 *
	 * When @p level is @c std::nullopt and the instance is in @c Global mode, the shared alphabet is used.
	 * In @c MultiLevel mode @p level must have a value.
	 */
	Symbol translate_symb(const std::string& symb, std::optional<Level> level = std::nullopt);

	/**
	 * @brief Translate a symbol value back to its name using the alphabet for the given level.
	 *
	 * Same level-resolution semantics as @c translate_symb.
	 */
	std::string reverse_translate_symbol(Symbol symbol, std::optional<Level> level = std::nullopt) const;

	/**
	 * @brief Get the symbols known to the alphabet on the given level.
	 */
	utils::OrdVector<Symbol> get_alphabet_symbols(std::optional<Level> level = std::nullopt) const;

	/**
	 * @brief Get the complement of @p symbols with respect to the alphabet for the given level.
	 */
	utils::OrdVector<Symbol>
		get_complement(const utils::OrdVector<Symbol>& symbols, std::optional<Level> level = std::nullopt) const;

	/**
	 * @brief Check whether the alphabet for the given level is empty.
	 *
	 * In @c Global mode, always checks @c alphabets[0] (level argument is ignored).
	 * In @c MultiLevel mode, checks @c alphabets[level] when @p level has a value, or returns @c true iff every
	 *  underlying alphabet is empty when @p level is @c std::nullopt.
	 */
	bool empty(std::optional<Level> level = std::nullopt) const;

	/**
	 * @brief Clear the alphabet for the given level.
	 *
	 * In @c Global mode, always clears @c alphabets[0] (level argument is ignored, calling @c clear repeatedly is
	 *  idempotent on the same alphabet).
	 * In @c MultiLevel mode, clears @c alphabets[level] when @p level has a value, or clears every underlying
	 *  alphabet when @p level is @c std::nullopt.
	 */
	void clear(std::optional<Level> level = std::nullopt);

	/**
	 * @brief Get the alphabet assigned to a specific level.
	 *
	 * Validated accessor for the internal vector of @c Alphabet*.
	 *
	 * - In @c Global mode, @c alphabets[0] is returned regardless of @p level.
	 * - In @c MultiLevel mode, @p level must have a value and be a valid index; the corresponding entry must be
	 *   non-null.
	 *
	 * @param[in] level Level whose alphabet should be returned (required in @c MultiLevel mode).
	 * @return Alphabet assigned to @p level.
	 * @throws std::runtime_error If @p level is missing in @c MultiLevel mode, out of range, or the entry is null.
	 */
	const Alphabet& for_level(std::optional<Level> level = std::nullopt) const;
	Alphabet& for_level(std::optional<Level> level = std::nullopt);

	/**
	 * @brief Alias for @c for_level.
	 *
	 * @see @c for_level.
	 */
	const Alphabet& operator[](std::optional<Level> level) const { return for_level(level); }
	/**
	 * @brief Alias for @c for_level.
	 *
	 * @see @c for_level.
	 */
	Alphabet& operator[](std::optional<Level> level) { return for_level(level); }

	/**
	 * @name Raw slot access (mode-agnostic).
	 *
	 * Bounds-checked access to a raw slot by its index in the underlying vector, ignoring @c mode. Unlike
	 *  @c for_level, a null entry is returned as-is instead of throwing; only an out-of-range @p index throws.
	 *  @see std::vector::at.
	 */
	///@{
	const std::shared_ptr<Alphabet>& at(size_t index) const { return alphabets_.at(index); }
	std::shared_ptr<Alphabet>& at(size_t index) { return alphabets_.at(index); }
	///@}

	/// Number of alphabet slots currently stored. @see std::vector::size.
	size_t size() const noexcept { return alphabets_.size(); }

	std::vector<std::shared_ptr<Alphabet>>::const_iterator begin() const noexcept { return alphabets_.begin(); }
	std::vector<std::shared_ptr<Alphabet>>::const_iterator end() const noexcept { return alphabets_.end(); }
	std::vector<std::shared_ptr<Alphabet>>::iterator begin() noexcept { return alphabets_.begin(); }
	std::vector<std::shared_ptr<Alphabet>>::iterator end() noexcept { return alphabets_.end(); }

	/// Append an alphabet as the new last level. @see std::vector::push_back.
	void push_back(std::shared_ptr<Alphabet> alphabet) {
		check_can_grow_to_(alphabets_.size() + 1);
		alphabets_.push_back(std::move(alphabet));
	}

	/// Insert an alphabet before @p pos, shifting subsequent levels up. @see std::vector::insert.
	std::vector<std::shared_ptr<Alphabet>>::iterator
		insert(std::vector<std::shared_ptr<Alphabet>>::const_iterator pos, std::shared_ptr<Alphabet> alphabet) {
		check_can_grow_to_(alphabets_.size() + 1);
		return alphabets_.insert(pos, std::move(alphabet));
	}

	/// Remove the last level's alphabet. @see std::vector::pop_back.
	void pop_back() { alphabets_.pop_back(); }

	/// Reserve storage for at least @p new_cap levels. @see std::vector::reserve.
	void reserve(size_t new_cap) { alphabets_.reserve(new_cap); }

	/// Resize the number of levels, value-initializing (null) any newly added slots. @see std::vector::resize.
	void resize(size_t count) {
		check_can_grow_to_(count);
		alphabets_.resize(count);
	}

	/// Remove the alphabet at @p pos. @see std::vector::erase.
	std::vector<std::shared_ptr<Alphabet>>::iterator erase(std::vector<std::shared_ptr<Alphabet>>::const_iterator pos) {
		return alphabets_.erase(pos);
	}

	/// Remove alphabets in the range [@p first, @p last). @see std::vector::erase.
	std::vector<std::shared_ptr<Alphabet>>::iterator erase(
		std::vector<std::shared_ptr<Alphabet>>::const_iterator first,
		std::vector<std::shared_ptr<Alphabet>>::const_iterator last
	) {
		return alphabets_.erase(first, last);
	}

  public:
	/**
	 * @brief Per-level alphabets.
	 *
	 * In @c Global mode, only @c alphabets[0] is used. In @c MultiLevel mode, indexed by level. Private so that the
	 *  @c Global invariant (exactly one alphabet) can be enforced by the build-up methods below; use @c for_level
	 *  (or @c operator[]) for validated read/write access, @c at for raw slot access, or the vector-like methods to
	 *  build up a @c MultiLevel instance.
	 * @warning Modifying directly may break the class invariants.
	 */
	std::vector<std::shared_ptr<Alphabet>> alphabets_{};

	/**
	 * @brief Operating mode. Defaults to @c MultiLevel.
	 *
	 * Private so switching to @c Global can be rejected while more than one alphabet is stored (see @c
	 * set_global_mode).
	 * @warning Modifying directly may break the class invariants.
	 */
	Mode mode_{Mode::MultiLevel};

  private:
	/// Throw if growing to @p new_size would violate the @c Global invariant of holding a single alphabet.
	void check_can_grow_to_(size_t new_size) const {
		if (mode_ == Mode::Global && new_size > 1) {
			throw std::runtime_error(
				"AlphabetLevels (Global) can only hold a single alphabet; use set_global_mode()/for_level()/"
				"operator[] to replace it."
			);
		}
	}
};

/**
 * @brief Encode a word using UTF-8 encoding.
 *
 * @param[in] word The word to encode.
 * @return The UTF-8 encoded word.
 */
Word encode_word_utf8(const Word& word);

/**
 * @brief Decode a word using UTF-8 encoding.
 *
 * @param[in] word The word to decode.
 * @return The UTF-8 decoded word.
 */
Word decode_word_utf8(const Word& word);

} // namespace mata

namespace std { // {{{
std::ostream& operator<<(std::ostream& os, const mata::Alphabet& alphabet);
}
#endif // MATA_ALPHABET_HH
