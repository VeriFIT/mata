# Python bindings for the automata library Mata

This folder contains Python bindings for the automata library Mata.

## Threads

The long-running operations (determinization, intersection, complement, minimization, reduction, inclusion and
equivalence checking, noodlification, ...) release the GIL, so several Python threads can run them in parallel.

The C++ library does not synchronize anything. The automata and alphabets passed to a call may be read from several
threads at once, but no thread may modify them while another thread is using them. Note that translating a symbol
through an `OnTheFlyAlphabet` inserts it into the alphabet, so one `OnTheFlyAlphabet` shared between threads is not
safe to translate symbols on concurrently.

## Contributing

For information on contributing to the Python binding, refer to [CONTRIBUTING.md](CONTRIBUTING.md).
