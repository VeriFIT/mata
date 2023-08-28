/* delta.cc -- Operations on Delta.
 *
 */
// TODO: Add file header.

#include "mata/utils/sparse-set.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/delta.hh"


#include <algorithm>
#include <list>
#include <iterator>

using namespace mata::utils;
using namespace mata::nfa;
using mata::Symbol;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

SymbolPost& SymbolPost::operator=(SymbolPost&& rhs) noexcept {
    if (*this != rhs) {
        symbol = rhs.symbol;
        targets = std::move(rhs.targets);
    }
    return *this;
}

void SymbolPost::insert(State s) {
    if(targets.empty() || targets.back() < s) {
        targets.push_back(s);
        return;
    }
    // Find the place where to put the element (if not present).
    // insert to OrdVector without the searching of a proper position inside insert(const Key&x).
    auto it = std::lower_bound(targets.begin(), targets.end(), s);
    if (it == targets.end() || *it != s) {
        targets.insert(it, s);
    }
}

void SymbolPost::insert(const StateSet& states) {
    for (State s : states) {
        insert(s);
    }
}

StatePost::const_iterator Delta::epsilon_symbol_posts(const State state, const Symbol epsilon) const {
    return epsilon_symbol_posts(state_post(state), epsilon);
}

StatePost::const_iterator Delta::epsilon_symbol_posts(const StatePost& state_post, const Symbol epsilon) {
    if (!state_post.empty()) {
        if (epsilon == EPSILON) {
            const auto& back = state_post.back();
            if (back.symbol == epsilon) { return std::prev(state_post.end()); }
        } else { return state_post.find(SymbolPost(epsilon)); }
    }
    return state_post.end();
}

void Delta::add(State source, Symbol symbol, State target) {
    const State max_state{ std::max(source, target) };
    if (max_state >= state_posts_.size()) {
        reserve_on_insert(state_posts_, max_state);
        state_posts_.resize(max_state + 1);
    }

    StatePost& state_transitions{ state_posts_[source] };

    if (state_transitions.empty()) {
        state_transitions.insert({ symbol, target });
    } else if (state_transitions.back().symbol < symbol) {
        state_transitions.insert({ symbol, target });
    } else {
        const auto symbol_transitions{ state_transitions.find(SymbolPost{ symbol }) };
        if (symbol_transitions != state_transitions.end()) {
            // Add transition with symbol already used on transitions from state_from.
            symbol_transitions->insert(target);
        } else {
            // Add transition to a new Move struct with symbol yet unused on transitions from state_from.
            const SymbolPost new_symbol_transitions{ symbol, target };
            state_transitions.insert(new_symbol_transitions);
        }
    }
}

void Delta::add(const State source, const Symbol symbol, const StateSet& targets) {
    if(targets.empty()) {
        return;
    }

    const State max_state{ std::max(source, targets.back()) };
    if (max_state >= state_posts_.size()) {
        reserve_on_insert(state_posts_, max_state + 1);
        state_posts_.resize(max_state + 1);
    }

    StatePost& state_transitions{ state_posts_[source] };

    if (state_transitions.empty()) {
        state_transitions.insert({ symbol, targets });
    } else if (state_transitions.back().symbol < symbol) {
        state_transitions.insert({ symbol, targets });
    } else {
        const auto symbol_transitions{ state_transitions.find(symbol) };
        if (symbol_transitions != state_transitions.end()) {
            // Add transition with symbolOnTransition already used on transitions from state_from.
            symbol_transitions->insert(targets);

        } else {
            // Add transition to a new Move struct with symbol yet unused on transitions from state_from.
            // Move new_symbol_transitions{ symbol, states };
            state_transitions.insert(SymbolPost{ symbol, targets});
        }
    }
}

void Delta::remove(State src, Symbol symb, State tgt) {
    if (src >= state_posts_.size()) {
        return;
    }

    StatePost& state_transitions{ state_posts_[src] };
    if (state_transitions.empty()) {
        throw std::invalid_argument(
                "Transition [" + std::to_string(src) + ", " + std::to_string(symb) + ", " +
                std::to_string(tgt) + "] does not exist.");
    } else if (state_transitions.back().symbol < symb) {
        throw std::invalid_argument(
                "Transition [" + std::to_string(src) + ", " + std::to_string(symb) + ", " +
                std::to_string(tgt) + "] does not exist.");
    } else {
        const auto symbol_transitions{ state_transitions.find(symb) };
        if (symbol_transitions == state_transitions.end()) {
            throw std::invalid_argument(
                    "Transition [" + std::to_string(src) + ", " + std::to_string(symb) + ", " +
                    std::to_string(tgt) + "] does not exist.");
        } else {
            symbol_transitions->remove(tgt);
            if (symbol_transitions->empty()) {
                state_posts_[src].remove(*symbol_transitions);
            }
        }
    }
}

bool Delta::contains(State src, Symbol symb, State tgt) const
{ // {{{
    if (state_posts_.empty()) {
        return false;
    }

    if (state_posts_.size() <= src)
        return false;

    const StatePost& tl = state_posts_[src];
    if (tl.empty()) {
        return false;
    }
    auto symbol_transitions{ tl.find(SymbolPost{ symb} ) };
    if (symbol_transitions == tl.cend()) {
        return false;
    }

    return symbol_transitions->targets.find(tgt) != symbol_transitions->targets.end();
}

bool Delta::contains(const Transition& transition) const {
    return contains(transition.source, transition.symbol, transition.target);
}

size_t Delta::num_of_transitions() const {
    size_t number_of_transitions{ 0 };
    for (const StatePost& state_post: state_posts_) {
        for (const SymbolPost& symbol_post: state_post) {
            number_of_transitions += symbol_post.size();
        }
    }
    return number_of_transitions;
}

bool Delta::empty() const {
    for (const StatePost& state_post: state_posts_) {
        if (!state_post.empty()) { return false; }
    }
    return true;
}

Delta::Transitions::const_iterator::const_iterator(const Delta& delta): delta_{ &delta }, current_state_{ 0 } {
    const size_t post_size = delta_->num_of_states();
    for (size_t i = 0; i < post_size; ++i) {
        if (!(*delta_)[i].empty()) {
            current_state_ = i;
            state_post_it_ = (*delta_)[i].begin();
            symbol_post_it_ = state_post_it_->targets.begin();
            transition_.source = current_state_;
            transition_.symbol = state_post_it_->symbol;
            transition_.target = *symbol_post_it_;
            return;
        }
    }

    // No transition found, delta contains only empty state posts.
    is_end_ = true;
}

Delta::Transitions::const_iterator::const_iterator(const Delta& delta, State current_state)
    : delta_{ &delta }, current_state_{ current_state } {
    const size_t post_size = delta_->num_of_states();
    for (State source{ current_state_ }; source < post_size; ++source) {
        const StatePost& state_post{ delta_->state_post(source) };
        if (!state_post.empty()) {
            current_state_ = source;
            state_post_it_ = state_post.begin();
            symbol_post_it_ = state_post_it_->targets.begin();
            transition_.source = current_state_;
            transition_.symbol = state_post_it_->symbol;
            transition_.target = *symbol_post_it_;
            return;
        }
    }

    // No transition found, delta from the current state contains only empty state posts.
    is_end_ = true;
}

Delta::Transitions::const_iterator& Delta::Transitions::const_iterator::operator++() {
    assert(delta_->begin() != delta_->end());

    ++symbol_post_it_;
    if (symbol_post_it_ != state_post_it_->targets.end()) {
        transition_.target = *symbol_post_it_;
        return *this;
    }

    ++state_post_it_;
    if (state_post_it_ != (*delta_)[current_state_].cend()) {
        symbol_post_it_ = state_post_it_->targets.begin();
        transition_.symbol = state_post_it_->symbol;
        transition_.target = *symbol_post_it_;
        return *this;
    }

    const size_t state_posts_size{ delta_->num_of_states() };
    do { // Skip empty posts.
        ++current_state_;
    } while (current_state_ < state_posts_size && (*delta_)[current_state_].empty());
    if (current_state_ >= state_posts_size) {
        is_end_ = true;
        return *this;
    }

    const StatePost& state_post{ (*delta_)[current_state_] };
    state_post_it_ = state_post.begin();
    symbol_post_it_ = state_post_it_->targets.begin();

    transition_.source = current_state_;
    transition_.symbol = state_post_it_->symbol;
    transition_.target = *symbol_post_it_;

    return *this;
}

const Delta::Transitions::const_iterator Delta::Transitions::const_iterator::operator++(int) {
    const Delta::Transitions::const_iterator tmp{ *this };
    ++(*this);
    return tmp;
}

bool Delta::Transitions::const_iterator::operator==(const Delta::Transitions::const_iterator& other) const {
    if (is_end_ && other.is_end_) {
        return true;
    } else if ((is_end_ && !other.is_end_) || (!is_end_ && other.is_end_)) {
        return false;
    } else {
        return current_state_ == other.current_state_ && state_post_it_ == other.state_post_it_
               && symbol_post_it_ == other.symbol_post_it_;
    }
}

std::vector<StatePost> Delta::renumber_targets(const std::function<State(State)>& target_renumberer) const {
    std::vector<StatePost> copied_state_posts;
    copied_state_posts.reserve(num_of_states());
    for(const StatePost& state_post: state_posts_) {
        StatePost copied_state_post;
        copied_state_post.reserve(state_post.size());
        for(const SymbolPost& symbol_post: state_post) {
            StateSet copied_targets;
            copied_targets.reserve(symbol_post.size());
            for(const State& state: symbol_post.targets) {
                copied_targets.push_back(std::move(target_renumberer(state)));
            }
            copied_state_post.push_back(std::move(SymbolPost(symbol_post.symbol, copied_targets)));
        }
        copied_state_posts.emplace_back(copied_state_post);
    }
    return copied_state_posts;
}

StatePost& Delta::mutable_state_post(State q) {
    if (q >= state_posts_.size()) {
        utils::reserve_on_insert(state_posts_, q);
        const size_t new_size{ q + 1 };
        state_posts_.resize(new_size);
    }

    return state_posts_[q];
}

void Delta::defragment(const BoolVector& is_staying, const std::vector<State>& renaming) {
    //TODO: this function seems to be unreadable, should be refactored, maybe into several functions with a clear functionality?

    //first, indexes of post are filtered (places of to be removed states are taken by states on their right)
    size_t move_index{ 0 };
    std::erase_if(state_posts_,
         [&](StatePost&) -> bool {
             size_t prev{ move_index };
             ++move_index;
             return !is_staying[prev];
         }
    );

    //this iterates through every post and every move, filters and renames states,
    //and then removes moves that became empty.
    for (State q=0,size=state_posts_.size(); q < size; ++q) {
        StatePost & p = mutable_state_post(q);
        for (auto move = p.begin(); move < p.end(); ++move) {
            move->targets.erase(
                    std::remove_if(move->targets.begin(), move->targets.end(), [&](State q) -> bool {
                        return !is_staying[q];
                    }),
                    move->targets.end()
            );
            move->targets.rename(renaming);
        }
        p.erase(
                std::remove_if(p.begin(), p.end(), [&](SymbolPost& move) -> bool {
                    return move.targets.empty();
                }),
                p.end()
        );
    }
}

bool Delta::operator==(const Delta& other) const {
    Delta::Transitions this_transitions{ transitions() };
    Delta::Transitions::const_iterator this_transitions_it{ this_transitions.begin() };
    const Delta::Transitions::const_iterator this_transitions_end{ this_transitions.end() };
    Delta::Transitions other_transitions{ other.transitions() };
    Delta::Transitions::const_iterator other_transitions_it{ other_transitions.begin() };
    const Delta::Transitions::const_iterator other_transitions_end{ other_transitions.end() };
    while (this_transitions_it != this_transitions_end) {
        if (other_transitions_it == other_transitions_end || *this_transitions_it != *other_transitions_it) {
            return false;
        }
        ++this_transitions_it;
        ++other_transitions_it;
    }
    return other_transitions_it == other_transitions_end;
}

StatePost::Moves::const_iterator::const_iterator(
    const StatePost& state_post, const Symbol first_symbol, const Symbol last_symbol,
    const Moves::FirstSymbolLookupDiretion first_symbol_lookup_direction)
    : state_post_{ &state_post }, last_symbol_{ last_symbol } {
    if (state_post_->empty()) {
        is_end_ = true;
        return;
    }
    const StatePost::const_iterator state_post_begin{ state_post_->begin() };
    const StatePost::const_iterator state_post_end{ state_post_->end() };
    if (first_symbol_lookup_direction == Moves::FirstSymbolLookupDiretion::Forward) {
        state_post_it_ = state_post_begin;
        while (state_post_it_ != state_post_end && first_symbol > state_post_it_->symbol) { ++state_post_it_; }
        if (state_post_it_ == state_post_end || state_post_it_->symbol > last_symbol_) {
            is_end_ = true;
            return;
        }
    } else { // first_symbol_lookup_direction == Moves::FirstSymbolLookupDiretion::Backward.
        auto previous_state_post_it = state_post_end;
        do {
            state_post_it_ = previous_state_post_it;
            --previous_state_post_it;
        } while (previous_state_post_it != state_post_begin && first_symbol < previous_state_post_it->symbol);
        if (previous_state_post_it == state_post_begin || first_symbol == previous_state_post_it->symbol) {
            state_post_it_ = previous_state_post_it;
        }
        if (state_post_it_->symbol < first_symbol) {
            is_end_ = true;
            return;
        }
    }
    symbol_post_it_ = state_post_it_->targets.begin();
    move_.symbol = state_post_it_->symbol;
    move_.target = *symbol_post_it_;
}

StatePost::Moves::const_iterator& StatePost::Moves::const_iterator::operator++() {
    ++symbol_post_it_;
    if (symbol_post_it_ != state_post_it_->targets.end()) {
        move_.target = *symbol_post_it_;
        return *this;
    }

    if (state_post_it_->symbol >= last_symbol_) {
        // Last symbol symbol post was reached and fully iterated over. We would now iterate to the next symbol, but
        //  that would already be over the iteration end specified by 'last_symbol_'.
        is_end_ = true;
        return *this;
    }
    // Iterate over to the next symbol post, which can be either an end iterator, or symbol post whose
    //  symbol <= last_symbol_.
    ++state_post_it_;
    if (state_post_it_ == state_post_->cend() || state_post_it_->symbol > last_symbol_) {
        is_end_ = true;
        return *this;
    }
    // The current symbol post is valid (not end iterator and symbol <= last_symbol_).
    symbol_post_it_ = state_post_it_->targets.begin();
    move_.symbol = state_post_it_->symbol;
    move_.target = *symbol_post_it_;
    return *this;
}

const StatePost::Moves::const_iterator StatePost::Moves::const_iterator::operator++(int) {
    const StatePost::Moves::const_iterator tmp{ *this };
    ++(*this);
    return tmp;
}

bool StatePost::Moves::const_iterator::operator==(const StatePost::Moves::const_iterator& other) const {
    if (is_end_ && other.is_end_) {
        return true;
    } else if ((is_end_ && !other.is_end_) || (!is_end_ && other.is_end_)) {
        return false;
    }
    return state_post_it_ == other.state_post_it_ && symbol_post_it_ == other.symbol_post_it_;
}

size_t StatePost::num_of_moves() const {
    size_t counter{ 0 };
    for (const SymbolPost& symbol_post: *this) {
        counter += symbol_post.size();
    }
    return counter;
}

StatePost::Moves& StatePost::Moves::operator=(StatePost::Moves&& other) noexcept {
    if (&other != this) {
        state_post_ = other.state_post_;
        first_symbol_ = other.first_symbol_;
        last_symbol_ = other.last_symbol_;
        first_symbol_lookup_direction_ = other.first_symbol_lookup_direction_;
    }
    return *this;
}

StatePost::Moves& StatePost::Moves::operator=(const Moves& other) noexcept {
    if (&other != this) {
        state_post_ = other.state_post_;
        first_symbol_ = other.first_symbol_;
        last_symbol_ = other.last_symbol_;
        first_symbol_lookup_direction_ = other.first_symbol_lookup_direction_;
    }
    return *this;
}

StatePost::Moves StatePost::moves() const { return { *this }; }

StatePost::Moves StatePost::epsilon_moves(const Symbol first_epsilon) const {
    return { *this, first_epsilon, Limits::max_symbol, Moves::FirstSymbolLookupDiretion::Backward};
}

StatePost::Moves StatePost::symbol_moves(const Symbol last_symbol) const {
    return { *this, Limits::min_symbol, last_symbol };
}

StatePost::Moves::const_iterator StatePost::Moves::begin() const {
     return { *state_post_, first_symbol_, last_symbol_, first_symbol_lookup_direction_ }; 
}

StatePost::Moves::const_iterator StatePost::Moves::end() const { return const_iterator{}; }

Delta::Transitions Delta::transitions() const { return Transitions{ this }; }

Delta::Transitions::const_iterator Delta::Transitions::begin() const { return const_iterator{ *delta_ }; }
Delta::Transitions::const_iterator Delta::Transitions::end() const { return const_iterator{}; }
