#!/bin/sh
valgrind --tool=callgrind --callgrind-out-file=callgrind.out $@
echo ""
echo "== Top functions ( > 1%) =="
callgrind_annotate --inclusive=no --tree=none --show-percs=yes callgrind.out | grep "\(\w\w\|[1-9]\).\w\w%" | tail +1
