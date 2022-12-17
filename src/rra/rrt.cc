/* rrt.cc -- operations for RRTs
 *
 * Copyright (c) 2020 Ondrej Lengal <ondra.lengal@gmail.com>
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

#include <mata/rrt.hh>

using Mata::Nfa::Nfa;

bool Mata::Rrt::Trans::Guard::operator==(const Guard& rhs) const
{ // {{{
  if (this->type != rhs.type) return false;
  switch (this->type)
  {
    case GuardType::IN1_VAR:
    case GuardType::IN2_VAR:
    case GuardType::INS_EQ:
    case GuardType::INS_NEQ: return true;

    case GuardType::IN1_EQ:
    case GuardType::IN2_EQ:
    case GuardType::IN1_NEQ:
    case GuardType::IN2_NEQ:
    case GuardType::IN1_IS:
    case GuardType::IN2_IS:
    case GuardType::IN1_ISNOT:
    case GuardType::IN2_ISNOT: return this->val == rhs.val;

    default: assert(false);
  }
} // Guard::operator== }}}

bool Mata::Rrt::Trans::Output::operator==(const Output& rhs) const
{ // {{{
  if (this->type != rhs.type) return false;
  switch (this->type) {
    case OutputType::PUT_REG:
    case OutputType::PUT_AUX: return this->val == rhs.val;
    case OutputType::PUT_IN1:
    case OutputType::PUT_IN2: return true;
    default: assert(false);
  }
} // Output::operator== }}}

bool Mata::Rrt::Trans::Update::operator==(const Update& rhs) const
{ // {{{
  if (this->type != rhs.type) return false;
  switch (this->type) {
    case UpdateType::REG_STORE_IN1:
    case UpdateType::REG_STORE_IN2:
    case UpdateType::AUX_STORE_IN1:
    case UpdateType::AUX_STORE_IN2:
    case UpdateType::REG_CLEAR:
    case UpdateType::AUX_CLEAR: return this->val == rhs.val;
    default: assert(false);
  }
} // Update::operator== }}}

bool Mata::Rrt::Trans::Label::operator==(const Label& rhs) const
{ // {{{
  if ((this->out1 != rhs.out1) && (this->out2 != rhs.out2)) return false;
  // FIXME: this is probably not optimal
  for (const Guard& grd : this->guards) {
    if (!Util::is_in(grd, rhs.guards)) return false;
  }
  for (const Guard& grd : rhs.guards) {
    if (!Util::is_in(grd, this->guards)) return false;
  }

  for (const Update& upd : this->updates) {
    if (!Util::is_in(upd, rhs.updates)) return false;
  }
  for (const Update& upd : rhs.updates) {
    if (!Util::is_in(upd, this->updates)) return false;
  }
  return true;
} // Label::operator== }}}

void Mata::Rrt::Rrt::add_trans(
		State                 src,
		const Trans::Label&   lbl,
		State                 tgt)
{ // {{{
  auto it_inserted = this->transitions.insert({src, { {lbl, tgt} } });
  if (!it_inserted.second) {
    // TODO: check whether the transition is not already there
    PostSymb& post = it_inserted.first->second;
    post.push_back({lbl, tgt});
  }
} // add_trans }}}


bool Mata::Rrt::Rrt::has_trans(
  State                 src,
  const Trans::Label&   lbl,
  State                 tgt)
{ // {{{
  auto it = this->transitions.find(src);
  if (it == this->transitions.end()) return false;
  for (auto lbl_state : it->second) {
    if (Trans(src, lbl_state.first, lbl_state.second) == Trans(src, lbl, tgt)) return true;
  }

  return false;
} // has_trans }}}


Mata::Nfa::Nfa Mata::Rrt::post_of_nfa(const Rrt& rrt, const Mata::Nfa::Nfa& nfa)
{ // {{{
  assert(false);
} // post_of_nfa }}}
