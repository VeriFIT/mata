from libcpp.memory cimport shared_ptr, make_shared
from libmata.nfa cimport CNfa

cimport libmata.nfa as mata
cimport libmata.parser as parser

# External Constructors
def from_regex(regex, encoding='utf-8'):
    """Creates automaton from the regular expression

    The format of the regex conforms to google RE2 regular expression library.

    :param str regex: regular expression
    :param str encoding: encoding of the string
    :return: Nfa automaton
    """
    result = mata.Nfa()
    #parser.create_nfa(result.as_ptr(), regex.encode(encoding))
    parser.create_nfa((<mata.Nfa>result).thisptr.get(), regex.encode(encoding))
    return result
