#!/usr/bin/env python3
import ctypes
import ctypes.util

VATALIB_NAME="../build/python_bind/libvata2-c-ifc"

###########################################
if __name__ == '__main__':
    vatalib_path = ctypes.util.find_library(VATALIB_NAME)
    if not vatalib_path:
        raise Exception("Unable to find the library {}".format(VATALIB_NAME))
    try:
        vatalib = ctypes.CDLL(vatalib_path)
    except OSError:
        raise Exception("Unable to load the library {}".format(VATALIB_NAME))

    vatalib.nfa_set_debug_level(4)
    print(vatalib.__str__())
    print(vatalib.nfa_init)
    aut1 = vatalib.nfa_init()
    print("aut1 = " + hex(aut1))
    aut2 = vatalib.nfa_init()
    print("aut2 = " + hex(aut2))
    vatalib.nfa_add_initial(aut1, 0)
    vatalib.nfa_add_initial(aut2, 1)
    vatalib.nfa_print(aut2)
    incl = vatalib.nfa_test_inclusion(aut1, aut2)
    print("Inclusion: " + str(incl))

