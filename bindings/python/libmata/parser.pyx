from libcpp.memory cimport shared_ptr, make_shared

from libmata.nfa.nfa cimport CNfa

cimport libmata.parser as parser
cimport libmata.nfa.nfa as mata_nfa

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