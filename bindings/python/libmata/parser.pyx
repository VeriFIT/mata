from libcpp.memory cimport shared_ptr, make_shared
from libcpp.string cimport string

from libmata.nfa.nfa cimport CNfa, CEPSILON

cimport libmata.parser as parser
cimport libmata.nfa.nfa as mata_nfa
cimport libmata.alphabets as alph
from libmata.alphabets import IntAlphabet

# External Constructors
def from_regex(regex, encoding='utf-8', bool reduce=True, bool trim=True, bool keep_epsilon=False):
    """Creates automaton from the regular expression

    The format of the regex conforms to google RE2 regular expression library.
    By default, the automaton has no epsilon transitions, is trimmed and reduced.

    :param str regex: regular expression
    :param str encoding: encoding of the string
    :param bool reduce: whether to reduce the automaton using simulation reduction; costly for large regexes
    :param bool trim: whether to trim the automaton
    :param bool keep_epsilon: whether to keep epsilon transitions (labelled with EPSILON) in the automaton
    :return: Nfa automaton
    """
    cdef parser.CRe2Options options
    options.keep_epsilon = keep_epsilon
    options.epsilon = CEPSILON
    options.trim = trim
    options.reduce = reduce
    result = mata_nfa.Nfa()
    parser.create_nfa((<mata_nfa.Nfa>result).thisptr.get(), regex.encode(encoding), options)
    return result

def from_mata(src: str | list[str], alph.Alphabet alphabet):
    """Loads automata from either single file or list or other stream of files.
    
    All automata passed in sources are mintermized together. If you wish to load 
    multiple automata and mintermize them, then you have to pass them as a list.

    :param src: list of paths to automata or single path
    :param alphabet: target alphabet; None reads the symbols as numbers
    :return: single automaton or list of automata
    """
    cdef CAlphabet * c_alphabet = NULL
    cdef optional[CNfa] explicit_aut
    cdef parser.ifstream fs
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
    automata = []
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
            result = mata_nfa.Nfa()
            parser.construct(
                (<mata_nfa.Nfa>result).thisptr.get(), parser.parse_from_mf(parser.parse_mf(fs, True))[0], c_alphabet
            )
            automata.append(result)
    if None in automata:
        bitvector_auts = bitvector_reader.build(c_alphabet)
    for i, aut in enumerate(automata):
        if aut is None:
            result = mata_nfa.Nfa()
            (<mata_nfa.Nfa>result).thisptr = make_shared[CNfa](bitvector_auts[next_bitvector_aut])
            next_bitvector_aut += 1
            automata[i] = result
    return automata