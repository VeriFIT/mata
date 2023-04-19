#!/bin/bash

print_usage() {
  echo "usage: ./run_massif.sh [OPTIONS] CMD"
  echo ""
  echo "OPTIONS:"
  echo "  -o, --output-file    Saves massif output to file"
}

outfile=

while [[ "$#" -gt 0 ]];
do
    key="$1"

    case $key in
        --help|-h)
            print_usage
            exit 0
            ;;
        --output-file|-o)
            outfile="$2"
            shift 2
            ;;
        *)
            break
            ;;
    esac
done
cmd=$@

if [ -z "$outfile" ]
then
    branch=$(git rev-parse --abbrev-ref HEAD)
    head=$(git rev-parse --short HEAD)
    escaped_cmd=$(printf '%q' "${cmd//[\/ ]/-}")
    massif_file="massif.out.$branch-$head-$escaped_cmd"
    existing_no=$(ls -1 $massif_file.* | wc -l)
    outfile="$massif_file.$existing_no"
fi

valgrind --tool=massif --pages-as-heap=yes --massif-out-file="$outfile" $cmd
grep mem_heap_B "$outfile" | sed -e 's/mem_heap_B=\(.*\)/\1/' | sort -g | tail -n 1 | sed 's/^/memory_peak: /'
echo ""
echo "== Output saved to '$outfile' =="
