from libcpp.memory cimport shared_ptr, make_shared
from libcpp.string cimport string

from libmata.nfa.nfa cimport CNfa, CEPSILON

cimport libmata.parser as parser
cimport libmata.nfa.nfa as mata_nfa
cimport libmata.alphabets as alph
from libmata.alphabets import IntAlphabet

# External Constructors
def from_regex(regex, encoding='utf-8', bool reduce=True, bool trim=True, bool keep_epsilon=False, long long max_mem=0):
    """Creates automaton from the regular expression

    The format of the regex conforms to google RE2 regular expression library.
    By default, the automaton has no epsilon transitions, is trimmed and reduced.

    :param str regex: regular expression
    :param str encoding: encoding of the string
    :param bool reduce: whether to reduce the automaton using simulation reduction; costly for large regexes
    :param bool trim: whether to trim the automaton
    :param bool keep_epsilon: whether to keep epsilon transitions (labelled with EPSILON) in the automaton
    :param int max_mem: approximate RE2 program memory budget in bytes. Non-positive values use RE2's default budget.
                        Applies to compilation, not to trimming or reduction.
    :return: Nfa automaton
    """
    cdef parser.CRe2Options options
    options.keep_epsilon = keep_epsilon
    options.epsilon = CEPSILON
    options.trim = trim
    options.reduce = reduce
    options.max_mem = max_mem
    result = mata_nfa.Nfa()
    parser.create_nfa((<mata_nfa.Nfa>result).thisptr.get(), regex.encode(encoding), options)
    return result

def from_mata(src: str | list[str], alph.Alphabet alphabet = None):
    """Loads automata from either single file or list or other stream of files.

    Every file must hold exactly one automaton, i.e., a single @-section.
    All automata passed in sources are mintermized together. To load multiple automata and mintermize them
    together, pass their paths as one iterable: a list, or any other iterable such as a generator.

    :param src: iterable of paths to automata or single path
    :param alphabet: target alphabet; when omitted, the symbols are read as numbers
    :raises ValueError: when @p src is an empty iterable
    :raises RuntimeError: when a file does not hold exactly one automaton
    :return: single automaton or list of automata
    """
    cdef CAlphabet * c_alphabet = NULL
    cdef optional[CNfa] explicit_aut
    cdef parser.ifstream fs
    cdef parser.Parsed parsed
    cdef parser.CBitvectorNfaReader bitvector_reader
    cdef vector[CNfa] bitvector_auts
    cdef size_t next_bitvector_aut = 0

    if alphabet is None:
        alphabet = IntAlphabet()
    c_alphabet = alphabet.as_base().get()

    # either load single automata; the path overload saves the whole-file copy the bytes roundtrip makes
    if isinstance(src, str):
        result = mata_nfa.Nfa()
        (<mata_nfa.Nfa>result).thisptr = make_shared[CNfa](
            parser.parse_from_mata_path(parser.cpath(src.encode('utf-8')), c_alphabet, NULL, True)
        )
        return result
    # load multiple automata: the inputs the fast reader of explicit alphabets reads are loaded right away, those with
    #  a bitvector alphabet go to the bitvector reader, which mintermizes them together, and the others go through the
    #  general reader
    if src is None:
        raise ValueError("from_mata() needs at least one source")
    automata = []
    # Iterate lazily: each source is opened before the next one is requested, so an error in a source surfaces
    #  before a later item of a generator is produced.
    for file in src:
        with open(file, 'rb') as handle:
            content = handle.read()
        explicit_aut = parser.parse_explicit_from_mata(content, c_alphabet, NULL, True)
        if explicit_aut.has_value():
            result = mata_nfa.Nfa()
            (<mata_nfa.Nfa>result).thisptr = make_shared[CNfa](explicit_aut.value())
            automata.append(result)
        elif bitvector_reader.add(content, True, NULL) != parser.Rejected:
            automata.append(None)
        else:
            fs = parser.ifstream(file.encode('utf-8'))
            parsed = parser.parse_mf(fs, True)
            # Both fast readers give up on a file of several sections, so only this reader sees one. Same check as
            #  builder::parse_from_mata() of the single path, naming the file.
            if parsed.size() != 1:
                raise RuntimeError(
                    f"The number of sections in the input file '{file}' is '{parsed.size()}'. Required is '1'."
                )
            result = mata_nfa.Nfa()
            parser.construct((<mata_nfa.Nfa>result).thisptr.get(), parser.parse_from_mf(parsed)[0], c_alphabet)
            automata.append(result)
    if not automata:
        raise ValueError("from_mata() needs at least one source")
    if None in automata:
        bitvector_auts = bitvector_reader.build(c_alphabet)
    for i, aut in enumerate(automata):
        if aut is None:
            result = mata_nfa.Nfa()
            (<mata_nfa.Nfa>result).thisptr = make_shared[CNfa](bitvector_auts[next_bitvector_aut])
            next_bitvector_aut += 1
            automata[i] = result
    return automata