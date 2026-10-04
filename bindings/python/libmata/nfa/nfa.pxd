from libcpp cimport bool
from libcpp.optional cimport optional
from libcpp.set cimport set as cset
from libcpp.unordered_set cimport unordered_set as uset
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector
from libcpp.memory cimport shared_ptr
from libcpp.string cimport string
from libcpp.list cimport list as clist
from libcpp.pair cimport pair
from libc.stdint cimport uintptr_t, uint8_t

from libmata.utils cimport CSparseSet, COrdVector, CBoolVector, CBinaryRelation, CPairHash
from libmata.alphabets cimport CAlphabet, CConstAlphabet, CLazyWordGenerator, Symbol

cdef extern from "<iostream>" namespace "std":
    cdef cppclass ostream:
        ostream& write(const char*, int) except +

cdef extern from "<fstream>" namespace "std":
    cdef cppclass ofstream(ostream):
        ofstream(const char*) except +

cdef extern from "<sstream>" namespace "std":
    cdef cppclass stringstream(ostream):
        stringstream(string) except +
        string str()

cdef extern from "mata/nfa/nfa.hh" namespace "mata::nfa":
    # Typedefs
    ctypedef uintptr_t State
    ctypedef COrdVector[State] StateSet
    ctypedef uset[State] UnorderedStateSet
    ctypedef umap[Symbol, StateSet] PostSymb
    ctypedef umap[State, PostSymb] StateToPostMap
    ctypedef umap[string, State] StringSubsetMap
    ctypedef umap[State, string] StateNameMap
    ctypedef umap[State, State] StateRenaming
    ctypedef umap[string, string] ParameterMap

    cdef const Symbol CEPSILON "mata::nfa::EPSILON"

    cdef cppclass CStatePost "mata::nfa::StatePost":
        void insert(CSymbolPost&)
        CSymbolPost& operator[](Symbol)
        CSymbolPost& back()
        void push_back(CSymbolPost&)
        void remove(CSymbolPost&)
        bool empty()
        vector[CSymbolPost] to_vector()
        COrdVector[CSymbolPost].const_iterator cbegin()
        COrdVector[CSymbolPost].const_iterator cend()

    cdef cppclass CTransitions "mata::nfa::Delta::Transitions":
        cppclass const_iterator:
            bool operator==(const_iterator&)
            bool operator!=(const_iterator&)
            CTrans& operator*()
            const_iterator& operator++()
        const_iterator begin()
        const_iterator end()
        CTransitions()


    cdef cppclass CDelta "mata::nfa::Delta":
        vector[CStatePost] state_posts

        bool operator==(CDelta&)
        void reserve(size_t)
        CStatePost& state_post(State)
        CStatePost& operator[](State)
        void clear()
        bool empty()
        bool uses_state(State)
        size_t num_of_transitions()
        size_t num_of_states()
        void add(CTrans) except +
        void add(State, Symbol, State) except +
        void add_targets "add" (State, Symbol, StateSet) except +
        void remove(CTrans) except +
        void remove(State, Symbol, State) except +
        bool contains(State, Symbol, State)
        bool contains(CTrans)
        CTransitions transitions()
        vector[CTrans] get_transitions_to(State) except +
        vector[CTrans] get_transitions_between(State, State) except +
        COrdVector[CSymbolPost].const_iterator epsilon_symbol_posts(State state, Symbol epsilon)
        COrdVector[CSymbolPost].const_iterator epsilon_symbol_posts(CStatePost& post, Symbol epsilon)
        void add(vector[CTrans]&) except +
        COrdVector[Symbol] get_used_symbols()


    cdef cppclass CRun "mata::nfa::Run":
        # Public Attributes
        vector[Symbol] word
        vector[State] path

        # Constructor
        CRun() except +

    cdef cppclass CTrans "mata::nfa::Transition":
        # Public Attributes
        State source
        Symbol symbol
        State target

        # Constructor
        CTrans() except +
        CTrans(State, Symbol, State) except +

        # Public Functions
        bool operator==(CTrans)
        bool operator!=(CTrans)

    cdef cppclass CSymbolPost "mata::nfa::SymbolPost":
        # Public Attributes
        Symbol symbol
        StateSet targets

        # Constructors
        CSymbolPost() except +
        CSymbolPost(Symbol) except +
        CSymbolPost(Symbol, State) except +
        CSymbolPost(Symbol, StateSet) except +

        bool operator<(CSymbolPost)
        bool operator<=(CSymbolPost)
        bool operator>(CSymbolPost)
        bool operator>=(CSymbolPost)

        COrdVector[State].const_iterator begin()
        COrdVector[State].const_iterator end()

    # Structural base shared by mata::nfa::Nfa and mata::nft::Nft.
    cdef cppclass CAutomaton "mata::Automaton":
        CSparseSet[State] initial
        CSparseSet[State] final
        CDelta delta

    cdef cppclass CNfa "mata::nfa::Nfa":
        # Public Attributes
        CSparseSet[State] initial
        CSparseSet[State] final
        CDelta delta
        umap[string, void*] attributes
        shared_ptr[CAlphabet] alphabet

        # Constructor
        CNfa() except +
        CNfa(unsigned long) except +
        CNfa(unsigned long, CSparseSet[State], CSparseSet[State], shared_ptr[CAlphabet])
        CNfa(const CNfa&)

        # Public Functions
        void unify_initial(bool)
        void unify_final(bool)
        bool is_state(State)
        StateSet post(StateSet&, Symbol)
        State add_state()
        State add_state(State)
        void print_to_dot(ostream, bool, bool, int)
        CNfa& trim(StateRenaming*)
        CNfa& concatenate(CNfa&)
        CNfa& unite_nondet_with(CNfa&)
        void get_one_letter_aut(CNfa&)
        bool is_epsilon(Symbol)
        CBoolVector get_useful_states()
        StateSet get_reachable_states()
        StateSet get_terminating_states()
        void remove_epsilon(Symbol) except +
        void clear()
        size_t num_of_states()
        bool is_lang_empty(CRun*)
        bool is_deterministic()
        bool is_complete(CAlphabet*) except +
        bool is_complete() except +
        bool is_universal(CAlphabet&, ParameterMap&) except +
        bool is_in_lang(CRun&)
        bool is_in_lang(CRun&, bool)
        bool is_in_lang(CRun&, bool, bool)
        StateSet read_word(CRun&)
        StateSet read_word(CRun&, bool)
        optional[State] read_word_det(CRun&)
        pair[CRun, bool] get_word_for_path(CRun&)
        cset[vector[Symbol]] get_words(size_t) except +
        bool make_complete(CAlphabet*, optional[State]) except +
        shared_ptr[CConstAlphabet] resolve_alphabet(CAlphabet*) except +
        COrdVector[Symbol] get_symbols_to_work_with(CAlphabet*) except +

    # Automata tests
    cdef bool c_is_included "mata::nfa::is_included" (CNfa&, CNfa&, CAlphabet*, ParameterMap&)
    cdef bool c_is_included "mata::nfa::is_included" (CNfa&, CNfa&, CRun*, CAlphabet*, ParameterMap&) except +
    cdef bool c_are_equivalent "mata::nfa::are_equivalent" (CNfa&, CNfa&, CAlphabet*, ParameterMap&)
    cdef bool c_are_equivalent "mata::nfa::are_equivalent" (CNfa&, CNfa&, ParameterMap&)

    # Automata operations
    cdef void compute_fw_direct_simulation(const CNfa&)

    # Helper functions
    cdef CRun c_encode_word "mata::nfa::encode_word" (CAlphabet*, vector[string])

cdef extern from "mata/nfa/algorithms.hh" namespace "mata::nfa::algorithms":
    cdef CBinaryRelation& c_compute_relation "mata::nfa::algorithms::compute_relation" (CNfa&, ParameterMap&)

cdef extern from "mata/nfa/plumbing.hh" namespace "mata::nfa::plumbing":
    cdef void get_elements(StateSet*, CBoolVector)
    cdef void c_determinize "mata::nfa::plumbing::determinize" (CNfa*, CNfa&, umap[StateSet, State]*)
    cdef void c_union_nondet "mata::nfa::plumbing::union_nondet" (CNfa*, CNfa&, CNfa&)
    cdef void c_intersection "mata::nfa::plumbing::intersection" (CNfa*, CNfa&, CNfa&, Symbol, umap[pair[State, State], State, CPairHash[State, State]]*)
    cdef void c_concatenate "mata::nfa::plumbing::concatenate" (CNfa*, CNfa&, CNfa&, bool, StateRenaming*, StateRenaming*)
    cdef void c_complement "mata::nfa::plumbing::complement" (CNfa*, CNfa&, CAlphabet&, ParameterMap&) except +
    cdef void c_revert "mata::nfa::plumbing::revert" (CNfa*, CNfa&)
    cdef void c_remove_epsilon "mata::nfa::plumbing::remove_epsilon" (CNfa*, CNfa&, Symbol) except +
    cdef void c_minimize "mata::nfa::plumbing::minimize" (CNfa*, CNfa&, ParameterMap&)
    cdef void c_reduce "mata::nfa::plumbing::reduce" (CNfa*, CNfa&, StateRenaming*, ParameterMap&) except +
    cdef void c_reduce_residual_with "mata::nfa::plumbing::reduce_residual_with" (CNfa*, CNfa&)
    cdef void c_reduce_residual_after "mata::nfa::plumbing::reduce_residual_after" (CNfa*, CNfa&)
    cdef CLazyWordGenerator* c_get_words_lazy_ptr "mata::nfa::plumbing::get_words_lazy_ptr" (CNfa&, size_t) except +



# Forward declarations of classes
#
# This is needed in order for these classes to be used in other packages.
cdef class Nfa:
    """Wrapper over NFA automaton.
    
    Ownership model:
    - Nfa owns the automaton via shared_ptr[CNfa]; the C++ object lives as long as any Python or C++ reference exists.
    - Delta views obtained from Nfa.delta hold an aliasing shared_ptr to the same automaton, keeping it alive.
    - Iteration generators (iterate(), iter_transitions_from()) hold self alive, so the automaton outlives them.
    - Transition/SymbolPost/Run objects own a raw heap object individually (not shared).
    """
    # TODO: Shared pointers bring atomic-count overhead; measure in a loop (create/drop views, call methods) to decide if real.
    cdef shared_ptr[CNfa] thisptr
    cdef label

cdef class Transition:
    """Wrapper over a transition: (source, symbol, target).
    
    Ownership: owns a raw heap object (not shared with the automaton).
    """
    cdef CTrans* thisptr
    cdef copy_from(self, CTrans trans)

cdef class Delta:
    """View over the transition relation (Delta) of an automaton.
    
    Ownership: holds an aliasing shared_ptr to the owning automaton, keeping it alive. The Delta view
    stays valid even if all other Python references to the automaton are dropped.
    """
    cdef shared_ptr[CAutomaton] automaton_ptr

cdef object wrap_delta(shared_ptr[CAutomaton] automaton_ptr)
