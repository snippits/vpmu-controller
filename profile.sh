#!/bin/sh
# Copyright (c) 2017, Medicine Yeh

SCRIPT_SRC=$(dirname $0)
options=""

function print_help()
{
    echo "$0 [OPTIONS, ...] command [command args...]"
    echo "    OPTIONS: --monitor, --phase, --trace, --jit"
}

function encapsulate()
{
    whitespace="[[:space:]]"
    for i in "$@"
    do
        if [[ $i =~ $whitespace ]]
        then
            i=\"$i\"
        fi
        # echo "$i"
        args+="$i "
    done

    echo ${args}
}

options=""
while [[ 1 ]]; do
    case "$1" in
        "--monitor" )
            options+="--monitor "
            ;;
        "--phase" )
            options+="--phase "
            ;;
        "--trace" )
            options+="--trace "
            ;;
        "--jit" )
            options+="--jit "
            ;;
        "--help" )
            print_help
            exit 0
            ;;
        "-h" )
            print_help
            exit 0
            ;;
        *)
            break
            ;;
    esac
    shift 1 # Remove the fisrt argument
done

args=$(encapsulate "$@")
${SCRIPT_SRC}/vpmu-control-arm ${options} --all_models --start --exec "${args}" --end

