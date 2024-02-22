from libcpp.memory cimport shared_ptr, make_shared
from libcpp.string cimport string

from libmata.nfa.nfa cimport CNfa

cimport libmata.parser as parser
cimport libmata.nfa.nfa as mata_nfa
cimport libmata.alphabets as alph

# External Constructors
def from_regex(regex, encoding='utf-8'):
    """Creates automaton from the regular expression

    The format of the regex conforms to google RE2 regular expression library.

    :param str regex: regular expression
    :param str encoding: encoding of the string
    :return: Nfa automaton
    """
    result = mata_nfa.Nfa()
    parser.create_nfa((<mata_nfa.Nfa>result).thisptr.get(), regex.encode(encoding))
    return result

def from_mata(src: str | list[str], alph.Alphabet alphabet):
    """Loads automata from either single file or list or other stream of files.
    
    All automata passed in sources are mintermized together. If you wish to load 
    multiple automata and mintermize them, then you have to pass them as a list.

    :param src: list of paths to automata or single path
    :param alphabet: target alphabet
    :return: single automaton or list of automata
    """
    cdef CAlphabet * c_alphabet = NULL
    cdef parser.Parsed parsed
    cdef parser.ifstream fs
    cdef vector[parser.CInterAut] inter_aut
    cdef vector[parser.CInterAut] res_inter_aut
    cdef vector[parser.CInterAut] mintermized_inter_aut
    cdef parser.CMintermization mintermization

    if alphabet:
        c_alphabet = alphabet.as_base()

    # either load single automata
    if isinstance(src, str):
        fs = parser.ifstream(src.encode('utf-8'))
        res_inter_aut = parser.parse_from_mf(parser.parse_mf(fs, True))
        result = mata_nfa.Nfa()
        if res_inter_aut[0].is_bitvector():
            parser.construct(
                (<mata_nfa.Nfa>result).thisptr.get(),
                mintermization.c_mintermize(res_inter_aut[0]),
                c_alphabet
            )
        else:
            parser.construct(
                (<mata_nfa.Nfa>result).thisptr.get(),
                res_inter_aut[0],
                c_alphabet
            )
        return result
    # load multiple automata
    else:
        automata = []
        for file in src:
            fs = parser.ifstream(file.encode('utf-8'))
            res_inter_aut = parser.parse_from_mf(parser.parse_mf(fs, True))
            inter_aut.emplace_back(res_inter_aut[0])
        if inter_aut[0].is_bitvector():
            mintermized_inter_aut = mintermization.c_mintermize_vec(inter_aut)
        else:
            mintermized_inter_aut = inter_aut
        for ia in mintermized_inter_aut:
            result = mata_nfa.Nfa()
            parser.construct(
                (<mata_nfa.Nfa>result).thisptr.get(),
                ia,
                c_alphabet
            )
            automata.append(result)
        return automata