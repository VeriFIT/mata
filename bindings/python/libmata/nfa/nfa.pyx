import shlex
import subprocess
import pandas
import networkx as nx

from libc.stdint cimport uint8_t
from libcpp cimport bool
from libcpp.list cimport list as clist
from libcpp.memory cimport shared_ptr, make_shared
from libcpp.set cimport set as cset
from libcpp.string cimport string
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.utility cimport pair
from libcpp.vector cimport vector

from cython.operator import dereference, postincrement as postinc, preincrement as preinc

cimport libmata.nfa.nfa as mata_nfa
cimport libmata.alphabets as alph

from libmata.nfa.nfa cimport \
    Symbol, State, StateSet, StateToStateMap, StringToSymbolMap, \
    CDelta, CRun, CTrans, CNfa, CMove, CEPSILON

from libmata.alphabets cimport CAlphabet
from libmata.utils cimport COrdVector, CBinaryRelation, BinaryRelation

cdef Symbol EPSILON = CEPSILON

def epsilon():
    return EPSILON

cdef class Run:
    """Wrapper over the run in NFA."""
    cdef mata_nfa.CRun *thisptr

    def __cinit__(self):
        """Constructor of the transition

        :param State src: source state
        :param Symbol s: symbol
        :param State tgt: target state
        """
        self.thisptr = new mata_nfa.CRun()

    def __dealloc__(self):
        """Destructor"""
        if self.thisptr != NULL:
            del self.thisptr

    @property
    def word(self):
        return self.thisptr.word

    @word.setter
    def word(self, value):
        self.thisptr.word = value

    @property
    def path(self):
        return self.thisptr.path

    @path.setter
    def path(self, value):
        self.thisptr.path = value

cdef class Trans:
    """Wrapper over the transitions in NFA."""

    @property
    def src(self):
        """
        :return: source state of the transition
        """
        return self.thisptr.src

    @property
    def symb(self):
        """
        :return: symbol for the transition
        """
        return self.thisptr.symb

    @property
    def tgt(self):
        """
        :return: target state of the transition
        """
        return self.thisptr.tgt

    def __cinit__(self, State src=0, Symbol s=0, State tgt=0):
        """Constructor of the transition

        :param State src: source state
        :param Symbol s: symbol
        :param State tgt: target state
        """
        self.thisptr = new mata_nfa.CTrans(src, s, tgt)

    def __dealloc__(self):
        """Destructor"""
        if self.thisptr != NULL:
            del self.thisptr

    cdef copy_from(self, CTrans trans):
        """Copies the internals of trans into the wrapped pointer

        :param CTrans trans: copied transition
        """
        self.thisptr.src = trans.src
        self.thisptr.symb = trans.symb
        self.thisptr.tgt = trans.tgt

    def __eq__(self, Trans other):
        return dereference(self.thisptr) == dereference(other.thisptr)

    def __neq__(self, Trans other):
        return dereference(self.thisptr) != dereference(other.thisptr)

    def __str__(self):
        return f"{self.thisptr.src}-[{self.thisptr.symb}]\u2192{self.thisptr.tgt}"

    def __repr__(self):
        return str(self)


cdef class Move:
    """Wrapper over pair of symbol and states for transitions"""
    cdef mata_nfa.CMove *thisptr

    @property
    def symbol(self):
        return self.thisptr.symbol

    @symbol.setter
    def symbol(self, value):
        self.thisptr.symbol = value

    @property
    def targets(self):
        cdef vector[State] states_as_vector = self.thisptr.targets.ToVector()
        return [s for s in states_as_vector]

    @targets.setter
    def targets(self, value):
        cdef StateSet targets = StateSet(value)
        self.thisptr.targets = targets

    def __cinit__(self, Symbol symbol, vector[State] states):
        cdef StateSet targets = StateSet(states)
        self.thisptr = new mata_nfa.CMove(symbol, targets)

    def __dealloc__(self):
        if self.thisptr != NULL:
            del self.thisptr

    def __lt__(self, Move other):
        return dereference(self.thisptr) < dereference(other.thisptr)

    def __gt__(self, Move other):
        return dereference(self.thisptr) > dereference(other.thisptr)

    def __le__(self, Move other):
        return dereference(self.thisptr) <= dereference(other.thisptr)

    def __ge__(self, Move other):
        return dereference(self.thisptr) >= dereference(other.thisptr)

    def __eq__(self, Move other):
        return self.symbol == other.symbol and self.targets == other.targets

    def __neq__(self, Move other):
        return self.symbol != other.symbol or self.targets != other.targets

    def __str__(self):
        trans = "{" + ",".join(map(str, self.targets)) + "}"
        return f"[{self.symbol}]\u2192{trans}"

    def __repr__(self):
        return str(self)


cdef class Nfa:
    """Wrapper over NFA

    Note: In order to add more properties to Nfa, see `nfa.pxd`, where there is forward declaration
    """

    def __cinit__(self, state_number = 0, alph.Alphabet alphabet = None, label=None):
        """Constructor of the NFA.

        :param int state_number: number of states in automaton
        :param alph.Alphabet alphabet: alphabet corresponding to the automaton
        """
        cdef CAlphabet* c_alphabet = NULL
        cdef StateSet empty_default_state_set
        if alphabet:
            c_alphabet = alphabet.as_base()
        self.thisptr = make_shared[CNfa](
            mata_nfa.CNfa(state_number, empty_default_state_set, empty_default_state_set, c_alphabet)
        )
        self.label = label

    @property
    def label(self):
        return self.label

    @label.setter
    def label(self, value):
        self.label = value

    @property
    def initial_states(self):
        return [initial_state for initial_state in self.thisptr.get().initial]

    @initial_states.setter
    def initial_states(self, vector[State] value):
        self.thisptr.get().initial.clear()
        for state in value:
            self.thisptr.get().initial.insert(state)

    @property
    def final_states(self):
        return [final_state for final_state in self.thisptr.get().final]

    @final_states.setter
    def final_states(self, vector[State] value):
        self.thisptr.get().final.clear()
        for state in value:
            self.thisptr.get().final.insert(state)

    def is_state(self, state):
        """Tests if state is in the automaton

        :param int state: tested state
        :return: true if state is in the automaton
        """
        return self.thisptr.get().is_state(state)

    def add_new_state(self):
        """Adds new state to automaton

        :return: number of the state
        """
        return self.thisptr.get().add_state()

    def add_state(self, State state):
        """Adds passed state to the automaton.

        :return: number of the state
        """
        return self.thisptr.get().add_state(state)

    def make_initial_state(self, State state):
        """Makes specified state from the automaton initial.

        :param State state: State to be made initial.
        """
        self.thisptr.get().initial.insert(state)

    def make_initial_states(self, vector[State] states):
        """Makes specified states from the automaton initial.

        :param list states: List of states to be made initial.
        """
        for state in states:
            self.thisptr.get().initial.insert(state)

    def has_initial_state(self, State st):
        """Tests if automaton contains given state

        :param State st: tested state
        :return: true if automaton contains given state
        """
        return self.thisptr.get().initial[st]

    def remove_initial_state(self, State state):
        """Removes state from initial state set of the automaton.

        :param State state: State to be removed from initial states.
        """
        self.thisptr.get().initial.erase(state)

    def clear_initial(self):
        """Clears initial state set of the automaton."""
        self.thisptr.get().initial.clear()

    def make_final_state(self, State state):
        """Makes specified state from the automaton final.

        :param State state: State to be made final.
        """
        self.thisptr.get().final.insert(state)

    def make_final_states(self, vector[State] states):
        """Makes specified states from the automaton final.

        :param vector[State] states: List of states to be made final.
        """
        for state in states:
            self.thisptr.get().final.insert(state)

    def has_final_state(self, State st):
        """Tests if automaton contains given state

        :param State st: tested state
        :return: true if automaton contains given state
        """
        return self.thisptr.get().final[st]

    def remove_final_state(self, State state):
        """Removes state from final state set of the automaton.

        :param State state: State to be removed from final states.
        """
        self.thisptr.get().final.erase(state)

    def clear_final(self):
        """Clears final state set of the automaton."""
        self.thisptr.get().final.clear()

    def unify_initial(self):
        """Unify initial states into a single new initial state."""
        self.thisptr.get().unify_initial()

    def unify_final(self):
        """Unify final states into a single new final state."""
        self.thisptr.get().unify_final()

    def add_transition_object(self, Trans tr):
        """Adds transition to automaton

        :param Trans tr: added transition
        """
        self.thisptr.get().delta.add(dereference(tr.thisptr))

    def add_transition(self, State src, symb, State tgt, alph.Alphabet alphabet = None):
        """Constructs transition and adds it to automaton

        :param State src: source state
        :param Symbol symb: symbol
        :param State tgt: target state
        :param alph.Alphabet alphabet: alphabet of the transition
        """
        if isinstance(symb, str):
            alphabet = alphabet or store().get('alphabet')
            if not alphabet:
                raise Exception(f"Cannot translate symbol '{symb}' without specified alphabet")
            self.thisptr.get().delta.add(src, alphabet.translate_symbol(symb), tgt)
        else:
            self.thisptr.get().delta.add(src, symb, tgt)

    def remove_trans(self, Trans tr):
        """Removes transition from the automaton.

        :param Trans tr: Transition to be removed.
        """
        self.thisptr.get().delta.remove(dereference(tr.thisptr))

    def remove_trans_raw(self, State src, Symbol symb, State tgt):
        """Constructs transition and removes it from the automaton.

        :param State src: Source state of the transition to be removed.
        :param Symbol symb: Symbol of the transition to be removed.
        :param State tgt: Target state of the transition to be removed.
        """
        self.thisptr.get().delta.remove(src, symb, tgt)

    def has_transition(self, State src, Symbol symb, State tgt):
        """Tests if automaton contains transition

        :param State src: source state
        :param Symbol symb: symbol
        :param State tgt: target state
        :return: true if automaton contains transition
        """
        return self.thisptr.get().delta.contains(src, symb, tgt)

    def get_num_of_trans(self):
        """Returns number of transitions in automaton

        :return: number of transitions in automaton
        """
        return self.thisptr.get().get_num_of_trans()

    def clear(self):
        """Clears all of the internals in the automaton"""
        self.thisptr.get().clear()

    def size(self) -> int:
        """Get the current number of states in the whole automaton.
        :return: The number of states.
        """
        return self.thisptr.get().size()

    def iterate(self):
        """Iterates over all transitions

        :return: stream of transitions
        """
        iterator = self.thisptr.get().begin()
        while iterator != self.thisptr.get().end():
            trans = Trans()
            lhs = dereference(iterator)
            trans.copy_from(lhs)
            preinc(iterator)
            yield trans

    def get_transitions_from_state(self, State state):
        """Returns list of Move for the given state

        :param State state: state for which we are getting the transitions
        :return: Move
        """
        cdef mata_nfa.CPost transitions = self.thisptr.get().get_moves_from(state)
        cdef vector[mata_nfa.CMove] transitions_list = transitions.ToVector()

        cdef vector[mata_nfa.CMove].iterator it = transitions_list.begin()
        cdef vector[mata_nfa.CMove].iterator end = transitions_list.end()
        transsymbols = []
        while it != end:
            t = Move(
                dereference(it).symbol,
                dereference(it).targets.ToVector()
            )
            postinc(it)
            transsymbols.append(t)
        return transsymbols

    def get_transitions_to_state(self, State state_to):
        """Get transitions leading to state_to (state_to is their target state).

        :return: List mata_nfa.CTrans: List of transitions leading to state_to.
        """
        cdef vector[CTrans] c_transitions = self.thisptr.get().get_transitions_to(state_to)
        trans = []
        for c_transition in c_transitions:
            trans.append(Trans(c_transition.src, c_transition.symb, c_transition.tgt))
        return trans

    def get_trans_as_sequence(self):
        """Get automaton transitions as a sequence.

        :return: List of automaton transitions.
        """
        cdef vector[CTrans] c_transitions = self.thisptr.get().get_trans_as_sequence()
        transitions = []
        for c_transition in c_transitions:
            transitions.append(Trans(c_transition.src, c_transition.symb, c_transition.tgt))
        return transitions

    def get_trans_from_state_as_sequence(self, State state_from):
        """Get automaton transitions from state_from as a sequence.

        :return: List of automaton transitions.
        """
        cdef vector[CTrans] c_transitions = self.thisptr.get().get_trans_from_as_sequence(state_from)
        transitions = []
        for c_transition in c_transitions:
            transitions.append(Trans(c_transition.src, c_transition.symb, c_transition.tgt))
        return transitions

    def get_useful_states(self):
        """Get useful states (states which are reachable and terminating at the same time).

        :return: A set of useful states.
        """
        cdef CBoolVector useful_states_bool_vec = self.thisptr.get().get_useful_states()
        cdef StateSet useful_states
        mata_nfa.get_elements(&useful_states, useful_states_bool_vec)
        return { state for state in useful_states }

    def get_reachable_states(self):
        """Get reachable states.

        :return: A set of reachable states.
        """
        cdef vector[State] return_value = self.thisptr.get().get_reachable_states().ToVector()
        return {state for state in return_value}

    def get_terminating_states(self):
        """Get terminating states (states with a path from them leading to any final state).

        :return: A set of terminating states.
        """
        cdef vector[State] return_value = self.thisptr.get().get_terminating_states().ToVector()
        return {state for state in return_value}

    def trim(self):
        """Remove inaccessible (unreachable) and not co-accessible (non-terminating) states.

        Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
         starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
         the starting point of a path ending in a final state).
        """
        self.thisptr.get().trim(NULL)

    def trim_with_state_map(self):
        """Remove inaccessible (unreachable) and not co-accessible (non-terminating) states.

        Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
         starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
         the starting point of a path ending in a final state).

        :return: State map of original to new states.
        """
        cdef StateToStateMap state_map
        self.thisptr.get().trim(&state_map)
        return {k: v for k, v in state_map}

    def get_one_letter_aut(self) -> Nfa:
        """Unify transitions to create a directed graph with at most a single transition between two states (using only
         one letter for all transitions).

        :return: mata_nfa.Nfa: One letter automaton representing a directed graph.
        """
        cdef Nfa one_letter_aut = mata_nfa.Nfa()
        self.thisptr.get().get_one_letter_aut(dereference(one_letter_aut.thisptr.get()))
        return one_letter_aut

    def is_epsilon(self, Symbol symbol) -> bool:
        """Check whether passed symbol is epsilon symbol or not.

        :param Symbol symbol: The symbol to check.
        :return: True if the passed symbol is epsilon symbol, False otherwise.
        """
        return self.thisptr.get().is_epsilon(symbol)

    def __str__(self):
        """String representation of the automaton displays states, and transitions

        :return: string representation of the automaton
        """
        result = "initial_states: {}\n".format([s for s in self.thisptr.get().initial])
        result += "final_states: {}\n".format([s for s in self.thisptr.get().final])
        result += "transitions:\n"
        for trans in self.iterate():
            symbol = trans.symb if self.thisptr.get().alphabet == NULL \
                else self.thisptr.get().alphabet.reverse_translate_symbol(trans.symb)
            result += f"{trans.src}-[{symbol}]\u2192{trans.tgt}\n"
        return result

    def __repr__(self):
        return str(self)

    def to_dot_file(self, output_file='aut.dot', output_format='pdf'):
        """Transforms the automaton to dot format.

        By default, the result is saved to `aut.dot`, and further to `aut.dot.pdf`.

        One can choose other format that is supported by graphviz format (e.g. pdf or png).

        :param str output_file: name of the output file where the automaton will be stored
        :param str output_format: format of the output file (pdf/png/etc)
        """
        cdef mata_nfa.ofstream* output
        output = new mata_nfa.ofstream(output_file.encode('utf-8'))
        try:
            self.thisptr.get().print_to_DOT(dereference(output))
        finally:
            del output

        graphviz_command = f"dot -O -T{output_format} {output_file}"
        _, err = run_safely_external_command(graphviz_command)
        if err:
            print(f"error while dot file: {err}")

    def to_dot_str(self, encoding='utf-8'):
        """Transforms the automaton to dot format string

        :param str encoding: encoding of the dot string
        :return: string with dot representation of the automaton
        """
        cdef mata_nfa.stringstream* output_stream
        output_stream = new mata_nfa.stringstream("".encode('ascii'))
        cdef string result
        try:
            self.thisptr.get().print_to_DOT(dereference(output_stream))
            result = output_stream.str()
        finally:
            del output_stream
        return result.decode(encoding)

    def to_dataframe(self) -> pandas.DataFrame:
        """Transforms the automaton to DataFrame format.

        Transforms the automaton into pandas.DataFrame format,
        that is suitable for further (statistical) analysis.
        The resulting DataFrame is in tabular format, with
        'source', 'symbol' and 'target' as the columns

        :return: automaton represented as a pandas dataframe
        """
        columns = ['source', 'symbol', 'target']
        data = [
            [trans.src, trans.symb, trans.tgt] for trans in self.iterate()
        ]
        return pandas.DataFrame(data, columns=columns)

    def to_networkx_graph(self) -> nx.Graph:
        """Transforms the automaton into networkx.Graph

        Transforms the automaton into networkx.Graph format,
        that is represented as graph with edges (src, tgt) with
        additional properties.

        Each symbol is added as an property to each edge.

        :return:
        """
        G = nx.DiGraph()
        for trans in self.iterate():
            G.add_edge(trans.src, trans.tgt, symbol=trans.symb)
        return G

    def post_map_of(self, State st, alph.Alphabet alphabet):
        """Returns mapping of symbols to set of states.

        :param State st: source state
        :param alph.Alphabet alphabet: alphabet of the post
        :return: dictionary mapping symbols to set of reachable states from the symbol
        """
        symbol_map = {}
        for symbol in alphabet.get_alphabet_symbols():
            symbol_map[symbol] = self.post_of({st}, symbol)
        return symbol_map

    def post_of(self, vector[State] states, Symbol symbol):
        """Returns sets of reachable states from set of states through a symbol

        :param StateSet states: set of states
        :param Symbol symbol: source symbol
        :return: set of reachable states
        """
        cdef vector[State] return_value
        cdef StateSet input_states = StateSet(states)
        return_value = self.thisptr.get().post(input_states, symbol).ToVector()
        return {v for v in return_value}


    def remove_epsilon_inplace(self, Symbol epsilon = CEPSILON):
        """Removes transitions which contain epsilon symbol.

        TODO: Possibly there may be issue with setting the size of the automaton beforehand?

        :param Symbol epsilon: Symbol representing the epsilon symbol.
        """
        self.thisptr.get().remove_epsilon(epsilon)

    def get_epsilon_transitions(self, State state, Symbol epsilon = CEPSILON) -> Move | None:
        """Get epsilon transitions for a state.

        :param state: State to get epsilon transitions for.
        :param epsilon: Epsilon symbol.
        :return: Epsilon transitions if there are any epsilon transitions for the passed state. None otherwise.
        """
        cdef COrdVector[CMove].const_iterator c_epsilon_transitions_iter = self.thisptr.get().get_epsilon_transitions(
            state, epsilon
        )
        if c_epsilon_transitions_iter == self.thisptr.get().get_moves_from(state).cend():
            return None

        cdef CMove epsilon_transitions = dereference(c_epsilon_transitions_iter)
        return Move(epsilon_transitions.symbol, epsilon_transitions.targets.ToVector())



    def get_symbols(self):
        """Return a set of symbols used on the transitions in NFA.

        :return: Set of symbols.
        """
        cdef COrdVector[Symbol] symbols = self.thisptr.get().get_used_symbols()
        return {s for s in symbols}



# Operations
def determinize_with_subset_map(Nfa lhs):
    """Determinize the lhs automaton

    :param Nfa lhs: non-deterministic finite automaton
    :return: deterministic finite automaton, subset map
    """
    result = Nfa()
    cdef umap[StateSet, State] subset_map
    mata_nfa.c_determinize(result.thisptr.get(), dereference(lhs.thisptr.get()), &subset_map)
    return result, subset_map_to_dictionary(subset_map)

def determinize(Nfa lhs):
    """Determinize the lhs automaton

    :param Nfa lhs: non-deterministic finite automaton
    :return: deterministic finite automaton
    """
    result = Nfa()
    mata_nfa.c_determinize(result.thisptr.get(), dereference(lhs.thisptr.get()), NULL)
    return result

def union(Nfa lhs, Nfa rhs):
    """Performs union of lhs and rhs

    :param Nfa lhs: first automaton
    :param Nfa rhs: second automaton
    :return: union of lhs and rhs
    """
    result = Nfa()
    mata_nfa.c_uni(
        result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get())
    )
    return result

def intersection(Nfa lhs, Nfa rhs, preserve_epsilon: bool = False):
    """Performs intersection of lhs and rhs.

    Supports epsilon symbols when preserve_epsilon is set to True.

    When computing intersection preserving epsilon transitions, create product of two NFAs, where both automata can contain ε-transitions. The product preserves the ε-transitions
     of both automata. This means that for each ε-transition of the form `s-ε->p` and each product state `(s,a)`,
     an ε-transition `(s,a)-ε->(p,a)` is created. Furthermore, for each ε-transition `s-ε->p` and `a-ε->b`,
     a product state `(s,a)-ε->(p,b)` is created.

    Automata must share alphabets.

    :param preserve_epsilon: Whether to compute intersection preserving epsilon transitions.
    :param Nfa lhs: First automaton.
    :param Nfa rhs: Second automaton.
    :return: Intersection of lhs and rhs.
    """
    result = Nfa()
    mata_nfa.c_intersection(
        result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), preserve_epsilon, NULL
    )
    return result

def intersection_with_product_map(Nfa lhs, Nfa rhs, preserve_epsilon: bool = False):
    """Performs intersection of lhs and rhs.

    Supports epsilon symbols when preserve_epsilon is set to True.

    When computing intersection preserving epsilon transitions, create product of two NFAs, where both automata can contain ε-transitions. The product preserves the ε-transitions
     of both automata. This means that for each ε-transition of the form `s-ε->p` and each product state `(s,a)`,
     an ε-transition `(s,a)-ε->(p,a)` is created. Furthermore, for each ε-transition `s-ε->p` and `a-ε->b`,
     a product state `(s,a)-ε->(p,b)` is created.

    Automata must share alphabets.

    :param preserve_epsilon: Whether to compute intersection preserving epsilon transitions.
    :param Nfa lhs: First automaton.
    :param Nfa rhs: Second automaton.
    :return: Intersection of lhs and rhs, product map of original pairs of states to new states.
    """
    result = Nfa()
    cdef umap[pair[State, State], State] c_product_map
    mata_nfa.c_intersection(
        result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), preserve_epsilon,
        &c_product_map
    )
    return result, {tuple(k): v for k, v in c_product_map}

def concatenate(Nfa lhs, Nfa rhs, use_epsilon: bool = False) -> Nfa:
    """Concatenate two NFAs.

    Supports epsilon symbols when @p use_epsilon is set to true.
    :param Nfa lhs: First automaton to concatenate.
    :param Nfa rhs: Second automaton to concatenate.
    :param use_epsilon: Whether to concatenate over an epsilon symbol.
    :return: Nfa: Concatenated automaton.
    """
    result = Nfa()
    mata_nfa.c_concatenate(result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()),
                         use_epsilon, NULL, NULL)
    return result

def concatenate_with_result_state_maps(Nfa lhs, Nfa rhs, use_epsilon: bool = False) \
        -> tuple[Nfa, dict[str, str], dict[str, str]]:
    """Concatenate two NFAs.

    :param Nfa lhs: First automaton to concatenate.
    :param Nfa rhs: Second automaton to concatenate.
    :param use_epsilon: Whether to concatenate over an epsilon symbol.
    :return: Nfa: Concatenated automaton.
    """
    result = Nfa()
    cdef StateToStateMap c_lhs_map
    cdef StateToStateMap c_rhs_map
    mata_nfa.c_concatenate(result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()),
                         use_epsilon, &c_lhs_map, &c_rhs_map)
    lhs_map = {}
    for key, value in c_lhs_map:
        lhs_map[key] = value
    rhs_map = {}
    for key, value in c_rhs_map:
        rhs_map[key] = value

    return result, lhs_map, rhs_map


def complement(Nfa lhs, alph.Alphabet alphabet, params = None):
    """Performs complement of lhs

    :param Nfa lhs: complemented automaton
    :param OnTheFlyAlphabet alphabet: alphabet of the lhs
    :param dict params: additional params
      - "algorithm": "classical" (classical algorithm determinizes the automaton, makes it complete and swaps final and non-final states);
      - "minimize": "true"/"false" (whether to compute minimal deterministic automaton for classical algorithm);
    :return: complemented automaton
    """
    result = Nfa()
    params = params or {'algorithm': 'classical', 'minimize': 'false'}
    mata_nfa.c_complement(
        result.thisptr.get(),
        dereference(lhs.thisptr.get()),
        <CAlphabet&>dereference(alphabet.as_base()),
        {
            k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
            for k, v in params.items()
        },
    )
    return result

def make_complete(Nfa lhs, State sink_state, alph.Alphabet alphabet):
    """Makes lhs complete

    :param Nfa lhs: automaton that will be made complete
    :param Symbol sink_state: sink state of the automaton
    :param OnTheFlyAlphabet alphabet: alphabet of the
    """
    if not lhs.thisptr.get().is_state(sink_state):
        lhs.thisptr.get().add_state(lhs.size())
    mata_nfa.c_make_complete(lhs.thisptr.get(), <CAlphabet&>dereference(alphabet.as_base()), sink_state)

def revert(Nfa lhs):
    """Reverses transitions in the lhs

    :param Nfa lhs: source automaton
    :return: automaton with reversed transitions
    """
    result = Nfa()
    mata_nfa.c_revert(result.thisptr.get(), dereference(lhs.thisptr.get()))
    return result

def remove_epsilon(Nfa lhs, Symbol epsilon = CEPSILON):
    """Removes transitions that contain epsilon symbol.

    :param Nfa lhs: Automaton, where epsilon transitions will be removed.
    :param Symbol epsilon: Symbol representing the epsilon.
    :return: Nfa: Automaton with epsilon transitions removed.
    """
    result = Nfa()
    mata_nfa.c_remove_epsilon(result.thisptr.get(), dereference(lhs.thisptr.get()), epsilon)
    return result

def minimize(Nfa lhs):
    """Minimizes the automaton lhs

    :param Nfa lhs: automaton to be minimized
    :return: minimized automaton
    """
    result = Nfa()
    mata_nfa.c_minimize(result.thisptr.get(), dereference(lhs.thisptr.get()))
    return result

def reduce_with_state_map(Nfa aut, bool trim_input = True, params = None):
    """Reduce the automaton.

    :param Nfa aut: Original automaton to reduce.
    :param bool trim_input: Whether to trim the input automaton first or not.
    :param Dict params: Additional parameters for the reduction algorithm:
        - "algorithm": "simulation"
    :return: (Reduced automaton, state map of original to new states)
    """
    params = params or {"algorithm": "simulation"}
    cdef StateToStateMap state_map
    result = Nfa()
    mata_nfa.c_reduce(result.thisptr.get(), dereference(aut.thisptr.get()), trim_input, &state_map,
                    {
                        k.encode('utf-8'): v.encode('utf-8') for k, v in params.items()
                    }
                    )

    return result, {k: v for k, v in state_map}

def reduce(Nfa aut, bool trim_input = True, params = None):
    """Reduce the automaton.

    :param bool trim_input: Whether to trim the input automaton first or not.
    :param Nfa aut: Original automaton to reduce.
    :param Dict params: Additional parameters for the reduction algorithm:
        - "algorithm": "simulation"
    :return: Reduced automaton
    """
    params = params or {"algorithm": "simulation"}
    result = Nfa()
    mata_nfa.c_reduce(result.thisptr.get(), dereference(aut.thisptr.get()), trim_input, NULL,
                    {
                        k.encode('utf-8'): v.encode('utf-8') for k, v in params.items()
                    }
                    )
    return result

def compute_relation(Nfa lhs, params = None):
    """Computes the relation for the automaton

    :param Nfa lhs: automaton
    :param Dict params: parameters of the computed relation
    :return: computd relation
    """
    params = params or {'relation': 'simulation', 'direction': 'forward'}
    cdef CBinaryRelation relation = mata_nfa.c_compute_relation(
        dereference(lhs.thisptr.get()),
        {
            k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
            for k, v in params.items()
        }
    )
    result = BinaryRelation()
    cdef size_t relation_size = relation.size()
    result.resize(relation_size)
    for i in range(0, relation_size):
        for j in range(0, relation_size):
            val = relation.get(i, j)
            result.set(i, j, val)
    return result

# Tests
def is_deterministic(Nfa lhs):
    """Tests if the lhs is determinstic

    :param Nfa lhs: non-determinstic finite automaton
    :return: true if the lhs is deterministic
    """
    return mata_nfa.c_is_deterministic(dereference(lhs.thisptr.get()))

def is_lang_empty(Nfa lhs, Run run = None):
    """Checks if language of automaton lhs is empty, if not, returns path of states as counter
    example.

    :param Nfa lhs:
    :return: true if the lhs is empty, counter example if lhs is not empty
    """
    if run:
        return mata_nfa.c_is_lang_empty(dereference(lhs.thisptr.get()), run.thisptr)
    else:
        return mata_nfa.c_is_lang_empty(dereference(lhs.thisptr.get()), NULL)

def is_universal(Nfa lhs, alph.Alphabet alphabet, params = None):
    """Tests if lhs is universal wrt given alphabet

    :param Nfa lhs: automaton tested for universality
    :param OnTheFlyAlphabet alphabet: on the fly alphabet
    :param dict params: additional params to the function, currently supports key 'algorithm',
        which determines used universality test
    :return: true if lhs is universal
    """
    params = params or {'algorithm': 'antichains'}
    return mata_nfa.c_is_universal(
        dereference(lhs.thisptr.get()),
        <CAlphabet&>dereference(alphabet.as_base()),
        {
            k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
            for k, v in params.items()
        }
    )

def is_included_with_cex(Nfa lhs, Nfa rhs, alph.Alphabet alphabet = None, params = None):
    """Test inclusion between two automata

    :param Nfa lhs: smaller automaton
    :param Nfa rhs: bigger automaton
    :param alph.Alphabet alphabet: alpabet shared by two automata
    :param dict params: additional params
    :return: true if lhs is included by rhs, counter example word if not
    """
    run = Run()
    cdef CAlphabet* c_alphabet = NULL
    if alphabet:
        c_alphabet = alphabet.as_base()
    params = params or {'algorithm': 'antichains'}
    result = mata_nfa.c_is_included(
        dereference(lhs.thisptr.get()),
        dereference(rhs.thisptr.get()),
        run.thisptr,
        c_alphabet,
        {
            k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
            for k, v in params.items()
        }
    )
    return result, run

def is_included(Nfa lhs, Nfa rhs, alph.Alphabet alphabet = None, params = None):
    """Test inclusion between two automata

    :param Nfa lhs: smaller automaton
    :param Nfa rhs: bigger automaton
    :param alph.Alphabet alphabet: alpabet shared by two automata
    :param dict params: additional params
    :return: true if lhs is included by rhs, counter example word if not
    """
    cdef CAlphabet* c_alphabet = NULL
    if alphabet:
        c_alphabet = alphabet.as_base()
    params = params or {'algorithm': 'antichains'}
    result = mata_nfa.c_is_included(
        dereference(lhs.thisptr.get()),
        dereference(rhs.thisptr.get()),
        NULL,
        c_alphabet,
        {
            k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
            for k, v in params.items()
        }
    )
    return result

def equivalence_check(Nfa lhs, Nfa rhs, alph.Alphabet alphabet = None, params = None) -> bool:
    """Test equivalence of two automata.

:param Nfa lhs: Smaller automaton.
:param Nfa rhs: Bigger automaton.
:param alph.Alphabet alphabet: Alphabet shared by two automata.
:param dict params: Additional params:
- "algorithm": "antichains"
:return: True if lhs is equivalent to rhs, False otherwise.
"""
    params = params or {'algorithm': 'antichains'}
    cdef CAlphabet * c_alphabet = NULL
    if alphabet:
        c_alphabet = alphabet.as_base()
        return mata_nfa.c_are_equivalent(
            dereference(lhs.thisptr.get()),
            dereference(rhs.thisptr.get()),
            c_alphabet,
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            }
        )
    else:
        return mata_nfa.c_are_equivalent(
            dereference(lhs.thisptr.get()),
            dereference(rhs.thisptr.get()),
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            }
        )

def is_complete(Nfa lhs, alph.Alphabet alphabet):
    """Test if automaton is complete

    :param Nf lhs: tested automaton
    :param OnTheFlyAlphabet alphabet: alphabet of the automaton
    :return: true if the automaton is complete
    """
    return mata_nfa.c_is_complete(
        dereference(lhs.thisptr.get()),
        <CAlphabet&>dereference(alphabet.as_base())
    )

def is_in_lang(Nfa lhs, vector[Symbol] word):
    """Tests if word is in language

    :param Nfa lhs: tested automaton
    :param vector[Symbol] word: tested word
    :return: true if word is in language of automaton lhs
    """
    run = Run()
    run.thisptr.word = word
    return mata_nfa.c_is_in_lang(dereference(lhs.thisptr.get()), dereference(run.thisptr))

def is_prefix_in_lang(Nfa lhs, vector[Symbol] word):
    """Test if any prefix of the word is in the language

    :param Nfa lhs: tested automaton
    :param vector[Symbol] word: tested word
    :return: true if any prefix of word is in language of automaton lhs
    """
    run = Run()
    run.thisptr.word = word
    return mata_nfa.c_is_prfx_in_lang(dereference(lhs.thisptr.get()), dereference(run.thisptr))

def accepts_epsilon(Nfa lhs):
    """Tests if automaton accepts epsilon

    :param Nfa lhs: tested automaton
    :return: true if automaton accepts epsilon
    """
    for state in lhs.thisptr.get().initial:
        if lhs.has_final_state(state):
            return True
    return False

# Helper functions
def get_word_for_path(Nfa lhs, path):
    """For a given path (set of states) returns a corresponding word

    >>> mata_nfa.Nfa.get_word_for_path(lhs, [0, 1, 2])
    ([1, 1], True)

    :param Nfa lhs: source automaton
    :param list path: list of states
    :return: pair of word (list of symbols) and true or false, whether the search was successful
    """
    cdef pair[CRun, bool] result
    input = Run()
    input.path = path
    result = mata_nfa.c_get_word_for_path(dereference(lhs.thisptr.get()), dereference(input.thisptr))
    return result.first.word, result.second

def encode_word(string_to_symbol, word):
    """Encodes word based on a string to symbol map

    >>> mata_nfa.Nfa.encode_word({'a': 1, 'b': 2, "c": 0}, "abca")
    [1, 2, 0, 1]

    :param dict string_to_symbol: dictionary of strings to integers
    :param word: list of strings representing a encoded word
    :return:
    """
    result = mata_nfa.c_encode_word(
        {k.encode('utf-8'): v for (k, v) in string_to_symbol.items()},
        [s.encode('utf-8') for s in word]
    )
    return result.word

cdef subset_map_to_dictionary(umap[StateSet, State] subset_map):
    """Helper function that translates the unordered map to dictionary

    :param umap[StateSet, State]: map of state sets to states
    :return: subset_map as dictionary
    """
    result = {}
    cdef umap[StateSet, State].iterator it = subset_map.begin()
    while it != subset_map.end():
        key = dereference(it).first.ToVector()
        value = dereference(it).second
        result[tuple(sorted(key))] = value
        postinc(it)
    return result


# Temporary for testing
def divisible_by(k: int):
    """Constructs automaton accepting strings containing ones divisible by "k"."""
    assert k > 1
    lhs = Nfa(k+1)
    lhs.make_initial_state(0)
    lhs.add_transition(0, 0, 0)
    for i in range(1, k + 1):
        lhs.add_transition(i - 1, 1, i)
        lhs.add_transition(i, 0, i)
    lhs.add_transition(k, 1, 1)
    lhs.make_final_state(k)
    return lhs


def run_safely_external_command(cmd: str, check_results=True, quiet=True, timeout=None, **kwargs):
    """Safely runs the piped command, without executing of the shell

    Courtesy of: https://blog.avinetworks.com/tech/python-best-practices

    :param str cmd: string with command that we are executing
    :param bool check_results: check correct command exit code and raise exception in case of fail
    :param bool quiet: if set to False, then it will print the output of the command
    :param int timeout: timeout of the command
    :param dict kwargs: additional args to subprocess call
    :return: returned standard output and error
    :raises subprocess.CalledProcessError: when any of the piped commands fails
    """
    # Split
    unpiped_commands = list(map(str.strip, cmd.split(" | ")))
    cmd_no = len(unpiped_commands)

    # Run the command through pipes
    objects: List[subprocess.Popen] = []
    for i in range(cmd_no):
        executed_command = shlex.split(unpiped_commands[i])

        # set streams
        stdin = None if i == 0 else objects[i-1].stdout
        stderr = subprocess.STDOUT if i < (cmd_no - 1) else subprocess.PIPE

        # run the piped command and close the previous one
        piped_command = subprocess.Popen(
            executed_command,
            shell=False, stdin=stdin, stdout=subprocess.PIPE, stderr=stderr, **kwargs
        )
        if i != 0:
            objects[i-1].stdout.close()  # type: ignore
        objects.append(piped_command)

    try:
        # communicate with the last piped object
        cmdout, cmderr = objects[-1].communicate(timeout=timeout)

        for i in range(len(objects) - 1):
            objects[i].wait(timeout=timeout)

    except subprocess.TimeoutExpired:
        for process in objects:
            process.terminate()
        raise

    # collect the return codes
    if check_results:
        for i in range(cmd_no):
            if objects[i].returncode:
                if not quiet and (cmdout or cmderr):
                    print(f"captured stdout: {cmdout.decode('utf-8')}", 'red')
                    print(f"captured stderr: {cmderr.decode('utf-8')}", 'red')
                raise subprocess.CalledProcessError(
                    objects[i].returncode, unpiped_commands[i]
                )

    return cmdout, cmderr


def get_elements_from_bool_vec(bool_vec: list[int]):
    cdef CBoolVector c_bool_vec = CBoolVector(bool_vec)
    cdef StateSet c_states
    mata_nfa.get_elements(&c_states, c_bool_vec)
    return { state for state in c_states }

# On the fly configuration of the library
_store = {
    'node_style': {
        "style": "filled",
        "color": "darkblue",
        "fillcolor": "lightsteelblue",
        "fontname": "Courier-Bold",
        "width": "0.3",
        "height": "0.3",
        "fontsize": "12",
        "fixedsize": "true",
        "penwidth": "1.5",
    },
    'edge_style': {
        "penwidth": "1.5",
        "color": "midnightblue",
    },
    'alphabet': None,
}


def store():
    """
    Returns the configuration of the library
    """
    return _store