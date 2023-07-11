/* post.cc -- Operations on Post.
 *
 */
// TODO: Add file header.

#include "mata/utils/sparse-set.hh"
#include "mata/nfa/nfa.hh"

#include <list>

using namespace Mata::Nfa;

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
