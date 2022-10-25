/* nfa-internals.hh -- Wrapping up algorithms for Nfa manipulation which would be otherwise in anonymous namespaces
 *
 * Copyright (c) 2022
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MATA_NFA_INTERNALS_HH_
#define MATA_NFA_INTERNALS_HH_

#include <mata/nfa.hh>

namespace Mata {

namespace Nfa {

/**
 * The following namespace contains methods which would be otherwise in anonymous namespace
 * to make them accessible to users of library. Typically, that are different algorithms for
 * operations such as complement, inclusion, or universality checking.
 * In Nfa interface, there are dispatch functions calling these function according to parameters
 * provided by an user.
 */
namespace Internals {

    void complement_classical(
            Nfa*               result,
            const Nfa&         aut,
            const Alphabet&    alphabet,
            SubsetMap*         subset_map);

    void complement_naive(
            Nfa*               result,
            const Nfa&         aut,
            const Alphabet&    alphabet,
            const StringDict&  params,
            SubsetMap*         subset_map);

    bool is_incl_naive(
            const Nfa&             smaller,
            const Nfa&             bigger,
            const Alphabet* const  alphabet,
            Word*                  cex,
            const StringDict&  /* params*/);

    bool is_incl_antichains(
            const Nfa&             smaller,
            const Nfa&             bigger,
            const Alphabet* const  alphabet,
            Word*                  cex,
            const StringDict&      params);

    bool is_universal_naive(
            const Nfa&         aut,
            const Alphabet&    alphabet,
            Word*              cex,
            const StringDict&  /* params*/);

    bool is_universal_antichains(
            const Nfa&         aut,
            const Alphabet&    alphabet,
            Word*              cex,
            const StringDict&  params);
} // Internals
} // Nfa
}

#endif // MATA_NFA_INTERNALS_HH
