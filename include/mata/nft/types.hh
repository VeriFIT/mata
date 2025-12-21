/** @file
 * @brief Basic types used in the @c mata::nft module for NFTs.
 */

#ifndef MATA_NFT_TYPES_HH
#define MATA_NFT_TYPES_HH

#include <optional>
#include <utility>

#include "mata/alphabet.hh"
#include "mata/nfa/types.hh"
#include "mata/utils/sparse-set.hh"

namespace mata::nft {

extern const std::string TYPE_NFT;

using Level = unsigned;

using State = mata::nfa::State;
using StateSet = mata::nfa::StateSet;

using Run = mata::nfa::Run;
using EpsilonClosureOpt = mata::nfa::EpsilonClosureOpt;

using StateRenaming = mata::nfa::StateRenaming;

/**
 * @brief Map of additional parameter name and value pairs.
 *
 * Used by certain functions for specifying some additional parameters in the following format:
 * ```cpp
 * ParameterMap {
 *     { "algorithm", "classical" },
 *     { "minimize", "true" }
 * }
 * ```
 */
using ParameterMap = mata::nfa::ParameterMap;

using Limits = mata::nfa::Limits;

class Nft; ///< A non-deterministic finite transducer.

enum class JumpMode {
    RepeatSymbol, ///< Repeat the symbol on the jump.
    AppendDontCares ///< Append a sequence of DONT_CAREs to the symbol on the jump.
};

using ProductFinalStateCondition = mata::nfa::ProductFinalStateCondition;

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = mata::nfa::EPSILON;
constexpr Symbol DONT_CARE = EPSILON - 1;

constexpr Level DEFAULT_LEVEL{ 0 };
constexpr Level DEFAULT_NUM_OF_LEVELS{ 2 };

class Levels: std::vector<Level> {
    using super = std::vector<Level>;
public:
    /// @brief Orderings of levels.
    class Ordering {
    public:
        using Compare = std::function<bool(Level, Level)>;
        /**
         * @brief Ordering for Levels in NFTs where lower levels precede higher levels.
         *
         * That is, levels are ordered as follows: 0 < 1 < 2 < ... < num_of_levels-1.
         */
        static bool Minimal(const Level lhs, const Level rhs) { return std::less()(lhs, rhs); }
        /**
         * @brief Ordering for levels in NFTs where lower levels precede higher levels, except for 0 which is the highest level.
         *
         * That is, levels are ordered as follows: 1 < 2 < ... < num_of_levels-1 < 0.
         * This ordering is used when handling intermediate states (with non-zero levels) in NFTs for determining the next lowest
         *  level of the next state.
         */
        static bool Next(const Level lhs, const Level rhs) { return lhs == 0 ? false : rhs == 0 ? true : lhs < rhs; }
    };

    /**
     * @brief Number of levels (tracks) the transducer recognizes. Each transducer transition will comprise
     *  @c num_of_levels of NFA transitions.
     *
     * @note The number of levels has to be at least 1.
     */
    size_t num_of_levels{ DEFAULT_NUM_OF_LEVELS };

    // explicit Levels(const std::vector<Level>& levels): super{ levels }, num_of_levels{ num_of_levels_of(levels).value_or(DEFAULT_NUM_OF_LEVELS) } {}
    // explicit Levels(std::vector<Level>&& levels): super{ std::move(levels) } { num_of_levels = num_of_levels_of(*this).value_or(DEFAULT_NUM_OF_LEVELS); }
    explicit Levels(const size_t num_of_levels, std::vector<Level> levels = {}): super{ std::move(levels) }, num_of_levels{ num_of_levels } {}
    Levels() = default;
    Levels(size_t num_of_levels, size_t count, Level value = DEFAULT_LEVEL);
    Levels(size_t num_of_levels, std::initializer_list<Level> levels);
    Levels(size_t num_of_levels, iterator first, iterator last);

    Levels(const Levels& other) = default;
    Levels(Levels&& other) = default;
    Levels& operator=(const Levels& other) = default;
    Levels& operator=(Levels&& other) = default;
    /**
     * @brief Assign levels from @p other to @c this.
     *
     * After assignment, @c num_of_levels is set to the minimal number of levels that can accommodate all levels in @p other.
     *
     * @param[in] other Levels to be assigned.
     */
    Levels& operator=(std::initializer_list<value_type> other);
    /**
     * @brief Assign levels from @p levels to @c this.
     *
     * After assignment, @c num_of_levels is set to the minimal number of levels that can accommodate all levels in @p levels.
     *
     * @param[in] levels Levels to be assigned.
     */
    Levels& operator=(const std::vector<Level>& levels);
    /**
     * @brief Assign levels from @p levels to @c this.
     *
     * After assignment, @c num_of_levels is set to the minimal number of levels that can accommodate all levels in @p levels.
     *
     * @param[in] levels Levels to be assigned.
     */
    Levels& operator=(std::vector<Level>&& levels);

    using
        // Member types.
        super::value_type,
        super::allocator_type,
        super::size_type,
        super::difference_type,
        super::reference,
        super::const_reference,
        super::pointer,
        super::const_pointer,
        super::iterator,
        super::const_iterator,
        super::reverse_iterator,
        super::const_reverse_iterator,
        // Member functions.
        super::assign,
        // super::assign_range // TODO(C++23): Enable when switching to C++23.
        super::get_allocator,
        // - Element access.
        super::at,
        super::operator[],
        super::front,
        super::back,
        super::data,
        super::begin,
        super::cbegin,
        super::end,
        super::cend,
        super::rbegin,
        super::crbegin,
        super::rend,
        super::crend,
        // - Capacity.
        super::empty,
        super::size,
        super::max_size,
        super::reserve,
        super::capacity,
        super::shrink_to_fit,
        // - Modifiers.
        super::clear,
        super::insert,
        // super::insert_range, // TODO(C++23): Enable when switching to C++23.
        super::emplace,
        super::erase,
        super::push_back,
        super::emplace_back,
        // super::append_range, // TODO(C++23): Enable when switching to C++23.
        super::pop_back,
        super::resize,
        super::swap;

    std::weak_ordering operator<=>(const Levels& other) const = default;
    bool operator==(const Levels& other) const = default;
    bool operator==(const std::vector<Level>& other) const { return static_cast<const std::vector<Level>&>(*this) == other; }

    /**
     * @brief Set level of @p state to @p level.
     *
     * If @p state is out of range, resize @c this to accommodate it.
     *
     * @c num_of_levels is not changed, in contrast to the behaviour of @c Levels::operator=.
     *
     * @param[in] state State whose level is to be set.
     * @param[in] level Level to be set for @p state.
     */
    Levels& set(State state, Level level = DEFAULT_LEVEL);
    /**
     * @brief Set levels of @c this to @p levels.
     *
     * @c num_of_levels is not changed, in contrast to the behaviour of @c Levels::operator=.
     *
     * @param[in] levels Vector of levels to be set.
     */
    Levels& set(const std::vector<Level>& levels);
    /**
     * @brief Set levels of @c this to @p levels.
     *
     * @c num_of_levels is not changed, in contrast to the behaviour of @c Levels::operator=.
     *
     * @param[in] levels Vector of levels to be set.
     */
    Levels& set(std::vector<Level>&& levels);

    /**
     * @brief Append @p levels_vector to the end of @c this.
     *
     * @param[in] levels_vector Vector of levels to be appended.
     */
    void append(const Levels& levels_vector);

    /**
     * @brief Count the number of occurrences of a level in @c this.
     *
     * @param[in] level Level to be counted.
     */
    size_t count(const Level level) const { return static_cast<size_t>(std::count(begin(), end(), level)); }

    /**
     * @brief Get levels of states in @p states.
     */
    std::vector<Level> get_levels_of(const utils::SparseSet<State>& states) const;
    /**
     * @brief Get levels of states in @p states.
     */
    std::vector<Level> get_levels_of(const StateSet& states) const;

    /**
     * @brief Get the minimal level for the states in @p states.
     *
     * "Minimal level" is defined as the level with the lowest numerical value, i.e., `0 < 1 < 2 < ... < num_of_levels-1`.
     * "Minimal" often relates to the current states ("What is the current state with minimal level?")
     */
    std::optional<Level> get_minimal_level_of(
            const StateSet& states, Ordering::Compare levels_ordering = Ordering::Minimal) const;

    /**
     * @brief Get the minimal next level for the states in @p states.
     *
     * "Minimal next level" is defined as the minimal level in the next transition (that may follow another level),
     *  i.e., `1 < 2 < ... < num_of_levels-1 < 0`.
     * "Minimal next" relates to the next target states ("What is the next minimal level to target in a transition?").
     */
    std::optional<Level> get_minimal_next_level_of(const StateSet& states) const;

    /**
     * @brief Check whether a transition can be made from a state with level @p source_level to a state with level
     *  @p target_level.
     *
     * A transition can be made if the target level is higher than the source level, or we can always jump to a state
     *  with level 0.
     *
     * @param[in] source_level Level of the source state.
     * @param[in] target_level Level of the target state.
     * @return @c true if the transition can be made, @c false otherwise.
     */
    static bool can_follow(Level source_level, Level target_level);

    /**
     * @brief Check whether a transition can be made from @p source to @p target.
     *
     * A transition can be made if the level of @p target is higher than the level of @p source, or if the level of
     *  @p target is 0.
     *
     * @param[in] source Source state.
     * @param[in] target Target state.
     * @return @c true if the transition can be made, @c false otherwise.
     */
    bool can_follow_for_states(State source, State target) const;

    std::optional<size_t> num_of_levels_used() const { return num_of_levels_of(*this); }

    static std::optional<size_t> num_of_levels_of(const Levels& levels) {
        if (levels.empty()) { return std::nullopt; }
        return std::ranges::max(levels) + 1;
    }
    static std::optional<size_t> num_of_levels_of(const std::vector<Level>& levels) {
        if (levels.empty()) { return std::nullopt; }
        return std::ranges::max(levels) + 1;
    }
    static std::optional<size_t> num_of_levels_of(const std::initializer_list<Level> levels) {
        if (levels.size() == 0) { return std::nullopt; }
        return std::ranges::max(levels) + 1;
    }

private:
    bool check_levels_in_range_() const { return std::ranges::all_of(*this, [&](const Level level) { return level < num_of_levels; }); }
};

} // namespace mata::nft.

#endif
