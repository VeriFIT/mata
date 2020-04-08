#!/usr/bin/env python3
import ctypes
import ctypes.util
import unittest

VATALIB_NAME="../build/python_bind/libvata2-c-ifc"

# global variable carrying reference to the loaded DLL
g_vatalib = None

# do this when the module is loaded
vatalib_path = ctypes.util.find_library(VATALIB_NAME)
if not vatalib_path:
    raise Exception("Unable to find the library {}".format(VATALIB_NAME))
try:
    g_vatalib = ctypes.CDLL(vatalib_path)
except OSError:
    raise Exception("Unable to load the library {}".format(VATALIB_NAME))


###########################################
class NFA:
    """A Python wrapper over a VATA NFA."""
    aut = None

    def __init__(self):
        """The constructor"""
        self.aut = g_vatalib.nfa_init()

    def __del__(self):
        """The destructor"""
        g_vatalib.nfa_free(self.aut)

    def addInitial(self, state):
        """Adds an initial state"""
        assert type(state) == int
        g_vatalib.nfa_add_initial(self.aut, state)

    def isInitial(self, state):
        """Checks whether a state is initial"""
        assert type(state) == int
        return True if g_vatalib.nfa_is_initial(self.aut, state) else False


###########################################
class NFATest(unittest.TestCase):


    def test_init_free(self):
        """Testing construction and destruction"""
        self.assertEqual(g_vatalib.nfa_library_size(), 0)
        aut = NFA()
        self.assertEqual(g_vatalib.nfa_library_size(), 1)
        del aut
        self.assertEqual(g_vatalib.nfa_library_size(), 0)


    def test_initial_states(self):
        """Testing adding and checking of initial states"""
        aut = NFA()
        self.assertFalse(aut.isInitial(42))
        aut.addInitial(42)
        self.assertTrue(aut.isInitial(42))


    def test_union(params):
        assert False
        pass



###########################################
if __name__ == '__main__':
    unittest.main()

