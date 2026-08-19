from libcpp cimport bool
from libcpp.optional cimport optional
from libcpp.set cimport set as cset
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector
from libcpp.memory cimport shared_ptr
from libcpp.string cimport string
from libcpp.utility cimport pair
from libc.stdint cimport uintptr_t

from libmata.utils cimport CSparseSet, COrdVector, CBoolVector, CBinaryRelation
from libmata.alphabets cimport CAlphabet, CConstAlphabet, CAlphabetLevels, Symbol, Level

from libmata.nfa.nfa cimport (
    State, StateSet, StateRenaming, ParameterMap,
    CDelta, CRun, CTrans, CNfa, CSymbolPost, CEPSILON,
    ostream, ofstream, stringstream,
)

cdef extern from "mata/nft/types.hh" namespace "mata::nft":
    cdef enum CJumpMode "mata::nft::JumpMode":
        JumpModeRepeatSymbol "mata::nft::JumpMode::RepeatSymbol"
        JumpModeAppendDontCares "mata::nft::JumpMode::AppendDontCares"
        JumpModeNoJump "mata::nft::JumpMode::NoJump"

    cdef enum CCompositionMode "mata::nft::CompositionMode":
        CompositionModeGeneral "mata::nft::CompositionMode::General"
        CompositionModeFastNoJump "mata::nft::CompositionMode::FastNoJump"
        CompositionModeAuto "mata::nft::CompositionMode::Auto"

    cdef const Symbol CNFT_EPSILON "mata::nft::EPSILON"
    cdef const Symbol CDONT_CARE "mata::nft::DONT_CARE"
    cdef const size_t CDEFAULT_NUM_OF_LEVELS "mata::nft::DEFAULT_NUM_OF_LEVELS"

cdef extern from "mata/nfa/types.hh" namespace "mata::nfa::Limits":
    cdef const State CLIMITS_MAX_STATE "mata::nfa::Limits::max_state"

    cdef cppclass CLevels "mata::nft::Levels":
        size_t num_of_levels

        CLevels() except +
        CLevels(vector[Level]&) except +
        CLevels(size_t) except +
        CLevels(size_t, vector[Level]) except +
        CLevels(size_t, size_t, Level) except +
        CLevels(CLevels&) except +

        bool operator==(CLevels&)
        Level& operator[](size_t) except +
        Level& at(size_t) except +
        size_t size()
        bool empty()
        void push_back(Level)
        void resize(size_t)
        void clear()
        vector[Level].iterator begin()
        vector[Level].iterator end()

        CLevels& set(State, Level) except +
        CLevels& set(vector[Level]&) except +
        void append(CLevels&) except +
        size_t count(Level)
        vector[Level] get_levels_of(StateSet&)
        Level next_level_after(Level)
        optional[Level] get_minimal_level_of(StateSet&) except +
        optional[Level] get_minimal_next_level_of(StateSet&) except +
        bool can_follow_for_states(State, State)
        optional[size_t] num_of_levels_used()


cdef extern from "mata/nft/delta.hh" namespace "mata::nft":
    ctypedef CDelta CNftDelta
    ctypedef CTrans CNftTransition
    ctypedef CSymbolPost CNftSymbolPost


cdef extern from "mata/nft/nft.hh" namespace "mata::nft":
    cdef cppclass CNft "mata::nft::Nft" (CNfa):
        # Public attributes.
        CLevels levels
        shared_ptr[CAlphabetLevels] alphabets

        # Constructors.
        CNft() except +
        CNft(CDelta, CSparseSet[State], CSparseSet[State], CLevels, shared_ptr[CAlphabetLevels]) except +
        CNft(size_t, CSparseSet[State], CSparseSet[State], CLevels, shared_ptr[CAlphabetLevels]) except +
        CNft(CNft&) except +
        CNft(CNfa&, size_t, Level) except +
        CNft(CNfa&, CLevels) except +

        # State/transition construction.
        # add_state()/add_state(State) are not redeclared here: they are inherited from CNfa's identical signature
        # (Cython would otherwise flag calls as ambiguous); Nft's own override is still dispatched to at the C++
        # level via name hiding, since it is called through a CNft-typed pointer.
        State add_state_with_level(Level) except +
        State add_state_with_level(State, Level) except +
        size_t num_of_states_with_level(Level)
        State insert_word(State, vector[Symbol]&, State) except +
        State insert_word(State, vector[Symbol]&) except +
        State add_transition(State, vector[Symbol]&, State) except +
        State add_transition(State, vector[Symbol]&) except +
        State add_transition_with_length(State, Symbol, size_t, CJumpMode) except +
        void add_transition_with_target(State, Symbol, State, CJumpMode) except +
        void add_transition_with_same_level_targets(State, Symbol, StateSet&, CJumpMode) except +
        State insert_word_by_levels(State, vector[vector[Symbol]]&, State) except +
        State insert_word_by_levels(State, vector[vector[Symbol]]&) except +

        CNft& insert_identity(State, vector[Symbol]&, CJumpMode) except +
        CNft& insert_identity(State, CAlphabet*, CJumpMode) except +
        CNft& insert_identity(State, Symbol, CJumpMode) except +

        bool contains_jump_transitions() except +
        # clear()/trim(StateRenaming*)/remove_epsilon(Symbol) are not redeclared: identical signatures are already
        # inherited from CNfa (see the add_state note above for why).
        bool is_identical(CNft&)

        CNft& concatenate(CNft&) except +
        CNft& unite_nondet_with(CNft&) except +
        CNft get_one_letter_aut(cset[Level]&, Symbol) except +
        void unwind_jumps_inplace(COrdVector[Symbol]&, CJumpMode) except +
        CNft unwind_jumps(COrdVector[Symbol]&, CJumpMode) except +

        string print_to_dot(bool, bool, int, CAlphabet*) except +
        void print_to_dot(ostream&, bool, bool, int, CAlphabet*) except +
        string print_to_mata() except +
        void print_to_mata(ostream&) except +

        # post(StateSet&, Symbol) [2-arg] is not redeclared: identical to the inherited CNfa signature.
        StateSet post(StateSet&, Symbol, Level) except +

        bool is_universal(CAlphabet&, CRun*, ParameterMap&) except +
        # is_universal(CAlphabet&, ParameterMap&) [2-arg] is not redeclared: identical to the inherited CNfa signature.
        bool is_in_lang(CRun&, bool, CJumpMode, bool) except +
        bool is_in_lang_prefix(CRun&, CJumpMode, bool) except +
        bool is_in_lang_by_levels(vector[vector[Symbol]]&, bool, CJumpMode, bool) except +
        bool is_in_lang_prefix_by_levels(vector[vector[Symbol]]&, CJumpMode, bool) except +
        # get_word_for_path(CRun&) is not redeclared: identical to the inherited CNfa signature.
        vector[vector[Symbol]] mk_level_word_from_word(vector[Symbol]&) except +
        vector[Symbol] mk_word_from_level_word(vector[vector[Symbol]]&) except +
        cset[vector[Symbol]] get_words(size_t, CJumpMode) except +

        CNft apply_nfa "apply" (CNfa&, Level, bool, CJumpMode) except +
        CNft apply_word "apply" (vector[Symbol]&, Level, bool, CJumpMode) except +

        CNfa to_nfa_copy() except +
        CNfa to_nfa_move() except +
        CNfa to_nfa_update_copy(COrdVector[Symbol]&, CJumpMode) except +
        CNfa to_nfa_update_move(COrdVector[Symbol]&, CJumpMode) except +

        bool make_complete(CAlphabet*, optional[vector[State]]&) except +
        bool make_complete(COrdVector[Symbol]&, optional[vector[State]]&) except +

        shared_ptr[CConstAlphabet] resolve_alphabet(CAlphabet*, optional[Level]) except +
        COrdVector[Symbol] get_symbols_to_work_with(CAlphabet*, optional[Level]) except +

    cdef CNft c_nft_with_levels "mata::nft::Nft::with_levels" (
        CLevels, size_t, CSparseSet[State], CSparseSet[State], shared_ptr[CAlphabetLevels]
    ) except +
    cdef CNft c_nft_with_levels "mata::nft::Nft::with_levels" (
        CLevels, CDelta, CSparseSet[State], CSparseSet[State], shared_ptr[CAlphabetLevels]
    ) except +
    cdef CNft c_nft_with_levels "mata::nft::Nft::with_levels" (
        size_t, size_t, CSparseSet[State], CSparseSet[State], shared_ptr[CAlphabetLevels]
    ) except +
    cdef CNft c_nft_with_levels "mata::nft::Nft::with_levels" (
        size_t, CDelta, CSparseSet[State], CSparseSet[State], shared_ptr[CAlphabetLevels]
    ) except +

    # Free functions.
    cdef COrdVector[Symbol] c_nft_get_symbols_to_work_with "mata::nft::get_symbols_to_work_with" (
        CNft&, CAlphabet*, optional[Level]
    ) except +
    cdef CNft c_union_nondet "mata::nft::union_nondet" (CNft&, CNft&) except +
    cdef CNft c_intersection "mata::nft::intersection" (
        CNft&, CNft&, umap[pair[State, State], State]*, CJumpMode, State, State
    ) except +
    cdef CNft c_compose "mata::nft::compose" (
        CNft&, CNft&, COrdVector[Level]&, COrdVector[Level]&, bool, CJumpMode, CCompositionMode
    ) except +
    cdef shared_ptr[CAlphabetLevels] c_compose_alphabets "mata::nft::compose_alphabets" (
        CNft&, CNft&, COrdVector[Level]&, COrdVector[Level]&, bool
    ) except +
    cdef CNft c_concatenate "mata::nft::concatenate" (
        CNft&, CNft&, bool, StateRenaming*, StateRenaming*
    ) except +
    cdef CNft c_concatenate_nth_power "mata::nft::concatenate_nth_power" (CNft, unsigned) except +
    cdef CNft c_complement "mata::nft::complement" (CNft&, CAlphabet&, ParameterMap&) except +
    cdef CNft c_complement_symbols "mata::nft::complement" (CNft&, COrdVector[Symbol]&, ParameterMap&) except +
    cdef CNft c_determinize "mata::nft::determinize" (CNft&, umap[StateSet, State]*) except +
    cdef CNft c_reduce "mata::nft::reduce" (CNft&, StateRenaming*, ParameterMap&) except +
    cdef bool c_is_included "mata::nft::is_included" (
        CNft&, CNft&, CRun*, CAlphabet*, CJumpMode, ParameterMap&
    ) except +
    cdef bool c_are_equivalent "mata::nft::are_equivalent" (
        CNft&, CNft&, CAlphabet*, CJumpMode, ParameterMap&
    ) except +
    cdef bool c_are_equivalent "mata::nft::are_equivalent" (
        CNft&, CNft&, CJumpMode, ParameterMap&
    ) except +
    cdef CNft c_revert "mata::nft::revert" (CNft&) except +
    cdef CNft c_invert_levels "mata::nft::invert_levels" (CNft&, CJumpMode) except +
    cdef CNft c_remove_epsilon "mata::nft::remove_epsilon" (CNft&, Symbol) except +
    cdef CNft c_project_out "mata::nft::project_out" (CNft&, COrdVector[Level]&, CJumpMode) except +
    cdef CNft c_project_to "mata::nft::project_to" (CNft&, COrdVector[Level]&, CJumpMode) except +
    cdef CNft c_insert_levels "mata::nft::insert_levels" (CNft&, CBoolVector&, CJumpMode) except +
    cdef CNft c_insert_level "mata::nft::insert_level" (CNft&, Level, CJumpMode) except +
    cdef CRun c_encode_word "mata::nft::encode_word" (CAlphabet*, vector[string]) except +
    cdef bool c_symbols_match "mata::nft::symbols_match" (Symbol, Symbol)
    cdef bool c_has_epsilon_cycle "mata::nft::has_epsilon_cycle" (CNft&)

cdef extern from "mata/nft/algorithms.hh" namespace "mata::nft::algorithms":
    cdef CBinaryRelation& c_nft_compute_relation "mata::nft::algorithms::compute_relation" (CNft&, ParameterMap&)

cdef extern from "mata/nft/plumbing.hh" namespace "mata::nft::plumbing":
    cdef void c_nft_union_nondet "mata::nft::plumbing::union_nondet" (CNft*, CNft&, CNft&)
    cdef void c_nft_intersection "mata::nft::plumbing::intersection" (
        CNft*, CNft&, CNft&, umap[pair[State, State], State]*, CJumpMode, State, State
    )
    cdef void c_nft_concatenate "mata::nft::plumbing::concatenate" (
        CNft*, CNft&, CNft&, bool, StateRenaming*, StateRenaming*
    )
    cdef void c_nft_complement "mata::nft::plumbing::complement" (CNft*, CNft&, CAlphabet&, ParameterMap&) except +
    cdef void c_nft_determinize "mata::nft::plumbing::determinize" (CNft*, CNft&, umap[StateSet, State]*)
    cdef void c_nft_reduce "mata::nft::plumbing::reduce" (CNft*, CNft&, StateRenaming*, ParameterMap&)
    cdef void c_nft_revert "mata::nft::plumbing::revert" (CNft*, CNft&)
    cdef void c_nft_remove_epsilon "mata::nft::plumbing::remove_epsilon" (CNft*, CNft&, Symbol) except +

cdef extern from "mata/nft/builder.hh" namespace "mata::nft::builder":
    cdef CNft c_create_single_word_nft "mata::nft::builder::create_single_word_nft" (vector[Symbol]&) except +
    cdef CNft c_create_empty_string_nft "mata::nft::builder::create_empty_string_nft" (size_t) except +
    cdef CNft c_create_sigma_star_nft "mata::nft::builder::create_sigma_star_nft" (size_t) except +
    cdef CNft c_parse_from_mata "mata::nft::builder::parse_from_mata" (string&) except +
    cdef CNft c_from_nfa_with_levels_zero "mata::nft::builder::from_nfa_with_levels_zero" (
        CNfa&, size_t, bool, optional[Symbol]
    ) except +
    cdef CNft c_from_nfa_with_levels_advancing "mata::nft::builder::from_nfa_with_levels_advancing" (
        CNfa, size_t
    ) except +


# Forward declarations of classes.
cdef class Levels:
    cdef CLevels* thisptr

cdef class Nft:
    cdef shared_ptr[CNft] thisptr
    cdef label
