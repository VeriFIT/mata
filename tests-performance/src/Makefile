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

INCLUDE_DIR=../../include
THIRD_PARTY_DIR=../../3rdparty
BUILD_DIR := $(shell \
	if [ -d "../../cmake-build-release" ]; then \
		echo "../../cmake-build-release"; \
	elif [ -d "../../cmake-build-debug" ]; then \
		echo "../../cmake-build-debug"; \
	else \
		echo "../../build"; \
	fi \
)

INCLUDE=-I$(INCLUDE_DIR) -I$(THIRD_PARTY_DIR)/simlib/include -I$(THIRD_PARTY_DIR)/re2/include -I $(THIRD_PARTY_DIR) -I $(THIRD_PARTY_DIR)/cudd/cudd
LIBS_ADD=-L$(BUILD_DIR)/src -L$(BUILD_DIR)/3rdparty/re2 -L$(BUILD_DIR)/3rdparty/simlib -L$(BUILD_DIR)/3rdparty/cudd
LIBS=-lmata -lsimlib -lre2 -lcudd


###############################################################################

.PHONY: all clean

all: $(patsubst %.cc,%,$(wildcard *.cc)) $(BUILD_DIR)/src/libmata.a

%: %.cc
	g++ $(CFLAGS) $(INCLUDE) $(LIBS_ADD) $< $(LIBS) -o ../bin/$@

clean:
	rm -rf $(patsubst %.cc,%,$(wildcard *.cc))
