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

using namespace Mata::Util;
using namespace Mata::Nfa;
using Mata::Symbol;

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

size_t Delta::size() const {
    size_t size = 0;
    for (State q = 0; q < num_of_states(); ++q) {
        for (const SymbolPost & m: (*this)[q]) { size = size + m.size(); }
    }
    return size;
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

bool Delta::empty() const
{
    return this->transitions_begin() == this->transitions_end();
}

Delta::transitions_const_iterator::transitions_const_iterator(const std::vector<StatePost>& post_p, bool ise)
    : post_(post_p), current_state_(0), is_end_{ ise } {
    const size_t post_size = post_.size();
    for (size_t i = 0; i < post_size; ++i) {
        if (!post_[i].empty()) {
            current_state_ = i;
            post_iterator_ = post_[i].begin();
            targets_position_ = post_iterator_->targets.begin();
            transition_.source = current_state_;
            transition_.symbol = post_iterator_->symbol;
            transition_.target = *targets_position_;
            return;
        }
    }

    // no transition found, an empty post
    is_end_ = true;
}

Delta::transitions_const_iterator::transitions_const_iterator(
    const std::vector<StatePost>& post_p, size_t as, StatePost::const_iterator pi, StateSet::const_iterator ti,
    bool ise)
    : post_(post_p), current_state_(as), post_iterator_(pi), targets_position_(ti), is_end_(ise) {
    transition_.source = current_state_;
    transition_.symbol = post_iterator_->symbol;
    transition_.target = *targets_position_;
};

Delta::transitions_const_iterator& Delta::transitions_const_iterator::operator++() {
    assert(post_.begin() != post_.end());

    ++targets_position_;
    if (targets_position_ != post_iterator_->targets.end()) {
        transition_.target = *targets_position_;
        return *this;
    }

    ++post_iterator_;
    if (post_iterator_ != post_[current_state_].cend()) {
        targets_position_ = post_iterator_->targets.begin();
        transition_.symbol = post_iterator_->symbol;
        transition_.target = *targets_position_;
        return *this;
    }

    ++current_state_;
    while (current_state_ < post_.size() && post_[current_state_].empty()) // skip empty posts
        current_state_++;

    if (current_state_ >= post_.size())
        is_end_ = true;
    else {
        post_iterator_ = post_[current_state_].begin();
        targets_position_ = post_iterator_->targets.begin();
    }

    transition_.source = current_state_;
    transition_.symbol = post_iterator_->symbol;
    transition_.target = *targets_position_;

    return *this;
}

const Delta::transitions_const_iterator Delta::transitions_const_iterator::operator++(int) {
    const transitions_const_iterator tmp = *this;
    ++(*this);
    return tmp;
}

Delta::transitions_const_iterator& Delta::transitions_const_iterator::operator=(const Delta::transitions_const_iterator& x) {
    // FIXME: this->post is never updated, because it is a const reference to std::vector which does not have assignment
    //  operator defined.
    this->post_iterator_ = x.post_iterator_;
    this->targets_position_ = x.targets_position_;
    this->current_state_ = x.current_state_;
    this->is_end_ = x.is_end_;
    transition_.source = x.transition_.source;
    transition_.symbol = x.transition_.symbol;
    transition_.target = x.transition_.target;
    return *this;
}

bool Mata::Nfa::Delta::transitions_const_iterator::operator==(const Delta::transitions_const_iterator& other) const {
    if (is_end_ && other.is_end_) {
        return true;
    } else if ((is_end_ && !other.is_end_) || (!is_end_ && other.is_end_)) {
        return false;
    } else {
        return current_state_ == other.current_state_ && post_iterator_ == other.post_iterator_
               && targets_position_ == other.targets_position_;
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
        Util::reserve_on_insert(state_posts_, q);
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

StatePost::moves_const_iterator::moves_const_iterator(const std::vector<SymbolPost>& symbol_posts, bool is_end)
    : symbol_posts_{ symbol_posts }, symbol_post_it_{ symbol_posts_.cbegin() }, is_end_{ is_end } {
    while (symbol_post_it_ != symbol_posts_.cend()) {
        if (!symbol_post_it_->empty()) {
            target_states_it_ = symbol_post_it_->targets.begin();
            move_.symbol = symbol_post_it_->symbol;
            move_.target = *target_states_it_;
            return;
        }
        ++symbol_post_it_;
    }
    // No move found. We are at the end of moves.
    is_end_ = true;
}

StatePost::moves_const_iterator::moves_const_iterator(
    const std::vector<SymbolPost>& symbol_posts, std::vector<SymbolPost>::const_iterator symbol_posts_it,
    StateSet::const_iterator target_states_it, bool is_end)
    : symbol_posts_{ symbol_posts }, symbol_post_it_{ symbol_posts_it }, target_states_it_{ target_states_it },
      is_end_{ is_end } {
        while (symbol_post_it_ != symbol_posts_.cend()) {
            if (!symbol_post_it_->empty()) {
                target_states_it_ = symbol_post_it_->targets.begin();
                move_.symbol = symbol_post_it_->symbol;
                move_.target = *target_states_it_;
                return;
            }
        }
        // No move found. We are at the end of moves.
        is_end_ = true;
}

StatePost::moves_const_iterator& StatePost::moves_const_iterator::operator++() {
    assert(symbol_posts_.begin() != symbol_posts_.end());

    ++target_states_it_;
    if (target_states_it_ != symbol_post_it_->targets.end()) {
        move_.target = *target_states_it_;
        return *this;
    }

    ++symbol_post_it_;
    if (symbol_post_it_ != symbol_posts_.cend()) {
        target_states_it_ = symbol_post_it_->targets.begin();
        move_.symbol = symbol_post_it_->symbol;
        move_.target = *target_states_it_;
        return *this;
    }

    is_end_ = true;
    return *this;
}

const StatePost::moves_const_iterator StatePost::moves_const_iterator::operator++(int) {
    const moves_const_iterator tmp = *this;
    ++(*this);
    return tmp;
}

StatePost::moves_const_iterator& StatePost::moves_const_iterator::operator=(const StatePost::moves_const_iterator& x) {
    // FIXME: this->symbol_posts is never updated, because it is a const reference to std::vector which does not have
    //  assignment operator defined.
    is_end_ = x.is_end_;
    move_.symbol = x.move_.symbol;
    move_.target = x.move_.target;
    symbol_post_it_ = x.symbol_post_it_;
    target_states_it_ = x.target_states_it_;
    return *this;
}

bool Mata::Nfa::StatePost::moves_const_iterator::operator==(const StatePost::moves_const_iterator& other) const {
    if (is_end_ && other.is_end_) {
        return true;
    } else if ((is_end_ && !other.is_end_) || (!is_end_ && other.is_end_)) {
        return false;
    } else {
        return symbol_post_it_ == other.symbol_post_it_ && target_states_it_ == other.target_states_it_;
    }
}
