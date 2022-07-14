// Copyright 2007 The RE2 Authors.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef RE2_PROG_H_
#define RE2_PROG_H_

// Compiled representation of regular expressions.
// See regexp.h for the Regexp class, which represents a regular
// expression symbolically.

#include <stdint.h>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <type_traits>

#include "util/util.h"
#include "util/logging.h"
#include "re2/pod_array.h"
#include "re2/re2.h"
#include "re2/sparse_array.h"
#include "re2/sparse_set.h"

namespace re2 {

// Opcodes for Inst
enum InstOp {
  kInstAlt = 0,      // choose between out_ and out1_
  kInstAltMatch,     // Alt: out_ is [00-FF] and back, out1_ is match; or vice versa.
  kInstByteRange,    // next (possible case-folded) byte must be in [lo_, hi_]
  kInstCapture,      // capturing parenthesis number cap_
  kInstEmptyWidth,   // empty-width special (^ $ ...); bit(s) set in empty_
  kInstMatch,        // found a match!
  kInstNop,          // no-op; occasionally unavoidable
  kInstFail,         // never match; occasionally unavoidable
  kNumInst,
};

// Bit flags for empty-width specials
enum EmptyOp {
  kEmptyBeginLine        = 1<<0,      // ^ - beginning of line
  kEmptyEndLine          = 1<<1,      // $ - end of line
  kEmptyBeginText        = 1<<2,      // \A - beginning of text
  kEmptyEndText          = 1<<3,      // \z - end of text
  kEmptyWordBoundary     = 1<<4,      // \b - word boundary
  kEmptyNonWordBoundary  = 1<<5,      // \B - not \b
  kEmptyAllFlags         = (1<<6)-1,
};

class Regexp;

// Compiled form of regexp program.
class Prog {
 public:
  Prog();
  ~Prog();

  // Single instruction in regexp program.
  class Inst {
   public:
    // See the assertion below for why this is so.
    Inst() = default;

    // Copyable.
    Inst(const Inst&) = default;
    Inst& operator=(const Inst&) = default;

    // Constructors per opcode
    void InitAlt(uint32_t out, uint32_t out1);
    void InitByteRange(int lo, int hi, int foldcase, uint32_t out);
    void InitCapture(int cap, uint32_t out);
    void InitEmptyWidth(EmptyOp empty, uint32_t out);
    void InitMatch(int id);
    void InitNop(uint32_t out);
    void InitFail();

    // Getters
    int id(Prog* p) { return static_cast<int>(this - p->inst_.data()); }
    InstOp opcode() { return static_cast<InstOp>(out_opcode_&7); }
    int last()      { return (out_opcode_>>3)&1; }
    int out()       { return out_opcode_>>4; }
    int out1()      { DCHECK(opcode() == kInstAlt || opcode() == kInstAltMatch); return out1_; }
    int cap()       { DCHECK_EQ(opcode(), kInstCapture); return cap_; }
    int lo()        { DCHECK_EQ(opcode(), kInstByteRange); return lo_; }
    int hi()        { DCHECK_EQ(opcode(), kInstByteRange); return hi_; }
    int foldcase()  { DCHECK_EQ(opcode(), kInstByteRange); return hint_foldcase_&1; }
    int hint()      { DCHECK_EQ(opcode(), kInstByteRange); return hint_foldcase_>>1; }
    int match_id()  { DCHECK_EQ(opcode(), kInstMatch); return match_id_; }
    EmptyOp empty() { DCHECK_EQ(opcode(), kInstEmptyWidth); return empty_; }

    // Returns string representation for debugging.
    std::string Dump();

    // Maximum instruction id.
    // (Must fit in out_opcode_. PatchList/last steal another bit.)
    static const int kMaxInst = (1<<28) - 1;

   private:
    void set_opcode(InstOp opcode) {
      out_opcode_ = (out()<<4) | (last()<<3) | opcode;
    }

    void set_last() {
      out_opcode_ = (out()<<4) | (1<<3) | opcode();
    }

    void set_out(int out) {
      out_opcode_ = (out<<4) | (last()<<3) | opcode();
    }

    void set_out_opcode(int out, InstOp opcode) {
      out_opcode_ = (out<<4) | (last()<<3) | opcode;
    }

    uint32_t out_opcode_;  // 28 bits: out, 1 bit: last, 3 (low) bits: opcode
    union {                // additional instruction arguments:
      uint32_t out1_;      // opcode == kInstAlt
                           //   alternate next instruction

      int32_t cap_;        // opcode == kInstCapture
                           //   Index of capture register (holds text
                           //   position recorded by capturing parentheses).
                           //   For \n (the submatch for the nth parentheses),
                           //   the left parenthesis captures into register 2*n
                           //   and the right one captures into register 2*n+1.

      int32_t match_id_;   // opcode == kInstMatch
                           //   Match ID to identify this match (for re2::Set).

      struct {             // opcode == kInstByteRange
        uint8_t lo_;       //   byte range is lo_-hi_ inclusive
        uint8_t hi_;       //
        uint16_t hint_foldcase_;  // 15 bits: hint, 1 (low) bit: foldcase
                           //   hint to execution engines: the delta to the
                           //   next instruction (in the current list) worth
                           //   exploring iff this instruction matched; 0
                           //   means there are no remaining possibilities,
                           //   which is most likely for character classes.
                           //   foldcase: A-Z -> a-z before checking range.
      };

      EmptyOp empty_;       // opcode == kInstEmptyWidth
                            //   empty_ is bitwise OR of kEmpty* flags above.
    };

    friend class Compiler;
    friend struct PatchList;
    friend class Prog;
  };

  // Inst must be trivial so that we can freely clear it with memset(3).
  // Arrays of Inst are initialised by copying the initial elements with
  // memmove(3) and then clearing any remaining elements with memset(3).
  static_assert(std::is_trivial<Inst>::value, "Inst must be trivial");

  Inst *inst(int id) { return &inst_[id]; }
  int start() { return start_; }
  void set_start(int start) { start_ = start; }
  int start_unanchored() { return start_unanchored_; }
  void set_start_unanchored(int start) { start_unanchored_ = start; }
  int size() { return size_; }
  bool reversed() { return reversed_; }
  void set_reversed(bool reversed) { reversed_ = reversed; }
  void set_dfa_mem(int64_t dfa_mem) { dfa_mem_ = dfa_mem; }
  bool anchor_start() { return anchor_start_; }
  void set_anchor_start(bool b) { anchor_start_ = b; }
  void set_anchor_end(bool b) { anchor_end_ = b; }
  int bytemap_range() { return bytemap_range_; }

  // Configures prefix accel using the analysis performed during compilation.
  void ConfigurePrefixAccel(const std::string& prefix, bool prefix_foldcase);

  // Returns string representation of program for debugging.
  std::string Dump();

  // Returns whether byte c is a word character: ASCII only.
  // Used by the implementation of \b and \B.
  // This is not right for Unicode, but:
  //   - it's hard to get right in a byte-at-a-time matching world
  //     (the DFA has only one-byte lookahead).
  //   - even if the lookahead were possible, the Progs would be huge.
  // This crude approximation is the same one PCRE uses.
  static bool IsWordChar(uint8_t c) {
    return ('A' <= c && c <= 'Z') ||
           ('a' <= c && c <= 'z') ||
           ('0' <= c && c <= '9') ||
           c == '_';
  }

  // Compute bytemap.
  void ComputeByteMap();

  // Run peep-hole optimizer on program.
  void Optimize();

  // Bit-state backtracking.  Fast on small cases but uses memory
  // proportional to the product of the list count and the text size.
  bool CanBitState() { return list_heads_.data() != NULL; }

  // Flattens the Prog from "tree" form to "list" form. This is an in-place
  // operation in the sense that the old instructions are lost.
  void Flatten();

  // Walks the Prog; the "successor roots" or predecessors of the reachable
  // instructions are marked in rootmap or predmap/predvec, respectively.
  // reachable and stk are preallocated scratch structures.
  void MarkSuccessors(SparseArray<int>* rootmap,
                      SparseArray<int>* predmap,
                      std::vector<std::vector<int>>* predvec,
                      SparseSet* reachable, std::vector<int>* stk);

  // Walks the Prog from the given "root" instruction; the "dominator root"
  // of the reachable instructions (if such exists) is marked in rootmap.
  // reachable and stk are preallocated scratch structures.
  void MarkDominator(int root, SparseArray<int>* rootmap,
                     SparseArray<int>* predmap,
                     std::vector<std::vector<int>>* predvec,
                     SparseSet* reachable, std::vector<int>* stk);

  // Walks the Prog from the given "root" instruction; the reachable
  // instructions are emitted in "list" form and appended to flat.
  // reachable and stk are preallocated scratch structures.
  void EmitList(int root, SparseArray<int>* rootmap,
                std::vector<Inst>* flat,
                SparseSet* reachable, std::vector<int>* stk);

  // Computes hints for ByteRange instructions in [begin, end).
  void ComputeHints(std::vector<Inst>* flat, int begin, int end);

 private:
  friend class Compiler;

  bool anchor_start_;       // regexp has explicit start anchor
  bool anchor_end_;         // regexp has explicit end anchor
  bool reversed_;           // whether program runs backward over input
  bool did_flatten_;        // has Flatten been called?
  bool did_onepass_;        // has IsOnePass been called?

  int start_;               // entry point for program
  int start_unanchored_;    // unanchored entry point for program
  int size_;                // number of instructions
  int bytemap_range_;       // bytemap_[x] < bytemap_range_

  bool prefix_foldcase_;    // whether prefix is case-insensitive
  size_t prefix_size_;      // size of prefix (0 if no prefix)
  union {
    uint64_t* prefix_dfa_;  // "Shift DFA" for prefix
    struct {
      int prefix_front_;    // first byte of prefix
      int prefix_back_;     // last byte of prefix
    };
  };

  int list_count_;                  // count of lists (see above)
  int inst_count_[kNumInst];        // count of instructions by opcode
  PODArray<uint16_t> list_heads_;   // sparse array enumerating list heads
                                    // not populated if size_ is overly large
  size_t bit_state_text_max_size_;  // upper bound (inclusive) on text.size()

  PODArray<Inst> inst_;              // pointer to instruction array
  PODArray<uint8_t> onepass_nodes_;  // data for OnePass nodes

  int64_t dfa_mem_;         // Maximum memory for DFAs.

  uint8_t bytemap_[256];    // map from input bytes to byte classes


  Prog(const Prog&) = delete;
  Prog& operator=(const Prog&) = delete;
};

}  // namespace re2

#endif  // RE2_PROG_H_
