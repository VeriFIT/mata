/*
 * mintermization.cc -- Mintermization of automaton
 * It transforms an automaton with a bitvector formula used a symbol to minterminized version of the automaton.
 *
 * Copyright (c) 2022 Martin Hruska <hruskamartin25@gmail.com>
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

#ifndef _MATA_MINTERM_HH
#define _MATA_MINTERM_HH

#include <mata/inter-aut.hh>

namespace Mata
{

class Mintermization
{
    Mata::IntermediateAut mintermize(const Mata::IntermediateAut& aut);
};

}  // namespace Mata
#endif //_MATA_INTER_AUT_HH
