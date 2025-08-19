#!/bin/sh

if [ $# -lt 2 ]
then
    echo "The script accepts the following arguments:"
    echo "    - the first argument is a full path to a file (including filename) on the filesystem"
    echo "    - the second argument is a text string which will be written within this file"
    exit 1
fi

WRITEFILE=$1
WRITESTR=$2

mkdir -p "$(dirname "$WRITEFILE")"

printf "$WRITESTR" > $WRITEFILE