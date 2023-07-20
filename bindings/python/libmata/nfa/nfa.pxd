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
from libmata.alphabets cimport CAlphabet, StringToSymbolMap, Symbol

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

cdef extern from "mata/nfa/nfa.hh" namespace "Mata::Nfa":
    # Typedefs
    ctypedef uintptr_t State
    ctypedef COrdVector[State] StateSet
    ctypedef uset[State] UnorderedStateSet
    ctypedef umap[Symbol, StateSet] PostSymb
    ctypedef umap[State, PostSymb] StateToPostMap
    ctypedef umap[string, State] StringSubsetMap
    ctypedef umap[string, State] StringToStateMap
    ctypedef umap[State, string] StateToStringMap
    ctypedef umap[State, State] StateToStateMap
    ctypedef umap[Symbol, string] SymbolToStringMap
    ctypedef umap[string, string] StringMap

    cdef const Symbol CEPSILON "Mata::Nfa::EPSILON"

    cdef cppclass CPost "Mata::Nfa::Post":
        void insert(CMove&)
        CMove& operator[](Symbol)
        CMove& back()
        void push_back(CMove&)
        void remove(CMove&)
        bool empty()
        size_t size()
        vector[CMove] ToVector()
        COrdVector[CMove].const_iterator cbegin()
        COrdVector[CMove].const_iterator cend()

    cdef cppclass CDelta "Mata::Nfa::Delta":
        vector[CPost] post

        void reserve(size_t)
        CPost& operator[](State)
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

    cdef cppclass CRun "Mata::Nfa::Run":
        # Public Attributes
        vector[Symbol] word
        vector[State] path

        # Constructor
        CRun() except +

    cdef cppclass CTrans "Mata::Nfa::Trans":
        # Public Attributes
        State src
        Symbol symb
        State tgt

        # Constructor
        CTrans() except +
        CTrans(State, Symbol, State) except +

        # Public Functions
        bool operator==(CTrans)
        bool operator!=(CTrans)

    cdef cppclass CMove "Mata::Nfa::Move":
        # Public Attributes
        Symbol symbol
        StateSet targets

        # Constructors
        CMove() except +
        CMove(Symbol) except +
        CMove(Symbol, State) except +
        CMove(Symbol, StateSet) except +

        bool operator<(CMove)
        bool operator<=(CMove)
        bool operator>(CMove)
        bool operator>=(CMove)

    cdef cppclass CNfa "Mata::Nfa::Nfa":
        # Nested iterator
        cppclass const_iterator:
            const_iterator()
            CTrans operator*()
            const_iterator& operator++()
            bool operator==(const_iterator&)
            bool operator!=(const_iterator&)
            void refresh_trans()

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
        COrdVector[Symbol] get_used_symbols()
        bool is_state(State)
        size_t get_num_of_trans()
        StateSet post(StateSet&, Symbol)
        CNfa.const_iterator begin()
        CNfa.const_iterator end()
        State add_state()
        State add_state(State)
        void print_to_DOT(ostream)
        CPost get_moves_from(State)
        vector[CTrans] get_transitions_to(State)
        vector[CTrans] get_trans_as_sequence()
        vector[CTrans] get_trans_from_as_sequence(State)
        void trim(StateToStateMap*)
        void get_one_letter_aut(CNfa&)
        bool is_epsilon(Symbol)
        CBoolVector get_useful_states()
        StateSet get_reachable_states()
        StateSet get_terminating_states()
        void remove_epsilon(Symbol) except +
        COrdVector[CMove].const_iterator get_epsilon_transitions(State state, Symbol epsilon)
        COrdVector[CMove].const_iterator get_epsilon_transitions(CPost& post, Symbol epsilon)
        void clear()
        size_t size()

    # Automata tests
    cdef bool c_is_deterministic "Mata::Nfa::is_deterministic" (CNfa&)
    cdef bool c_is_lang_empty "Mata::Nfa::is_lang_empty" (CNfa&, CRun*)
    cdef bool c_is_universal "Mata::Nfa::is_universal" (CNfa&, CAlphabet&, StringMap&) except +
    cdef bool c_is_included "Mata::Nfa::is_included" (CNfa&, CNfa&, CAlphabet*, StringMap&)
    cdef bool c_is_included "Mata::Nfa::is_included" (CNfa&, CNfa&, CRun*, CAlphabet*, StringMap&) except +
    cdef bool c_are_equivalent "Mata::Nfa::are_equivalent" (CNfa&, CNfa&, CAlphabet*, StringMap&)
    cdef bool c_are_equivalent "Mata::Nfa::are_equivalent" (CNfa&, CNfa&, StringMap&)
    cdef bool c_is_complete "Mata::Nfa::is_complete" (CNfa&, CAlphabet&) except +
    cdef bool c_is_in_lang "Mata::Nfa::is_in_lang" (CNfa&, CRun&)
    cdef bool c_is_prfx_in_lang "Mata::Nfa::is_prfx_in_lang" (CNfa&, CRun&)

    # Automata operations
    cdef void compute_fw_direct_simulation(const CNfa&)

    # Helper functions
    cdef pair[CRun, bool] c_get_word_for_path "Mata::Nfa::get_word_for_path" (CNfa&, CRun&)
    cdef CRun c_encode_word "Mata::Nfa::encode_word" (StringToSymbolMap&, vector[string])

cdef extern from "mata/nfa/algorithms.hh" namespace "Mata::Nfa::Algorithms":
    cdef CBinaryRelation& c_compute_relation "Mata::Nfa::Algorithms::compute_relation" (CNfa&, StringMap&)

cdef extern from "mata/nfa/plumbing.hh" namespace "Mata::Nfa::Plumbing":
    cdef void get_elements(StateSet*, CBoolVector)
    cdef void c_determinize "Mata::Nfa::Plumbing::determinize" (CNfa*, CNfa&, umap[StateSet, State]*)
    cdef void c_uni "Mata::Nfa::Plumbing::uni" (CNfa*, CNfa&, CNfa&)
    cdef void c_intersection "Mata::Nfa::Plumbing::intersection" (CNfa*, CNfa&, CNfa&, bool, umap[pair[State, State], State]*)
    cdef void c_concatenate "Mata::Nfa::Plumbing::concatenate" (CNfa*, CNfa&, CNfa&, bool, StateToStateMap*, StateToStateMap*)
    cdef void c_complement "Mata::Nfa::Plumbing::complement" (CNfa*, CNfa&, CAlphabet&, StringMap&) except +
    cdef void c_make_complete "Mata::Nfa::Plumbing::make_complete" (CNfa*, CAlphabet&, State) except +
    cdef void c_revert "Mata::Nfa::Plumbing::revert" (CNfa*, CNfa&)
    cdef void c_remove_epsilon "Mata::Nfa::Plumbing::remove_epsilon" (CNfa*, CNfa&, Symbol) except +
    cdef void c_minimize "Mata::Nfa::Plumbing::minimize" (CNfa*, CNfa&)
    cdef void c_reduce "Mata::Nfa::Plumbing::reduce" (CNfa*, CNfa&, bool, StateToStateMap*, StringMap&)



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

cdef class Trans:
    cdef CTrans* thisptr
    cdef copy_from(self, CTrans trans)
