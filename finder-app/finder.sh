#!/bin/sh

if [ $# -lt 2 ]
then
    echo "The script accepts the following arguments:"
    echo "    - the first argument is a path to a directory on the filesystem"
    echo "    - the second argument is a text string which will be searched within these files"
    exit 1
else
    FILESDIR=$1
    SEARCHSTR=$2
fi

if [ ! -d $FILESDIR ]
then
    echo "$FILESDIR is not a valid directory"
    exit 1
fi

FILES=$(find $FILESDIR -type f -name "*")
NUM_STRINGS=0
NUM_FILES=0
for file in $FILES
do
    MATCHES=$(grep -c $SEARCHSTR $file)
    NUM_STRINGS=$(($NUM_STRINGS + $MATCHES))
    NUM_FILES=$(($NUM_FILES + 1))
done

echo "The number of files are $NUM_FILES and the number of matching lines are $NUM_STRINGS"