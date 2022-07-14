// Copyright 2004 The RE2 Authors.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "re2/stringpiece.h"

#include <ostream>


namespace re2 {

const StringPiece::size_type StringPiece::npos;  // initialized in stringpiece.h

StringPiece::size_type StringPiece::copy(char* buf, size_type n,
                                         size_type pos) const {
  size_type ret = std::min(size_ - pos, n);
  memcpy(buf, data_ + pos, ret);
  return ret;
}

StringPiece::size_type StringPiece::find(const StringPiece& s,
                                         size_type pos) const {
  if (pos > size_) return npos;
  const_pointer result = std::search(data_ + pos, data_ + size_,
                                     s.data_, s.data_ + s.size_);
  size_type xpos = result - data_;
  return xpos + s.size_ <= size_ ? xpos : npos;
}

StringPiece::size_type StringPiece::find(char c, size_type pos) const {
  if (size_ <= 0 || pos >= size_) return npos;
  const_pointer result = std::find(data_ + pos, data_ + size_, c);
  return result != data_ + size_ ? result - data_ : npos;
}

std::ostream& operator<<(std::ostream& o, const StringPiece& p) {
  o.write(p.data(), p.size());
  return o;
}

}  // namespace re2
