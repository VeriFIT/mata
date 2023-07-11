/* move.cc -- Operations on Move.
 *
 */
// TODO: Add file header.

#include "mata/utils/sparse-set.hh"
#include "mata/nfa/nfa.hh"

#include <list>

using namespace Mata::Nfa;

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
