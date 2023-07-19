/* delta.cc -- Operations on Delta.
 *
 */
// TODO: Add file header.

#include "mata/utils/sparse-set.hh"
#include "mata/nfa/nfa.hh"

#include <algorithm>
#include <list>
#include <iterator>

using std::tie;

using namespace Mata::Util;
using namespace Mata::Nfa;
using Mata::Symbol;
using Mata::BoolVector;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

Move& Move::operator=(Move&& rhs) noexcept {
    if (*this != rhs) {
        symbol = rhs.symbol;
        targets = std::move(rhs.targets);
    }
    return *this;
}

void Move::insert(State s) {
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

void Move::insert(const StateSet& states) {
    for (State s : states) {
        insert(s);
    }
}

Post::const_iterator Nfa::Nfa::get_epsilon_transitions(const State state, const Symbol epsilon) const {
    assert(is_state(state));
    return get_epsilon_transitions(get_moves_from(state), epsilon);
}

Post::const_iterator Nfa::Nfa::get_epsilon_transitions(const Post& state_transitions, const Symbol epsilon) {
    if (!state_transitions.empty()) {
        if (epsilon == EPSILON) {
            const auto& back = state_transitions.back();
            if (back.symbol == epsilon) {
                return std::prev(state_transitions.end());
            }
        } else {
            return state_transitions.find(Move(epsilon));
        }
    }

    return state_transitions.end();
}

size_t Delta::size() const
{
    size_t size = 0;
    for (State q = 0; q < num_of_states(); ++q)
        for (const Move & m: (*this)[q])
            size = size + m.size();

    return size;
}

void Delta::add(State state_from, Symbol symbol, State state_to) {
    const State max_state{ std::max(state_from, state_to) };
    if (max_state >= posts.size()) {
        reserve_on_insert(posts, max_state);
        posts.resize(max_state + 1);
    }

    Post& state_transitions{ posts[state_from] };

    if (state_transitions.empty()) {
        state_transitions.insert({ symbol, state_to });
    } else if (state_transitions.back().symbol < symbol) {
        state_transitions.insert({ symbol, state_to });
    } else {
        const auto symbol_transitions{ state_transitions.find(Move{ symbol }) };
        if (symbol_transitions != state_transitions.end()) {
            // Add transition with symbol already used on transitions from state_from.
            symbol_transitions->insert(state_to);
        } else {
            // Add transition to a new Move struct with symbol yet unused on transitions from state_from.
            const Move new_symbol_transitions{ symbol, state_to };
            state_transitions.insert(new_symbol_transitions);
        }
    }
}

void Delta::add(const State state_from, const Symbol symbol, const StateSet& states) {
    if(states.empty()) {
        return;
    }

    const State max_state{ std::max(state_from, states.back()) };
    if (max_state >= posts.size()) {
        reserve_on_insert(posts, max_state + 1);
        posts.resize(max_state + 1);
    }

    Post& state_transitions{ posts[state_from] };

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
            state_transitions.insert(Move{symbol, states});
        }
    }
}

void Delta::remove(State src, Symbol symb, State tgt) {
    if (src >= posts.size()) {
        return;
    }

    Post& state_transitions{ posts[src] };
    if (state_transitions.empty()) {
        throw std::invalid_argument(
                "Transition [" + std::to_string(src) + ", " + std::to_string(symb) + ", " +
                std::to_string(tgt) + "] does not exist.");
    } else if (state_transitions.back().symbol < symb) {
        throw std::invalid_argument(
                "Transition [" + std::to_string(src) + ", " + std::to_string(symb) + ", " +
                std::to_string(tgt) + "] does not exist.");
    } else {
        const auto symbol_transitions{ state_transitions.find(Move{symb }) };
        if (symbol_transitions == state_transitions.end()) {
            throw std::invalid_argument(
                    "Transition [" + std::to_string(src) + ", " + std::to_string(symb) + ", " +
                    std::to_string(tgt) + "] does not exist.");
        } else {
            symbol_transitions->remove(tgt);
            if (symbol_transitions->empty()) {
                posts[src].remove(*symbol_transitions);
            }
        }
    }
}

bool Delta::contains(State src, Symbol symb, State tgt) const
{ // {{{
    if (posts.empty()) {
        return false;
    }

    if (posts.size() <= src)
        return false;

    const Post& tl = posts[src];
    if (tl.empty()) {
        return false;
    }
    auto symbol_transitions{ tl.find(Move{symb} ) };
    if (symbol_transitions == tl.cend()) {
        return false;
    }

    return symbol_transitions->targets.find(tgt) != symbol_transitions->targets.end();
}

bool Delta::empty() const
{
    return this->begin() == this->end();
}

Delta::const_iterator::const_iterator(const std::vector<Post>& post_p, bool ise) :
    post(post_p), current_state(0), is_end{ ise }
{
    const size_t post_size = post.size();
    for (size_t i = 0; i < post_size; ++i) {
        if (!post[i].empty()) {
            current_state = i;
            post_iterator = post[i].begin();
            targets_position = post_iterator->targets.begin();
            return;
        }
    }

    // no transition found, an empty post
    is_end = true;
}

Delta::const_iterator& Delta::const_iterator::operator++()
{
    assert(post.begin() != post.end());

    ++targets_position;
    if (targets_position != post_iterator->targets.end())
        return *this;

    ++post_iterator;
    if (post_iterator != post[current_state].cend()) {
        targets_position = post_iterator->targets.begin();
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

    return *this;
}

Delta::const_iterator Delta::const_iterator::operator++(int) {
    const const_iterator tmp = *this;
    ++(*this);
    return tmp;
}

Delta::const_iterator& Delta::const_iterator::operator=(const Delta::const_iterator& x) {
    this->post_iterator = x.post_iterator;
    this->targets_position = x.targets_position;
    this->current_state = x.current_state;
    this->is_end = x.is_end;

    return *this;
}

bool Mata::Nfa::operator==(const Delta::const_iterator& a, const Delta::const_iterator& b) {
    if (a.is_end && b.is_end) {
        return true;
    } else if ((a.is_end && !b.is_end) || (!a.is_end && b.is_end)) {
        return false;
    } else {
        return a.current_state == b.current_state && a.post_iterator == b.post_iterator
               && a.targets_position == b.targets_position;
    }
}

State Delta::find_max_state() {
    size_t max = 0;
    State src = 0;
    for (Post & p: posts) {
        if (src > max)
            max = src;
        for (Move & m: p) {
            if (!m.targets.empty())
                if (m.targets.back() > max)
                    max = m.targets.back();
        }
        src++;
    }
    return max;
}

std::vector<Post> Delta::transform(const std::function<State(State)>& lambda) const {
    std::vector<Post> cp_post_vector;
    cp_post_vector.reserve(num_of_states());
    for(const Post& act_post: this->posts) {
        Post cp_post;
        cp_post.reserve(act_post.size());
        for(const Move& mv : act_post) {
            StateSet cp_dest;
            cp_dest.reserve(mv.size());
            for(const State& state : mv.targets) {
                cp_dest.push_back(std::move(lambda(state)));
            }
            cp_post.push_back(std::move(Move(mv.symbol, cp_dest)));
        }
        cp_post_vector.emplace_back(cp_post);
    }
    return cp_post_vector;
}

Post& Delta::get_mutable_post(State q) {
    if (q >= posts.size()) {
        Util::reserve_on_insert(posts, q);
        const size_t new_size{ q + 1 };
        posts.resize(new_size);
    }

    return posts[q];
}

void Delta::defragment(const BoolVector& is_staying, const std::vector<State>& renaming) {
    //TODO: this function seems to be unreadable, should be refactored, maybe into several functions with a clear functionality?

    //first, indexes of post are filtered (places of to be removed states are taken by states on their right)
    size_t move_index{ 0 };
    std::erase_if(posts,
         [&](Post&) -> bool {
             size_t prev{ move_index };
             ++move_index;
             return !is_staying[prev];
         }
    );

    //this iterates through every post and every move, filters and renames states,
    //and then removes moves that became empty.
    for (State q=0,size=posts.size(); q < size; ++q) {
        Post & p = get_mutable_post(q);
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
                std::remove_if(p.begin(), p.end(), [&](Move& move) -> bool {
                    return move.targets.empty();
                }),
                p.end()
        );
    }
}

const Post& Delta::operator[](State q) const {
    if (q >= posts.size()) {
        return empty_post;
    }
    return posts[q];
}
