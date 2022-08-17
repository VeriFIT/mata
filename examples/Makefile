###############################################################################
#
#                          Makefile for MATA examples
#
###############################################################################

CFLAGS=-std=c++14 \
  -pedantic-errors \
  -Wextra \
  -Wall \
  -Wfloat-equal \
  -Wctor-dtor-privacy \
  -Weffc++ \
  -Woverloaded-virtual \
  -fdiagnostics-show-option \
	-g


INCLUDE=-I../include -I../3rdparty/simlib/include -I../3rdparty/re2/include

LIBS_ADD=-L../build/src -L../build/3rdparty/re2 -L../build/3rdparty/simlib

LIBS=-lmata -lsimlib -lre2


###############################################################################

.PHONY: all clean

all: $(patsubst %.cc,%,$(wildcard *.cc)) ../build/src/libmata.a

%: %.cc
	g++ $(CFLAGS) $(INCLUDE) $(LIBS_ADD) $< $(LIBS) -o $@

clean:
	rm -rf $(patsubst %.cc,%,$(wildcard *.cc))
