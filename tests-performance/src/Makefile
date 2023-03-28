###############################################################################
#
#                          Makefile for MATA examples
#
###############################################################################

CFLAGS=-std=c++17 \
  -Wextra \
  -Wall \
  -Wfloat-equal \
  -Wctor-dtor-privacy \
  -Weffc++ \
  -Woverloaded-virtual \
  -fdiagnostics-show-option \
	-g


INCLUDE=-I../../include -I../../3rdparty/simlib/include -I../../3rdparty/re2/include -I ../../3rdparty -I ../../3rdparty/cudd/cudd

LIBS_ADD=-L../../build/src -L../../build/3rdparty/re2 -L../../build/3rdparty/simlib -L../../build/3rdparty/cudd/lib

LIBS=-lmata -lsimlib -lre2 -lcudd


###############################################################################

.PHONY: all clean

all: $(patsubst %.cc,%,$(wildcard *.cc)) ../../build/src/libmata.a

%: %.cc
	g++ $(CFLAGS) $(INCLUDE) $(LIBS_ADD) $< $(LIBS) -o ../bin/$@

clean:
	rm -rf $(patsubst %.cc,%,$(wildcard *.cc))
