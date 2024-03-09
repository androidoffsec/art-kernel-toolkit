#!/bin/bash

set -o errexit
set -o pipefail

usage() {
    echo "Usage: $0 [-c|--check]"
    echo "  -c,  --check         check format instead of formatting the code"
    exit 1
}

if [[ $# -gt 1 ]]; then
    usage
fi

CHECK=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -c | --check)
            CHECK=1
            shift
            ;;
        *)
            usage
            ;;
    esac
done

CLANG_ARGS="-i"
MDFORMAT_ARGS="--wrap 80"

set -o xtrace

if [[ $CHECK == 1 ]]; then
    CLANG_ARGS="--dry-run -Werror"
    MDFORMAT_ARGS="--check"
fi

find src -name "*.c" -o -name "*.h" | xargs clang-format $CLANG_ARGS
mdformat $MDFORMAT_ARGS *.md
