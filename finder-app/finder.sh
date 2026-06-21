#!/bin/sh

# Used claude AI to provide a working example, read man pages for relevant commands, then re-wrote implementation. Primarily gained knowledge of 'wc -l' and 'test -ne'

if [ $# -ne 2 ]
then
    echo "Two args are required: <filesdir> <searchstr>"
    exit 1
fi

filesdir="$1"
searchstr="$2"

if [ ! -d $filesdir ]
then
    echo "$filesdir is not a directory"
    exit 1
fi

x=$(find -L $filesdir -type f | wc -l)
y=$(grep -r $searchstr $filesdir | wc -l)

echo "The number of files are $x and the number of matching lines are $y"

exit 0