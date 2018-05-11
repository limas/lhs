#!/bin/bash

do_run=no
compile_only=no

# check argument
while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
        -r)
            do_run=yes
            shift
        ;;
        -c)
            compile_only=yes
            shift
        ;;
        *)
            echo "unknown option: $1"
            exit -1
        ;;
    esac
done

# compile server framework
if [[ $compile_only == yes ]]; then
    make clean all
    exit 0;
else
    make clean all > /dev/null
fi

# install database if not yet installed

# do startup server if specified
if [[ $do_run == yes ]]; then
    ./server
fi
