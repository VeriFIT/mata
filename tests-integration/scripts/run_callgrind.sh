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
cmd=$*

if [ -z "$outfile" ]
then
    branch=$(git rev-parse --abbrev-ref HEAD)
    head=$(git rev-parse --short HEAD)
    escaped_cmd=$(printf '%q' "${cmd//[\/ ]/-}")
    callgrind_file="callgrind.out.$branch-$head-$escaped_cmd"
    if [ -e "$callgrind_file".0 ]; then
        existing_files=("$callgrind_file".*)
        number_of_existing_files=${#existing_files[@]}
        outfile="$callgrind_file"."$number_of_existing_files"
    else
        outfile="$callgrind_file".0
    fi
fi

valgrind --tool=callgrind --callgrind-out-file="$outfile" "$cmd"
echo ""
echo "== Top functions ( > 1%) =="
callgrind_annotate --inclusive=no --tree=none --show-percs=yes "$outfile" | grep "\(\w\w\|[1-9]\).\w\w%" | tail +1 | sed 's/\[[^][]*\]$//g'
echo ""
echo "== Output saved to '$outfile' =="
