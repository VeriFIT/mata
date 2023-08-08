// TODO: Insert file header.

#ifndef MATA_DELTA_HH
#define MATA_DELTA_HH

#include "mata/nfa/types.hh"

namespace Mata::Nfa {

/**
 * Structure represents a post of a single @c symbol: a set of target states in transitions.
 *
 * A set of @c SymbolPost, called @c StatePost, is describing the automata transitions from a single source state.
 */
class SymbolPost {
public:
    Symbol symbol{};
    StateSet targets{};

    SymbolPost() = default;
    explicit SymbolPost(Symbol symbol) : symbol{ symbol }, targets{} {}
    SymbolPost(Symbol symbol, State state_to) : symbol{ symbol }, targets{ state_to } {}
    SymbolPost(Symbol symbol, StateSet states_to) : symbol{ symbol }, targets{ std::move(states_to) } {}

    SymbolPost(SymbolPost&& rhs) noexcept : symbol{ rhs.symbol }, targets{ std::move(rhs.targets) } {}
    SymbolPost(const SymbolPost& rhs) = default;
    SymbolPost& operator=(SymbolPost&& rhs) noexcept;
    SymbolPost& operator=(const SymbolPost& rhs) = default;

    std::weak_ordering operator<=>(const SymbolPost& other) const { return symbol <=> other.symbol; }
    bool operator==(const SymbolPost& other) const { return symbol == other.symbol; }

    StateSet::iterator begin() { return targets.begin(); }
    StateSet::iterator end() { return targets.end(); }

    StateSet::const_iterator cbegin() const { return targets.cbegin(); }
    StateSet::const_iterator cend() const { return targets.cend(); }

    size_t count(State s) const { return targets.count(s); }
    bool empty() const { return targets.empty(); }
    size_t size() const { return targets.size(); }

    void insert(State s);
    void insert(const StateSet& states);

    // THIS BREAKS THE SORTEDNESS INVARIANT,
    // dangerous,
    // but useful for adding states in a random order to sort later (supposedly more efficient than inserting in a random order)
    void inline push_back(const State s) { targets.push_back(s); }

    void remove(State s) { targets.remove(s); }

    std::vector<State>::const_iterator find(State s) const { return targets.find(s); }
    std::vector<State>::iterator find(State s) { return targets.find(s); }
}; // class Mata::Nfa::Move.

/**
 * @brief A data structure representing possible transitions over different symbols from a source state.
 *
 * It is an ordered vector containing possible @c SymbolPost (i.e., pair of symbol and target states).
 * @c SymbolPosts in the vector are ordered by symbols in @c SymbolPosts.
 */
class StatePost : private Util::OrdVector<SymbolPost> {
private:
    using super = Util::OrdVector<SymbolPost>;
public:
    using super::iterator, super::const_iterator;
    using super::begin, super::end, super::cbegin, super::cend;
    using super::OrdVector;
    using super::operator=;
    StatePost(const StatePost&) = default;
    StatePost(StatePost&&) = default;
    StatePost& operator=(const StatePost&) = default;
    StatePost& operator=(StatePost&&) = default;
    using super::insert;
    using super::reserve;
    using super::remove;
    using super::empty, super::size;
    using super::ToVector;
    using super::erase;
    // dangerous, breaks the sortedness invariant
    using super::push_back;
    // is adding non-const version as well ok?
    using super::back;
    using super::filter;

    using super::find;
    iterator find(const Symbol symbol) { return super::find({ symbol, {} }); }
    const_iterator find(const Symbol symbol) const { return super::find({ symbol, {} }); }
}; // struct Post.

/**
 * Delta is a data structure for representing transition relation.
 * Its underlying data structure is vector of Post structures.
 * Each index of vector corresponds to one state, that is a number of
 * state is an index to the vector of Posts.
 */
class Delta {
private:
    std::vector<StatePost> state_posts;

public:
    inline static const StatePost empty_state_post; // When posts[q] is not allocated, then delta[q] returns this.

    Delta() : state_posts() {}
    explicit Delta(size_t n) : state_posts(n) {}

    void reserve(size_t n) {
        state_posts.reserve(n);
    };

    /**
     * Size of delta is number of all transitions, i.e. triples of form (state, symbol, state)
     */
    size_t size() const;

    /**
     * @brief Get constant reference to the state post of @p src_state.
     *
     * If we try to access a state post of a @p src_state which is present in the automaton as an initial/final state,
     *  yet does not have allocated space in @c Delta, an @c empty_post is returned. Hence, the function has no side
     *  effects (no allocation is performed; iterators remain valid).
     * @param state_from[in] Source state of a state post to access.
     * @return State post of @p src_state.
     */
    const StatePost& state_post(const State src_state) const {
        if (src_state >= num_of_states()) {
            return empty_state_post;
        }
        return state_posts[src_state];
    }

    /**
     * @brief Get constant reference to the state post of @p src_state.
     *
     * If we try to access a state post of a @p src_state which is present in the automaton as an initial/final state,
     *  yet does not have allocated space in @c Delta, an @c empty_post is returned. Hence, the function has no side
     *  effects (no allocation is performed; iterators remain valid).
     * @param state_from[in] Source state of a state post to access.
     * @return State post of @p src_state.
     */
    const StatePost& operator[](State src_state) const { return state_post(src_state); }

    /**
     * @brief Get mutable (non-constant) reference to the state post of @p src_state.
     *
     * The function allows modifying the state post.
     *
     * BEWARE, IT HAS A SIDE EFFECT.
     *
     * If we try to access a state post of a @p src_state which is present in the automaton as an initial/final state,
     *  yet does not have allocated space in @c Delta, a new state post for @p src_state will be allocated along with
     *  all state posts for all previous states. This in turn may cause that the entire post data structure is
     *  re-allocated. Iterators to @c Delta will get invalidated.
     * Use the constant 'state_post()' is possible. Or, to prevent the side effect from causing issues, one might want
     *  to make sure that posts of all states in the automaton are allocated, e.g., write an NFA method that allocate
     *  @c Delta for all states of the NFA.
     * @param state_from[in] Source state of a state post to access.
     * @return State post of @p src_state.
     */
    StatePost& mutable_state_post(State src_state);

    void defragment(const BoolVector& is_staying, const std::vector<State>& renaming);

    void emplace_back() { state_posts.emplace_back(); }

    void clear() { state_posts.clear(); }

    void increase_size(size_t n) {
        assert(n >= state_posts.size());
        state_posts.resize(n);
    }

    /**
     * @return Number of states in the whole Delta, including both source and target states.
     */
    size_t num_of_states() const { return state_posts.size(); }

    void add(State state_from, Symbol symbol, State state_to);
    void add(const Trans& trans) { add(trans.src, trans.symb, trans.tgt); }
    void remove(State src, Symbol symb, State tgt);
    void remove(const Trans& trans) { remove(trans.src, trans.symb, trans.tgt); }

    /**
     * Check whether @c Delta contains a passed transition.
     */
    bool contains(State src, Symbol symb, State tgt) const;
    /**
     * Check whether @c Delta contains a transition passed as a triple.
     */
    bool contains(const Trans& transition) const;

    /**
     * Check whether automaton contains no transitions.
     * @return True if there are no transitions in the automaton, false otherwise.
     */
    bool empty() const;

    /**
     * @brief Append post vector to the delta.
     *
     * @param post_vector Vector of posts to be appended.
     */
    void append(const std::vector<StatePost>& post_vector) {
        for(const StatePost& pst : post_vector) {
            this->state_posts.push_back(pst);
        }
    }

    /**
     * @brief Copy posts of delta and apply a lambda update function on each state from
     * targets.
     *
     * IMPORTANT: In order to work properly, the lambda function needs to be
     * monotonic.
     *
     * @param lambda Monotonic lambda function mapping states to different states
     * @return std::vector<Post> Copied posts.
     */
    std::vector<StatePost> transform(const std::function<State(State)>& lambda) const;

    /**
     * @brief Add transitions to multiple destinations
     *
     * @param state_from From
     * @param symbol Symbol
     * @param states Set of states to
     */
    void add(const State state_from, const Symbol symbol, const StateSet& states);

    /**
     * Iterator over transitions. It iterates over triples (lhs, symbol, rhs) where lhs and rhs are states.
     */
    struct transitions_const_iterator {
    private:
        const std::vector<StatePost>& post;
        size_t current_state;
        StatePost::const_iterator post_iterator{};
        StateSet::const_iterator targets_position{};
        bool is_end;
        Trans transition{};

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = int;
        using difference_type = int;
        using pointer = int*;
        using reference = int&;

        explicit transitions_const_iterator(const std::vector<StatePost>& post_p, bool ise = false);

        transitions_const_iterator(const std::vector<StatePost>& post_p, size_t as,
                                   StatePost::const_iterator pi, StateSet::const_iterator ti, bool ise = false);

        transitions_const_iterator(const transitions_const_iterator& other) = default;

        const Trans& operator*() const { return transition; }

        // Prefix increment
        transitions_const_iterator& operator++();
        // Postfix increment
        const transitions_const_iterator operator++(int);

        transitions_const_iterator& operator=(const transitions_const_iterator& x);

        friend bool operator==(const transitions_const_iterator& a, const transitions_const_iterator& b);
        friend bool operator!=(const transitions_const_iterator& a, const transitions_const_iterator& b) { return !(a == b); };
    };

    transitions_const_iterator transitions_cbegin() const { return transitions_const_iterator(state_posts); }
    transitions_const_iterator transitions_cend() const { return transitions_const_iterator(state_posts, true); }
    transitions_const_iterator transitions_begin() const { return transitions_cbegin(); }
    transitions_const_iterator transitions_end() const { return transitions_cend(); }

    struct TransitionsIterator {
        transitions_const_iterator m_begin;
        transitions_const_iterator m_end;
        transitions_const_iterator begin() const { return m_begin; }
        transitions_const_iterator end() const { return m_end; }
    };

    /**
     * Iterate over transitions represented as 'Trans' instances.
     * @return Iterator over transitions.
     */
    TransitionsIterator transitions() const {
        return { .m_begin = transitions_begin(), .m_end = transitions_end() };
    }

    using const_iterator = std::vector<StatePost>::const_iterator;
    const_iterator cbegin() const { return state_posts.cbegin(); }
    const_iterator cend() const { return state_posts.cend(); }
    const_iterator begin() const { return state_posts.begin(); }
    const_iterator end() const { return state_posts.end(); }
}; // struct Delta.

bool operator==(const Delta::transitions_const_iterator& a, const Delta::transitions_const_iterator& b);

} // namespace Mata::Nfa.

#endif //MATA_DELTA_HH
