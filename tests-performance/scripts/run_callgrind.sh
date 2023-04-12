#!/bin/bash

print_usage() {
  echo "usage: ./run_callgrind.sh [OPTIONS] CMD"
  echo ""
  echo "OPTIONS:"
  echo "  -o, --output-file    Saves callgrind output to file"
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
    outfile="callgrind.out.$branch-$head-$escaped_cmd"
fi

valgrind --tool=callgrind --callgrind-out-file="$outfile" $cmd
echo ""
echo "== Top functions ( > 1%) =="
callgrind_annotate --inclusive=no --tree=none --show-percs=yes callgrind.out | grep "\(\w\w\|[1-9]\).\w\w%" | tail +1 | sed 's/\[[^][]*\]$//g'
echo ""
echo "== Output saved to '$outfile' =="
