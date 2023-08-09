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

StatePost::const_iterator Nfa::Nfa::get_epsilon_transitions(const State state, const Symbol epsilon) const {
    assert(is_state(state));
    return get_epsilon_transitions(delta.state_post(state), epsilon);
}

StatePost::const_iterator Nfa::Nfa::get_epsilon_transitions(const StatePost& state_transitions, const Symbol epsilon) {
    if (!state_transitions.empty()) {
        if (epsilon == EPSILON) {
            const auto& back = state_transitions.back();
            if (back.symbol == epsilon) {
                return std::prev(state_transitions.end());
            }
        } else {
            return state_transitions.find(SymbolPost(epsilon));
        }
    }

    return state_transitions.end();
}

size_t Delta::size() const
{
    size_t size = 0;
    for (State q = 0; q < num_of_states(); ++q)
        for (const SymbolPost & m: (*this)[q])
            size = size + m.size();

    return size;
}

void Delta::add(State state_from, Symbol symbol, State state_to) {
    const State max_state{ std::max(state_from, state_to) };
    if (max_state >= state_posts.size()) {
        reserve_on_insert(state_posts, max_state);
        state_posts.resize(max_state + 1);
    }

    StatePost& state_transitions{ state_posts[state_from] };

    if (state_transitions.empty()) {
        state_transitions.insert({ symbol, state_to });
    } else if (state_transitions.back().symbol < symbol) {
        state_transitions.insert({ symbol, state_to });
    } else {
        const auto symbol_transitions{ state_transitions.find(SymbolPost{ symbol }) };
        if (symbol_transitions != state_transitions.end()) {
            // Add transition with symbol already used on transitions from state_from.
            symbol_transitions->insert(state_to);
        } else {
            // Add transition to a new Move struct with symbol yet unused on transitions from state_from.
            const SymbolPost new_symbol_transitions{ symbol, state_to };
            state_transitions.insert(new_symbol_transitions);
        }
    }
}

void Delta::add(const State state_from, const Symbol symbol, const StateSet& states) {
    if(states.empty()) {
        return;
    }

    const State max_state{ std::max(state_from, states.back()) };
    if (max_state >= state_posts.size()) {
        reserve_on_insert(state_posts, max_state + 1);
        state_posts.resize(max_state + 1);
    }

    StatePost& state_transitions{ state_posts[state_from] };

    if (state_transitions.empty()) {
        state_transitions.insert({ symbol, states });
    } else if (state_transitions.back().symbol < symbol) {
        state_transitions.insert({ symbol, states });
    } else {
        const auto symbol_transitions{ state_transitions.find(symbol) };
        if (symbol_transitions != state_transitions.end()) {
            // Add transition with symbolOnTransition already used on transitions from state_from.
            symbol_transitions->insert(states);

        } else {
            // Add transition to a new Move struct with symbol yet unused on transitions from state_from.
            // Move new_symbol_transitions{ symbol, states };
            state_transitions.insert(SymbolPost{ symbol, states});
        }
    }
}

void Delta::remove(State src, Symbol symb, State tgt) {
    if (src >= state_posts.size()) {
        return;
    }

    StatePost& state_transitions{ state_posts[src] };
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
                state_posts[src].remove(*symbol_transitions);
            }
        }
    }
}

bool Delta::contains(State src, Symbol symb, State tgt) const
{ // {{{
    if (state_posts.empty()) {
        return false;
    }

    if (state_posts.size() <= src)
        return false;

    const StatePost& tl = state_posts[src];
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
    : post(post_p), current_state(0), is_end{ ise } {
    const size_t post_size = post.size();
    for (size_t i = 0; i < post_size; ++i) {
        if (!post[i].empty()) {
            current_state = i;
            post_iterator = post[i].begin();
            targets_position = post_iterator->targets.begin();
            transition.source = current_state;
            transition.symbol = post_iterator->symbol;
            transition.target = *targets_position;
            return;
        }
    }

    // no transition found, an empty post
    is_end = true;
}

Delta::transitions_const_iterator::transitions_const_iterator(
    const std::vector<StatePost>& post_p, size_t as, StatePost::const_iterator pi, StateSet::const_iterator ti,
    bool ise)
    : post(post_p), current_state(as), post_iterator(pi), targets_position(ti), is_end(ise) {
    transition.source = current_state;
    transition.symbol = post_iterator->symbol;
    transition.target = *targets_position;
};

Delta::transitions_const_iterator& Delta::transitions_const_iterator::operator++() {
    assert(post.begin() != post.end());

    ++targets_position;
    if (targets_position != post_iterator->targets.end()) {
        transition.target = *targets_position;
        return *this;
    }

    ++post_iterator;
    if (post_iterator != post[current_state].cend()) {
        targets_position = post_iterator->targets.begin();
        transition.symbol = post_iterator->symbol;
        transition.target = *targets_position;
        return *this;
    }

    ++current_state;
    while (current_state < post.size() && post[current_state].empty()) // skip empty posts
        current_state++;

    if (current_state >= post.size())
        is_end = true;
    else {
        post_iterator = post[current_state].begin();
        targets_position = post_iterator->targets.begin();
    }

    transition.source = current_state;
    transition.symbol = post_iterator->symbol;
    transition.target = *targets_position;

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
    this->post_iterator = x.post_iterator;
    this->targets_position = x.targets_position;
    this->current_state = x.current_state;
    this->is_end = x.is_end;
    transition.source = x.transition.source;
    transition.symbol = x.transition.symbol;
    transition.target = x.transition.target;
    return *this;
}

bool Mata::Nfa::Delta::transitions_const_iterator::operator==(const Delta::transitions_const_iterator& other) const {
    if (is_end && other.is_end) {
        return true;
    } else if ((is_end && !other.is_end) || (!is_end && other.is_end)) {
        return false;
    } else {
        return current_state == other.current_state && post_iterator == other.post_iterator
               && targets_position == other.targets_position;
    }
}

std::vector<StatePost> Delta::transform(const std::function<State(State)>& lambda) const {
    std::vector<StatePost> cp_post_vector;
    cp_post_vector.reserve(num_of_states());
    for(const StatePost& act_post: this->state_posts) {
        StatePost cp_post;
        cp_post.reserve(act_post.size());
        for(const SymbolPost& mv : act_post) {
            StateSet cp_dest;
            cp_dest.reserve(mv.size());
            for(const State& state : mv.targets) {
                cp_dest.push_back(std::move(lambda(state)));
            }
            cp_post.push_back(std::move(SymbolPost(mv.symbol, cp_dest)));
        }
        cp_post_vector.emplace_back(cp_post);
    }
    return cp_post_vector;
}

StatePost& Delta::mutable_state_post(State q) {
    if (q >= state_posts.size()) {
        Util::reserve_on_insert(state_posts, q);
        const size_t new_size{ q + 1 };
        state_posts.resize(new_size);
    }

    return state_posts[q];
}

void Delta::defragment(const BoolVector& is_staying, const std::vector<State>& renaming) {
    //TODO: this function seems to be unreadable, should be refactored, maybe into several functions with a clear functionality?

    //first, indexes of post are filtered (places of to be removed states are taken by states on their right)
    size_t move_index{ 0 };
    std::erase_if(state_posts,
         [&](StatePost&) -> bool {
             size_t prev{ move_index };
             ++move_index;
             return !is_staying[prev];
         }
    );

    //this iterates through every post and every move, filters and renames states,
    //and then removes moves that became empty.
    for (State q=0,size=state_posts.size(); q < size; ++q) {
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
