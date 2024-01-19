#!/bin/sh

# Accept 2 runtime arguments
if [ $# -ne 2 ]; then
  echo "Expected number of args is 2, not [$#]."
  exit 1
fi

# Full path to to a file (including filename)
writefile=$1
# Test string which will be written to file 
writestr=$2

# Specified file name
fileName=$(basename $writefile)
# Specified directory name
dirName=$(dirname $writefile)

# Does current directory exist? Create if it doesn't
if [ ! -d $dirName ]; then
  mkdir -p $dirName

  if [ $? -ne 0 ]; then
    echo "Failed to create directory [$dirName]."
    exit 1
  fi
fi

# Write string to file
echo $writestr > $writefile

if [ ! -f $writefile ]; then
  echo "Failed to write file [$writefile]."
  exit 1
fi

exit 0

