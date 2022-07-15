cimport pynfa
from libcpp.vector cimport vector
from libcpp.list cimport list as clist
from cython.operator import dereference, postincrement as postinc, preincrement as preinc
from libcpp.unordered_map cimport unordered_map as umap

import shlex
import subprocess

cdef class Trans:
    """
    Wrapper over the transitions
    """
    cdef pynfa.CTrans *thisptr

    def __cinit__(self, State src=0, Symbol s=0, State tgt=0):
        self.thisptr = new pynfa.CTrans(src, s, tgt)

    def __dealloc__(self):
        if self.thisptr != NULL:
            del self.thisptr

    def __eq__(self, Trans other):
        return dereference(self.thisptr) == dereference(other.thisptr)

    def __neq__(self, Trans other):
        return dereference(self.thisptr) != dereference(other.thisptr)

    cdef copy_from(self, CTrans trans):
        """Copies the internals of trans into the wrapped pointer

        :param CTrans trans: copied transition
        """
        self.thisptr.src = trans.src
        self.thisptr.symb = trans.symb
        self.thisptr.tgt = trans.tgt

    def __str__(self):
        return f"{self.thisptr.src}-[{self.thisptr.symb}]\u2192{self.thisptr.tgt}"

    def __repr__(self):
        return str(self)

cdef class Nfa:
    """
    Wrapper over NFA
    """
    cdef pynfa.CNfa *thisptr

    def __cinit__(self, state_number = 0):
        self.thisptr = new pynfa.CNfa(state_number)

    def __dealloc__(self):
        del self.thisptr

    def add_initial_state(self, State st):
        self.thisptr.add_initial(st)

    def add_initial_states(self, vector[State] states):
        self.thisptr.add_initial(states)

    def has_initial_state(self, State st):
        return self.thisptr.has_initial(st)

    def add_final_state(self, State st):
        self.thisptr.add_final(st)

    def has_final_state(self, State st):
        return self.thisptr.has_final(st)

    def state_size(self):
        return self.thisptr.get_num_of_states()

    def add_trans(self, Trans tr):
        self.thisptr.add_trans(dereference(tr.thisptr))

    def add_trans_raw(self, State src, Symbol symb, State tgt):
        self.thisptr.add_trans(src, symb, tgt)

    def has_trans(self, Trans tr):
        return self.thisptr.has_trans(dereference(tr.thisptr))

    def has_trans_raw(self, State src, Symbol symb, State tgt):
        return self.thisptr.has_trans(src, symb, tgt)

    def trans_empty(self):
        return self.thisptr.nothing_in_trans()

    def trans_size(self):
        return self.thisptr.trans_size()

    def iterate(self):
        iterator = self.thisptr.begin()
        while iterator != self.thisptr.end():
            trans = Trans()
            lhs = dereference(iterator)
            trans.copy_from(lhs)
            preinc(iterator)
            yield trans

    def to_dot_file(self, output_file='aut.dot', output_format='pdf'):
        """Transforms the automaton to dot format.

        By default the result is saved to `aut.dot`, and further to `aut.dot.pdf`.

        One can choose other format that is supported by graphviz format (e.g. pdf or png).

        :param str output_file: name of the output file where the automaton will be stored
        :param str output_format: format of the output file (pdf/png/etc)
        """
        cdef pynfa.ofstream* output
        output = new pynfa.ofstream(output_file.encode('utf-8'))
        try:
            self.thisptr.print_to_DOT(dereference(output))
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
        cdef pynfa.stringstream* output_stream
        output_stream = new pynfa.stringstream("")
        cdef string result
        try:
            self.thisptr.print_to_DOT(dereference(output_stream))
            result = output_stream.str()
        finally:
            del output_stream
        return result.decode(encoding)

    def post_map_of(self, State st, OnTheFlyAlphabet alphabet):
        """Returns mapping of symbols to set of states.

        :param State st: source state
        :return: dictionary mapping symbols to set of reachable states from the symbol
        """
        symbol_map = {}
        for symbol in alphabet.get_symbols():
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
        return_value = self.thisptr.post(input_states, symbol).ToVector()
        return {v for v in return_value}

    # External Constructors
    @classmethod
    def from_regex(cls, regex, encoding='utf-8'):
        """Creates automaton from the regular expression

        The format of the regex conforms to google RE2 regular expression library.

        :param str regex: regular expression
        :param str encoding: encoding of the string
        :return: Nfa automaton
        """
        result = Nfa()
        pynfa.create_nfa(result.thisptr, regex.encode(encoding))
        return result

    # Operations
    @classmethod
    def determinize(cls, Nfa lhs):
        """Determinize the lhs automaton

        :param Nfa lhs: non-deterministic finite automaton
        :return: deterministic finite automaton, subset map
        """
        result = Nfa()
        cdef SubsetMap subset_map
        pynfa.determinize(result.thisptr, dereference(lhs.thisptr), &subset_map)
        return result, subset_map_to_dictionary(subset_map)

    @classmethod
    def union(cls, Nfa lhs, Nfa rhs):
        """Performs union of lhs and rhs

        :param Nfa lhs: first automaton
        :param Nfa rhs: second automaton
        :return: union of lhs and rhs
        """
        result = Nfa()
        pynfa.uni(
            result.thisptr, dereference(lhs.thisptr), dereference(rhs.thisptr)
        )
        return result

    @classmethod
    def intersection(cls, Nfa lhs, Nfa rhs):
        """Performs intersection of lhs and rhs

        :param Nfa lhs: first automaton
        :param Nfa rhs: second automaton
        :return: intersection of lhs and rhs, product map of the results
        """
        result = Nfa()
        cdef ProductMap product_map
        pynfa.intersection(
            result.thisptr, dereference(lhs.thisptr), dereference(rhs.thisptr), &product_map
        )
        return result, {tuple(sorted(k)): v for k, v in product_map}

    @classmethod
    def complement(cls, Nfa lhs, OnTheFlyAlphabet alphabet, params = None):
        """Performs complement of lhs

        :param Nfa lhs: complemented automaton
        :param OnTheFlyAlphabet alphabet: alphabet of the lhs
        :param dict params: additional params
        :return: complemented automaton, map of subsets to states
        """
        result = Nfa()
        params = params or {'algo': 'classical'}
        cdef SubsetMap subset_map
        pynfa.complement(
            result.thisptr,
            dereference(lhs.thisptr),
            <CAlphabet&>dereference(alphabet.thisptr),
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            },
            &subset_map
        )
        return result, subset_map_to_dictionary(subset_map)

    @classmethod
    def make_complete(cls, Nfa lhs, State sink_state, OnTheFlyAlphabet alphabet):
        """Makes lhs complete

        :param Nfa lhs: automaton that will be made complete
        :param Symbol sink_state: sink state of the automaton
        :param OnTheFlyAlphabet alphabet: alphabet of the
        """
        if not lhs.thisptr.is_state(sink_state):
            lhs.thisptr.increase_size(lhs.state_size() + 1)
        pynfa.make_complete(lhs.thisptr, <CAlphabet&>dereference(alphabet.thisptr), sink_state)

    @classmethod
    def revert(cls, Nfa lhs):
        """Reverses transitions in the lhs

        :param Nfa lhs: source automaton
        :return: automaton with reversed transitions
        """
        result = Nfa()
        pynfa.revert(result.thisptr, dereference(lhs.thisptr))
        return result

    @classmethod
    def remove_epsilon(cls, Nfa lhs, Symbol epsilon):
        """Removes transitions that contains epsilon symbol

        TODO: Possibly there may be issue with setting the size of the automaton beforehand?

        :param Nfa lhs: automaton, where epsilon transitions will be removed
        :param Symbol epsilon: symbol representing the epsilon
        :return: automaton, with epsilon transitions removed
        """
        result = Nfa(lhs.state_size())
        pynfa.remove_epsilon(
            result.thisptr, dereference(lhs.thisptr), epsilon
        )
        return result

    @classmethod
    def minimize(cls, Nfa lhs):
        """Minimies the automaton lhs

        :param Nfa lhs: automaton to be minimized
        :return: minimized automaton
        """
        result = Nfa()
        pynfa.minimize(result.thisptr, dereference(lhs.thisptr))
        return result

    # Tests
    @classmethod
    def is_deterministic(cls, Nfa lhs):
        """Tests if the lhs is determinstic

        :param Nfa lhs: non-determinstic finite automaton
        :return: true if the lhs is deterministic
        """
        return pynfa.is_deterministic(dereference(lhs.thisptr))

    @classmethod
    def is_lang_empty_path_counterexample(cls, Nfa lhs):
        """Checks if language of automaton lhs is empty, if not, returns path of states as counter
        example.

        :param Nfa lhs:
        :return: true if the lhs is empty, counter example if lhs is not empty
        """
        cdef Path path
        result = pynfa.is_lang_empty(dereference(lhs.thisptr), &path)
        return result, path


    @classmethod
    def is_lang_empty_word_counterexample(cls, Nfa lhs):
        """Checks if language of automaton lhs is empty, if not, returns word as counter example.

        :param Nfa lhs:
        :return: true if the lhs is empty, counter example if lhs is not empty
        """
        cdef Word word
        result = pynfa.is_lang_empty_cex(dereference(lhs.thisptr), &word)
        return result, word

    @classmethod
    def is_universal(cls, Nfa lhs, OnTheFlyAlphabet alphabet, params = None):
        """Tests if lhs is universal wrt given alphabet

        :param Nfa lhs: automaton tested for universality
        :param OnTheFlyAlphabet alphabet: on the fly alphabet
        :param dict params: additional params to the function, currently supports key 'algo',
            which determines used universality test
        :return: true if lhs is universal
        """
        params = params or {'algo': 'antichains'}
        return pynfa.is_universal(
            dereference(lhs.thisptr),
            <CAlphabet&>dereference(alphabet.thisptr),
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            }
        )

    @classmethod
    def is_included(
            cls, Nfa lhs, Nfa rhs, OnTheFlyAlphabet alphabet, params = None
    ):
        """Test inclusion between two automata

        :param Nfa lhs: smaller automaton
        :param Nfa rhs: bigger automaton
        :param Alphabet alphabet: alpabet shared by two automata
        :param dict params: adtitional params
        :return: true if lhs is included by rhs, counter example word if not
        """
        cdef Word word
        params = params or {'algo': 'antichains'}
        result = pynfa.is_incl(
            dereference(lhs.thisptr),
            dereference(rhs.thisptr),
            <CAlphabet&>dereference(alphabet.thisptr),
            &word,
            {
                k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v
                for k, v in params.items()
            }
        )
        return result, word

    @classmethod
    def is_complete(cls, Nfa lhs, OnTheFlyAlphabet alphabet):
        """Test if automaton is complete

        :param Nf lhs: tested automaton
        :param OnTheFlyAlphabet alphabet: alphabet of the automaton
        :return: true if the automaton is complete
        """
        return pynfa.is_complete(
            dereference(lhs.thisptr),
            <CAlphabet&>dereference(alphabet.thisptr)
        )

    @classmethod
    def is_in_lang(cls, Nfa lhs, vector[Symbol] word):
        """Tests if word is in language

        :param Nfa lhs: tested automaton
        :param vector[Symbol] word: tested word
        :return: true if word is in language of automaton lhs
        """
        return pynfa.is_in_lang(dereference(lhs.thisptr), <Word> word)

    @classmethod
    def is_prefix_in_lang(cls, Nfa lhs, vector[Symbol] word):
        """Test if any prefix of the word is in the language

        :param Nfa lhs: tested automaton
        :param vector[Symbol] word: tested word
        :return: true if any prefix of word is in language of automaton lhs
        """
        return pynfa.is_prfx_in_lang(dereference(lhs.thisptr), <Word> word)

    @classmethod
    def accepts_epsilon(cls, Nfa lhs):
        """Tests if automaton accepts epsilon

        :param Nfa lhs: tested automaton
        :return: true if automaton accepts epsilon
        """
        cdef vector[State] initial_states = lhs.thisptr.initialstates.ToVector()
        for state in initial_states:
            if lhs.has_final_state(state):
                return True
        return False

    # Helper functions
    @classmethod
    def get_forward_reachable_states(cls, Nfa lhs, OnTheFlyAlphabet alphabet):
        """Returns list of reachable states from initial states

        WARNING: This is quite inefficient operation, that could be implemented better

        >>> pynfa.Nfa.get_forward_reachable_states(lhs)
        {0, 1, 2}

        :param Nfa lhs: source automaton
        :return: set of reachable states
        """
        cdef vector[State] initial_states = lhs.thisptr.initialstates.ToVector()
        reachable = {state for state in initial_states}
        worklist = reachable
        while worklist:
            state = worklist.pop()
            for post in lhs.post_map_of(state, alphabet).values():
                worklist.update({s for s in post if s not in reachable})
            reachable = reachable.union(worklist)
        return reachable


    @classmethod
    def get_word_for_path(cls, Nfa lhs, path):
        """For a given path (set of states) returns a corresponding word

        >>> pynfa.Nfa.get_word_for_path(lhs, [0, 1, 2])
        ([1, 1], True)

        :param Nfa lhs: source automaton
        :param list path: list of states
        :return: pair of word (list of symbols) and true or false, whether the search was successful
        """
        return pynfa.get_word_for_path(dereference(lhs.thisptr), path)

    @classmethod
    def encode_word(cls, string_to_symbol, word):
        """Encodes word based on a string to symbol map

        >>> pynfa.Nfa.encode_word({'a': 1, 'b': 2, "c": 0}, "abca")
        [1, 2, 0, 1]

        :param dict string_to_symbol: dictionary of strings to integers
        :param word: list of strings representing a encoded word
        :return:
        """
        return pynfa.encode_word(
            {k.encode('utf-8'): v for (k, v) in string_to_symbol.items()},
            [s.encode('utf-8') for s in word]
        )

cdef class CharAlphabet:
    cdef pynfa.CCharAlphabet *thisptr

    def __cinit__(self):
        self.thisptr = new pynfa.CCharAlphabet()

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        return self.thisptr.translate_symb(symbol.encode('utf-8'))


cdef class EnumAlphabet:
    cdef pynfa.CEnumAlphabet *thisptr

    def __cinit__(self, enums):
        cdef vector[string] enums_as_strings = [e.encode('utf-8') for e in enums]
        self.thisptr = new pynfa.CEnumAlphabet(enums_as_strings.begin(), enums_as_strings.end())

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        return self.thisptr.translate_symb(symbol.encode('utf-8'))


cdef class DirectAlphabet:
    cdef pynfa.CDirectAlphabet *thisptr

    def __cinit__(self):
        self.thisptr = new pynfa.CDirectAlphabet()

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        return self.thisptr.translate_symb(symbol.encode('utf-8'))


cdef class OnTheFlyAlphabet:
    cdef pynfa.COnTheFlyAlphabet *thisptr
    cdef StringToSymbolMap string_to_symbol_map

    def __cinit__(self, State initial_symbol = 0):
        self.thisptr = new pynfa.COnTheFlyAlphabet(&self.string_to_symbol_map, initial_symbol)

    def __dealloc__(self):
        del self.thisptr

    def translate_symbol(self, str symbol):
        return self.thisptr.translate_symb(symbol.encode('utf-8'))

    cdef get_symbols(self):
        cdef clist[Symbol] symbols = self.thisptr.get_symbols()
        return [s for s in symbols]

cdef subset_map_to_dictionary(SubsetMap subset_map):
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
    """
    Constructs automaton accepting strings containing ones divisible by "k"
    """
    assert k > 1
    lhs = Nfa(k+1)
    lhs.add_initial_state(0)
    lhs.add_trans_raw(0, 0, 0)
    for i in range(1, k + 1):
        lhs.add_trans_raw(i - 1, 1, i)
        lhs.add_trans_raw(i, 0, i)
    lhs.add_trans_raw(k, 1, 1)
    lhs.add_final_state(k)
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
