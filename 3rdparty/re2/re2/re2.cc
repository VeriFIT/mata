// Copyright 2003-2009 The RE2 Authors.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Regular expression interface RE2.
//
// Originally the PCRE C++ wrapper, but adapted to use
// the new automata-based regular expression engines.

#include "re2/re2.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <stdint.h>
#include <string>
#include <vector>

#include "util/logging.h"
#include "re2/prog.h"
#include "re2/regexp.h"
#include "re2/sparse_array.h"

namespace re2 {

const int RE2::Options::kDefaultMaxMem;  // initialized in re2.h

// static empty objects for use as const references.
// To avoid global constructors, allocated in RE2::Init().
static const std::string* empty_string;
static const std::map<std::string, int>* empty_named_groups;
static const std::map<int, std::string>* empty_group_names;

int RE2::Options::ParseFlags() const {
  int flags = Regexp::ClassNL;
  switch (encoding()) {
    default:
      if (log_errors())
        LOG(ERROR) << "Unknown encoding " << encoding();
      break;
    case RE2::Options::EncodingUTF8:
      break;
    case RE2::Options::EncodingLatin1:
      flags |= Regexp::Latin1;
      break;
  }

  if (!posix_syntax())
    flags |= Regexp::LikePerl;

  if (literal())
    flags |= Regexp::Literal;

  if (never_nl())
    flags |= Regexp::NeverNL;

  if (dot_nl())
    flags |= Regexp::DotNL;

  if (never_capture())
    flags |= Regexp::NeverCapture;

  if (!case_sensitive())
    flags |= Regexp::FoldCase;

  if (perl_classes())
    flags |= Regexp::PerlClasses;

  if (word_boundary())
    flags |= Regexp::PerlB;

  if (one_line())
    flags |= Regexp::OneLine;

  return flags;
}

RE2::~RE2() {
  if (suffix_regexp_)
    suffix_regexp_->Decref();
  if (entire_regexp_)
    entire_regexp_->Decref();
  delete prog_;
  delete rprog_;
  if (error_ != empty_string)
    delete error_;
  if (named_groups_ != NULL && named_groups_ != empty_named_groups)
    delete named_groups_;
  if (group_names_ != NULL &&  group_names_ != empty_group_names)
    delete group_names_;
}

}  // namespace re2
