#!/bin/bash

set -eu

TESTS_DIR=`dirname "$0"`/tests

for infile in "$TESTS_DIR"/*.in
do
    outfile=`echo $infile | sed 's/\.in/\.out/'`
    if [ ! -e "$outfile" ]
    then
        echo "$0: error: no out file $outfile"
        exit 1
    fi

    # Treat "random.in" and "random-opt.in" specially.
    if [ `basename $infile` = "random.in" ]
    then
        infile="--random 1000000"
    elif [ `basename $infile` = "random-opt.in" ]
    then
        infile="--random 1000000 --optimize"
    fi

    if ./lirasm --execute $infile | tr -d '\r' > testoutput.txt && cmp -s testoutput.txt $outfile
    then
        echo "$0: output correct for $infile"
    else
        echo "$0: incorrect output for $infile"
        echo "$0: === actual output ==="
        cat testoutput.txt
        echo "$0: === expected output ==="
	cat $outfile
    fi
done

rm testoutput.txt
