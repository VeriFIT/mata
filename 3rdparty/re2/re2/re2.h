// Copyright 2003-2009 The RE2 Authors.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef RE2_RE2_H_
#define RE2_RE2_H_

// C++ interface to the re2 regular-expression library.
// RE2 supports Perl-style regular expressions (with extensions like
// \d, \w, \s, ...).
//
// -----------------------------------------------------------------------
// REGEXP SYNTAX:
//
// This module uses the re2 library and hence supports
// its syntax for regular expressions, which is similar to Perl's with
// some of the more complicated things thrown away.  In particular,
// backreferences and generalized assertions are not available, nor is \Z.
//
// See https://github.com/google/re2/wiki/Syntax for the syntax
// supported by RE2, and a comparison with PCRE and PERL regexps.
//
// For those not familiar with Perl's regular expressions,
// here are some examples of the most commonly used extensions:
//
//   "hello (\\w+) world"  -- \w matches a "word" character
//   "version (\\d+)"      -- \d matches a digit
//   "hello\\s+world"      -- \s matches any whitespace character
//   "\\b(\\w+)\\b"        -- \b matches non-empty string at word boundary
//   "(?i)hello"           -- (?i) turns on case-insensitive matching
//   "/\\*(.*?)\\*/"       -- .*? matches . minimum no. of times possible
//
// The double backslashes are needed when writing C++ string literals.
// However, they should NOT be used when writing C++11 raw string literals:
//
//   R"(hello (\w+) world)"  -- \w matches a "word" character
//   R"(version (\d+))"      -- \d matches a digit
//   R"(hello\s+world)"      -- \s matches any whitespace character
//   R"(\b(\w+)\b)"          -- \b matches non-empty string at word boundary
//   R"((?i)hello)"          -- (?i) turns on case-insensitive matching
//   R"(/\*(.*?)\*/)"        -- .*? matches . minimum no. of times possible
//
// When using UTF-8 encoding, case-insensitive matching will perform
// simple case folding, not full case folding.
//
// -----------------------------------------------------------------------
// MATCHING INTERFACE:
//
// The "FullMatch" operation checks that supplied text matches a
// supplied pattern exactly.
//
// Example: successful match
//    CHECK(RE2::FullMatch("hello", "h.*o"));
//
// Example: unsuccessful match (requires full match):
//    CHECK(!RE2::FullMatch("hello", "e"));
//
// -----------------------------------------------------------------------
// UTF-8 AND THE MATCHING INTERFACE:
//
// By default, the pattern and input text are interpreted as UTF-8.
// The RE2::Latin1 option causes them to be interpreted as Latin-1.
//
// Example:
//    CHECK(RE2::FullMatch(utf8_string, RE2(utf8_pattern)));
//    CHECK(RE2::FullMatch(latin1_string, RE2(latin1_pattern, RE2::Latin1)));
//
// -----------------------------------------------------------------------
// MATCHING WITH SUBSTRING EXTRACTION:
//
// You can supply extra pointer arguments to extract matched substrings.
// On match failure, none of the pointees will have been modified.
// On match success, the substrings will be converted (as necessary) and
// their values will be assigned to their pointees until all conversions
// have succeeded or one conversion has failed.
// On conversion failure, the pointees will be in an indeterminate state
// because the caller has no way of knowing which conversion failed.
// However, conversion cannot fail for types like string and StringPiece
// that do not inspect the substring contents. Hence, in the common case
// where all of the pointees are of such types, failure is always due to
// match failure and thus none of the pointees will have been modified.
//
// Example: extracts "ruby" into "s" and 1234 into "i"
//    int i;
//    std::string s;
//    CHECK(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", &s, &i));
//
// Example: fails because string cannot be stored in integer
//    CHECK(!RE2::FullMatch("ruby", "(.*)", &i));
//
// Example: fails because there aren't enough sub-patterns
//    CHECK(!RE2::FullMatch("ruby:1234", "\\w+:\\d+", &s));
//
// Example: does not try to extract any extra sub-patterns
//    CHECK(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", &s));
//
// Example: does not try to extract into NULL
//    CHECK(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", NULL, &i));
//
// Example: integer overflow causes failure
//    CHECK(!RE2::FullMatch("ruby:1234567891234", "\\w+:(\\d+)", &i));
//
// NOTE(rsc): Asking for substrings slows successful matches quite a bit.
// This may get a little faster in the future, but right now is slower
// than PCRE.  On the other hand, failed matches run *very* fast (faster
// than PCRE), as do matches without substring extraction.
//
// -----------------------------------------------------------------------
// PARTIAL MATCHES
//
// You can use the "PartialMatch" operation when you want the pattern
// to match any substring of the text.
//
// Example: simple search for a string:
//      CHECK(RE2::PartialMatch("hello", "ell"));
//
// Example: find first number in a string
//      int number;
//      CHECK(RE2::PartialMatch("x*100 + 20", "(\\d+)", &number));
//      CHECK_EQ(number, 100);
//
// -----------------------------------------------------------------------
// PRE-COMPILED REGULAR EXPRESSIONS
//
// RE2 makes it easy to use any string as a regular expression, without
// requiring a separate compilation step.
//
// If speed is of the essence, you can create a pre-compiled "RE2"
// object from the pattern and use it multiple times.  If you do so,
// you can typically parse text faster than with sscanf.
//
// Example: precompile pattern for faster matching:
//    RE2 pattern("h.*o");
//    while (ReadLine(&str)) {
//      if (RE2::FullMatch(str, pattern)) ...;
//    }
//
// -----------------------------------------------------------------------
// SCANNING TEXT INCREMENTALLY
//
// The "Consume" operation may be useful if you want to repeatedly
// match regular expressions at the front of a string and skip over
// them as they match.  This requires use of the "StringPiece" type,
// which represents a sub-range of a real string.
//
// Example: read lines of the form "var = value" from a string.
//      std::string contents = ...;     // Fill string somehow
//      StringPiece input(contents);    // Wrap a StringPiece around it
//
//      std::string var;
//      int value;
//      while (RE2::Consume(&input, "(\\w+) = (\\d+)\n", &var, &value)) {
//        ...;
//      }
//
// Each successful call to "Consume" will set "var/value", and also
// advance "input" so it points past the matched text.  Note that if the
// regular expression matches an empty string, input will advance
// by 0 bytes.  If the regular expression being used might match
// an empty string, the loop body must check for this case and either
// advance the string or break out of the loop.
//
// The "FindAndConsume" operation is similar to "Consume" but does not
// anchor your match at the beginning of the string.  For example, you
// could extract all words from a string by repeatedly calling
//     RE2::FindAndConsume(&input, "(\\w+)", &word)
//
// -----------------------------------------------------------------------
// USING VARIABLE NUMBER OF ARGUMENTS
//
// The above operations require you to know the number of arguments
// when you write the code.  This is not always possible or easy (for
// example, the regular expression may be calculated at run time).
// You can use the "N" version of the operations when the number of
// match arguments are determined at run time.
//
// Example:
//   const RE2::Arg* args[10];
//   int n;
//   // ... populate args with pointers to RE2::Arg values ...
//   // ... set n to the number of RE2::Arg objects ...
//   bool match = RE2::FullMatchN(input, pattern, args, n);
//
// The last statement is equivalent to
//
//   bool match = RE2::FullMatch(input, pattern,
//                               *args[0], *args[1], ..., *args[n - 1]);
//
// -----------------------------------------------------------------------
// PARSING HEX/OCTAL/C-RADIX NUMBERS
//
// By default, if you pass a pointer to a numeric value, the
// corresponding text is interpreted as a base-10 number.  You can
// instead wrap the pointer with a call to one of the operators Hex(),
// Octal(), or CRadix() to interpret the text in another base.  The
// CRadix operator interprets C-style "0" (base-8) and "0x" (base-16)
// prefixes, but defaults to base-10.
//
// Example:
//   int a, b, c, d;
//   CHECK(RE2::FullMatch("100 40 0100 0x40", "(.*) (.*) (.*) (.*)",
//         RE2::Octal(&a), RE2::Hex(&b), RE2::CRadix(&c), RE2::CRadix(&d));
// will leave 64 in a, b, c, and d.

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include "re2/stringpiece.h"

namespace re2 {
class Prog;
class Regexp;
}  // namespace re2

namespace re2 {

// Interface for regular expression matching.  Also corresponds to a
// pre-compiled regular expression.  An "RE2" object is safe for
// concurrent use by multiple threads.
class RE2 {
 public:
  // We convert user-passed pointers into special Arg objects
  class Options;

  RE2() = default;
  ~RE2();

  // If RE2 could not be created properly, returns an error string.
  // Else returns the empty string.
  const std::string& error() const { return *error_; }

  // Returns the underlying Regexp; not for general use.
  // Returns entire_regexp_ so that callers don't need
  // to know about prefix_ and prefix_foldcase_.
  re2::Regexp* Regexp() const { return entire_regexp_; }

  // Generic matching interface

  // Type of match.
  enum Anchor {
    UNANCHORED,         // No anchoring
    ANCHOR_START,       // Anchor at start only
    ANCHOR_BOTH         // Anchor at start and end
  };

  // Constructor options
  class Options {
   public:
    // The options are (defaults in parentheses):
    //
    //   utf8             (true)  text and pattern are UTF-8; otherwise Latin-1
    //   posix_syntax     (false) restrict regexps to POSIX egrep syntax
    //   longest_match    (false) search for longest match, not first match
    //   log_errors       (true)  log syntax and execution errors to ERROR
    //   max_mem          (see below)  approx. max memory footprint of RE2
    //   literal          (false) interpret string as literal, not regexp
    //   never_nl         (false) never match \n, even if it is in regexp
    //   dot_nl           (false) dot matches everything including new line
    //   never_capture    (false) parse all parens as non-capturing
    //   case_sensitive   (true)  match is case-sensitive (regexp can override
    //                              with (?i) unless in posix_syntax mode)
    //
    // The following options are only consulted when posix_syntax == true.
    // When posix_syntax == false, these features are always enabled and
    // cannot be turned off; to perform multi-line matching in that case,
    // begin the regexp with (?m).
    //   perl_classes     (false) allow Perl's \d \s \w \D \S \W
    //   word_boundary    (false) allow Perl's \b \B (word boundary and not)
    //   one_line         (false) ^ and $ only match beginning and end of text
    //
    // The max_mem option controls how much memory can be used
    // to hold the compiled form of the regexp (the Prog) and
    // its cached DFA graphs.  Code Search placed limits on the number
    // of Prog instructions and DFA states: 10,000 for both.
    // In RE2, those limits would translate to about 240 KB per Prog
    // and perhaps 2.5 MB per DFA (DFA state sizes vary by regexp; RE2 does a
    // better job of keeping them small than Code Search did).
    // Each RE2 has two Progs (one forward, one reverse), and each Prog
    // can have two DFAs (one first match, one longest match).
    // That makes 4 DFAs:
    //
    //   forward, first-match    - used for UNANCHORED or ANCHOR_START searches
    //                               if opt.longest_match() == false
    //   forward, longest-match  - used for all ANCHOR_BOTH searches,
    //                               and the other two kinds if
    //                               opt.longest_match() == true
    //   reverse, first-match    - never used
    //   reverse, longest-match  - used as second phase for unanchored searches
    //
    // The RE2 memory budget is statically divided between the two
    // Progs and then the DFAs: two thirds to the forward Prog
    // and one third to the reverse Prog.  The forward Prog gives half
    // of what it has left over to each of its DFAs.  The reverse Prog
    // gives it all to its longest-match DFA.
    //
    // Once a DFA fills its budget, it flushes its cache and starts over.
    // If this happens too often, RE2 falls back on the NFA implementation.

    // For now, make the default budget something close to Code Search.
    static const int kDefaultMaxMem = 8<<20;

    enum Encoding {
      EncodingUTF8 = 1,
      EncodingLatin1
    };

    Options() :
      encoding_(EncodingUTF8),
      posix_syntax_(false),
      log_errors_(true),
      max_mem_(kDefaultMaxMem),
      literal_(false),
      never_nl_(false),
      dot_nl_(false),
      never_capture_(false),
      case_sensitive_(true),
      perl_classes_(false),
      word_boundary_(false),
      one_line_(false) {
    }

    Encoding encoding() const { return encoding_; }

    bool posix_syntax() const { return posix_syntax_; }

    bool log_errors() const { return log_errors_; }

    int64_t max_mem() const { return max_mem_; }

    bool literal() const { return literal_; }

    bool never_nl() const { return never_nl_; }

    bool dot_nl() const { return dot_nl_; }

    bool never_capture() const { return never_capture_; }

    bool case_sensitive() const { return case_sensitive_; }

    bool perl_classes() const { return perl_classes_; }

    bool word_boundary() const { return word_boundary_; }

    bool one_line() const { return one_line_; }


    int ParseFlags() const;

   private:
    Encoding encoding_;
    bool posix_syntax_;
    bool log_errors_;
    int64_t max_mem_;
    bool literal_;
    bool never_nl_;
    bool dot_nl_;
    bool never_capture_;
    bool case_sensitive_;
    bool perl_classes_;
    bool word_boundary_;
    bool one_line_;
  };
 private:

  std::string pattern_;         // string regular expression
  re2::Regexp* entire_regexp_{};  // parsed regular expression
  const std::string* error_{};    // error indicator (or points to empty string)
  std::string error_arg_;       // fragment of regexp showing error
  std::string prefix_;          // required prefix (before suffix_regexp_)
  re2::Regexp* suffix_regexp_{};  // parsed regular expression, prefix_ removed
  re2::Prog* prog_{};             // compiled program for regexp

  // Reverse Prog for DFA execution only
  mutable re2::Prog* rprog_{};
  // Map from capture names to indices
  mutable const std::map<std::string, int>* named_groups_{};
  // Map from capture indices to names
  mutable const std::map<int, std::string>* group_names_{};

  RE2(const RE2&) = delete;
  RE2& operator=(const RE2&) = delete;
};

}  // namespace re2

using re2::RE2;

#endif  // RE2_RE2_H_
