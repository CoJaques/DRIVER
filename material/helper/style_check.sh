#!/bin/bash

if [[ $# != 1 ]]; then
    echo "Usage: ${BASH_SOURCE[0]} <file>"
    exit 1
fi

if [[ ! -f $1 ]]; then
    echo "$1 doesn't exist or is not a file"
    exit 1
fi

# Get original script directory path
SOURCE=${BASH_SOURCE[0]}
while [ -L "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
    DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
    SOURCE=$(readlink "$SOURCE")
    [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

meld $1 <(clang-format $1 -style=file:${DIR}/.clang-format)
