// TODO: Insert file header.

#ifndef MATA_POST_HH
#define MATA_POST_HH

#include "move.hh"

namespace Mata::Nfa {

/**
 * Post is a data structure representing possible transitions over different symbols.
 * It is an ordered vector containing possible Moves (i.e., pair of symbol and target states.
 * Vector is ordered by symbols which are numbers.
 */
struct Post : private Util::OrdVector<Move> {
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

} // namespace Mata::Nfa.

#endif //MATA_POST_HH
