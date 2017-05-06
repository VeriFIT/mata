###############################################################################
#
#                          Makefile for VATA examples
#
###############################################################################

CFLAGS=-std=c++11 \
  -pedantic-errors \
  -Wextra \
  -Wall \
  -Wfloat-equal \
  -Wctor-dtor-privacy \
  -Weffc++ \
  -Woverloaded-virtual \
  -fdiagnostics-show-option \
	-g


INCLUDE=-I../include

LIBS_ADD=-L../build/src

LIBS=-lvata-ng


###############################################################################

.PHONY: all clean

all: $(patsubst %.cc,%,$(wildcard *.cc)) ../build/src/libvata-ng.a

%: %.cc
	g++ $(CFLAGS) $(INCLUDE) $(LIBS_ADD) $< $(LIBS) -o $@

clean:
	rm -rf $(patsubst %.cc,%,$(wildcard *.cc))
