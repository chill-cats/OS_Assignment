#! /usr/bin/env bash

MAX_ITERATION=10000
PROGRAM_NAME="./os"
ARGS="./input/os_1"
EXPECTED_PAGE_COUNT=19

for (( i=0; i<MAX_ITERATION; i++ )) do
    echo "Iteration $i"
    output=$($PROGRAM_NAME $ARGS)
    echo "Program output: $output"
    if [ $? -ne 0 ]; then
        echo "Program failed"
        echo "Iteration $i"
        exit 1
    fi

    # if ! grep -q "0a" <<< "$output"; then
    #     echo "Program failed"
    #     exit 1
    # fi
    PROGRAM_FRAME=$(ggrep --p "^\d\d\d: " <<< "$output" | wc -l)

    if [[ $PROGRAM_FRAME -ne $EXPECTED_PAGE_COUNT ]]; then
        echo "Page not enough"
        echo "Expected $EXPECTED_PAGE_COUNT, but got $PROGRAM_FRAME"
        echo "Iteration $i"
        exit 1
    fi


done
