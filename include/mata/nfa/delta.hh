// TODO: Insert file header.

#ifndef MATA_DELTA_HH
#define MATA_DELTA_HH

namespace Mata::Nfa {

/**
 * Structure represents a move which is a symbol and a set of target states of transitions.
 */
class Move {
public:
    Symbol symbol{};
    StateSet targets{};

    Move() = default;
    explicit Move(Symbol symbol) : symbol{ symbol }, targets{} {}
    Move(Symbol symbol, State state_to) : symbol{ symbol }, targets{ state_to } {}
    Move(Symbol symbol, StateSet states_to) : symbol{ symbol }, targets{ std::move(states_to) } {}

    Move(Move&& rhs) noexcept : symbol{ rhs.symbol }, targets{ std::move(rhs.targets) } {}
    Move(const Move& rhs) = default;
    Move& operator=(Move&& rhs) noexcept;
    Move& operator=(const Move& rhs) = default;

    inline bool operator<(const Move& rhs) const { return symbol < rhs.symbol; }
    inline bool operator<=(const Move& rhs) const { return symbol <= rhs.symbol; }
    inline bool operator==(const Move& rhs) const { return symbol == rhs.symbol; }
    inline bool operator!=(const Move& rhs) const { return symbol != rhs.symbol; }
    inline bool operator>(const Move& rhs) const { return symbol > rhs.symbol; }
    inline bool operator>=(const Move& rhs) const { return symbol >= rhs.symbol; }

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
 * Post is a data structure representing possible transitions over different symbols.
 * It is an ordered vector containing possible Moves (i.e., pair of symbol and target states.
 * Vector is ordered by symbols which are numbers.
 */
class Post : private Util::OrdVector<Move> {
private:
    using super = Util::OrdVector<Move>;
public:
    using super::iterator, super::const_iterator;
    using super::begin, super::end, super::cbegin, super::cend;
    using super::OrdVector;
    using super::operator=;
    Post(const Post&) = default;
    Post(Post&&) = default;
    Post& operator=(const Post&) = default;
    Post& operator=(Post&&) = default;
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
    std::vector<Post> posts;

public:
    inline static const Post empty_post; // When posts[q] is not allocated, then delta[q] returns this.

    Delta() : posts() {}
    explicit Delta(size_t n) : posts(n) {}

    void reserve(size_t n) {
        posts.reserve(n);
    };

    /**
     * Size of delta is number of all transitions, i.e. triples of form (state, symbol, state)
     */
    size_t size() const;

    // Get a non const reference to post of a state, which allows modifying the post.
    //
    // BEWARE, IT HAS A SIDE EFFECT.
    //
    // Namely, it allocates the post of the state if it was not allocated yet. This in turn may cause that
    // the entire post data structure is re-allocated, iterators to it get invalidated ...
    // Use the constant [] operator below if possible.
    // Or, to prevent the side effect from happening, one might want to make sure that posts of all states in the automaton
    // are allocated, e.g., write an NFA method that allocate delta for all states of the NFA.
    // But it feels fragile, before doing something like that, better think and talk to people.
    Post& get_mutable_post(State q);

    void defragment(const BoolVector& is_staying, const std::vector<State>& renaming);

    // Get a constant reference to the post of a state. No side effects.
    const Post & operator[] (State q) const;

    void emplace_back() { posts.emplace_back(); }

    void clear() { posts.clear(); }

    void increase_size(size_t n) {
        assert(n >= posts.size());
        posts.resize(n);
    }

    /**
     * @return Number of states in the whole Delta, including both source and target states.
     */
    size_t num_of_states() const { return posts.size(); }

    void add(State state_from, Symbol symbol, State state_to);
    void add(const Trans& trans) { add(trans.src, trans.symb, trans.tgt); }
    void remove(State src, Symbol symb, State tgt);
    void remove(const Trans& trans) { remove(trans.src, trans.symb, trans.tgt); }

    bool contains(State src, Symbol symb, State tgt) const;

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
    void append(const std::vector<Post>& post_vector) {
        for(const Post& pst : post_vector) {
            this->posts.push_back(pst);
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
    std::vector<Post> transform(const std::function<State(State)>& lambda) const;

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
    struct const_iterator {
    private:
        const std::vector<Post>& post;
        size_t current_state;
        Post::const_iterator post_iterator{};
        StateSet::const_iterator targets_position{};
        bool is_end;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = int;
        using difference_type = int;
        using pointer = int*;
        using reference = int&;

        explicit const_iterator(const std::vector<Post>& post_p, bool ise = false);

        const_iterator(const std::vector<Post>& post_p, size_t as,
                       Post::const_iterator pi, StateSet::const_iterator ti, bool ise = false) :
                post(post_p), current_state(as), post_iterator(pi), targets_position(ti), is_end(ise) {};

        const_iterator(const const_iterator& other) = default;

        Trans operator*() const { return Trans{current_state, (*post_iterator).symbol, *targets_position}; }

        // Prefix increment
        const_iterator& operator++();
        // Postfix increment
        const_iterator operator++(int);

        const_iterator& operator=(const const_iterator& x);

        friend bool operator==(const const_iterator& a, const const_iterator& b);
        friend bool operator!=(const const_iterator& a, const const_iterator& b) { return !(a == b); };
    };

    const_iterator cbegin() const { return const_iterator(posts); }
    const_iterator cend() const { return const_iterator(posts, true); }
    const_iterator begin() const { return cbegin(); }
    const_iterator end() const { return cend(); }

private:
    State find_max_state();
}; // struct Delta.

bool operator==(const Delta::const_iterator& a, const Delta::const_iterator& b);

} // namespace Mata::Nfa.

#endif //MATA_DELTA_HH
