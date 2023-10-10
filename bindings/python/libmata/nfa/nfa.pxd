from libcpp cimport bool
from libcpp.set cimport set as cset
from libcpp.unordered_set cimport unordered_set as uset
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.vector cimport vector
from libcpp.memory cimport shared_ptr
from libcpp.string cimport string
from libcpp.list cimport list as clist
from libcpp.pair cimport pair
from libc.stdint cimport uintptr_t, uint8_t

from libmata.utils cimport CSparseSet, COrdVector, CBoolVector, CBinaryRelation
from libmata.alphabets cimport CAlphabet, Symbol

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
        vector[CSymbolPost] ToVector()
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
        CTransitions transitions

        void reserve(size_t)
        CStatePost& state_post(State)
        CStatePost& operator[](State)
        void emplace_back()
        void clear()
        bool empty()
        void resize(size_t)
        size_t num_of_states()
        void defragment()
        void add(CTrans) except +
        void add(State, Symbol, State) except +
        void remove(CTrans) except +
        void remove(State, Symbol, State) except +
        bool contains(State, Symbol, State)
        COrdVector[CSymbolPost].const_iterator epsilon_symbol_posts(State state, Symbol epsilon)
        COrdVector[CSymbolPost].const_iterator epsilon_symbol_posts(CStatePost& post, Symbol epsilon)
        size_t num_of_transitions()
        CTransitions transitions()
        vector[CTrans] get_transitions_to(State)
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

    cdef cppclass CNfa "mata::nfa::Nfa":
        # Public Attributes
        CSparseSet[State] initial
        CSparseSet[State] final
        CDelta delta
        umap[string, void*] attributes
        CAlphabet* alphabet

        # Constructor
        CNfa() except +
        CNfa(unsigned long) except +
        CNfa(unsigned long, StateSet, StateSet, CAlphabet*)
        CNfa(const CNfa&)

        # Public Functions
        void make_initial(State)
        void make_initial(vector[State])
        bool has_initial(State)
        void remove_initial(State)
        void clear_initial()
        void make_final(State)
        void make_final(vector[State])
        bool has_final(State)
        void remove_final(State)
        void clear_final()
        void unify_initial()
        void unify_final()
        bool is_state(State)
        StateSet post(StateSet&, Symbol)
        State add_state()
        State add_state(State)
        void print_to_DOT(ostream)
        CNfa& trim(StateRenaming*)
        CNfa& concatenate(CNfa&)
        CNfa& uni(CNfa&)
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
        bool is_prfx_in_lang(CRun&)
        pair[CRun, bool] get_word_for_path(CRun&)
        void make_complete(CAlphabet&, State) except +

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
    cdef void c_uni "mata::nfa::plumbing::uni" (CNfa*, CNfa&, CNfa&)
    cdef void c_intersection "mata::nfa::plumbing::intersection" (CNfa*, CNfa&, CNfa&, Symbol, umap[pair[State, State], State]*)
    cdef void c_concatenate "mata::nfa::plumbing::concatenate" (CNfa*, CNfa&, CNfa&, bool, StateRenaming*, StateRenaming*)
    cdef void c_complement "mata::nfa::plumbing::complement" (CNfa*, CNfa&, CAlphabet&, ParameterMap&) except +
    cdef void c_revert "mata::nfa::plumbing::revert" (CNfa*, CNfa&)
    cdef void c_remove_epsilon "mata::nfa::plumbing::remove_epsilon" (CNfa*, CNfa&, Symbol) except +
    cdef void c_minimize "mata::nfa::plumbing::minimize" (CNfa*, CNfa&)
    cdef void c_reduce "mata::nfa::plumbing::reduce" (CNfa*, CNfa&, StateRenaming*, ParameterMap&)



# Forward declarations of classes
#
# This is needed in order for these classes to be used in other packages.
cdef class Nfa:
    # TODO: Shared pointers are not ideal as they bring some overhead which could be substantial in theory. We are not
    #  sure whether the shared pointers will be a problem in this case, but it would be good to pay attention to this and
    #  potentially create some kind of Factory/Allocator/Pool class, that would take care of management of the pointers
    #  to optimize the shared pointers away if we find that the overhead is becoming too significant to ignore.
    cdef shared_ptr[CNfa] thisptr
    cdef label

cdef class Transition:
    cdef CTrans* thisptr
    cdef copy_from(self, CTrans trans)
