// TODO: Insert file header.

#ifndef MATA_MOVE_HH
#define MATA_MOVE_HH

#include "types.hh"

namespace Mata::Nfa {

/**
 * Structure represents a move which is a symbol and a set of target states of transitions.
 */
struct Move {
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

    StateSet::const_iterator cbegin() const  { return targets.cbegin(); }
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

} // namespace Mata::Nfa.

#endif //MATA_MOVE_HH
