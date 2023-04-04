#!/bin/sh

valgrind --tool=massif --pages-as-heap=yes --massif-out-file=massif.out $@
grep mem_heap_B massif.out | sed -e 's/mem_heap_B=\(.*\)/\1/' | sort -g | tail -n 1 | sed 's/^/memory_peak: /'
