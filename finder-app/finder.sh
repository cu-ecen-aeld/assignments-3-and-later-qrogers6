#!/bin/sh

# Accept 2 runtime arguments
if [ $# -ne 2 ]; then
  echo "Expected number of args is 2, not [$#]."
  exit 1
fi

# Path to directory on the filesystem
filesdir=$1
# Text string to be searched within the specified filesystem
searchstr=$2

# Does path to directory exist
if [ ! -d $filesdir ]; then
  echo "[$filesdir] is not a directory."
  exit 1
fi

# Number of files in the directory and all subdirectories
numberOfFiles=$(ls $filesdir | wc -l )
# Number of matching lines found in files 
numberOfLines=$(grep -rnw $filesdir -e $searchstr | wc -l)

echo "The number of files are ${numberOfFiles} and \
the number of matching lines are ${numberOfLines}"

exit 0

