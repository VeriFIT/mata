/* rrt.cc -- operations for RRTs
 *
 * Copyright (c) 2020 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libvata2.
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

#include <vata2/rrt.hh>

void Vata2::Rrt::Rrt::add_trans(
		State                 src,
		const Trans::Label&   lbl,
		State                 tgt)
{ // {{{
  auto iter_inserted = this->transitions.insert({src, { {lbl, tgt} } });
  if (!iter_inserted.second) {
    // TODO: check whether the transition is not already there
    PostSymb& post = iter_inserted.first->second;
    post.push_back({lbl, tgt});
  }
	assert(false);



} // add_trans }}}
