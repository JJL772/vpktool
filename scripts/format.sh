#!/bin/bash
TOP=$(cd `dirname $0`;cd ..;pwd)
SRCDIR="$TOP/src/"
files=`find "$TOP" -iname "*.c" -o -iname "*.cpp" -o -iname "*.cc" -o -iname "*.h"`
for file in $files; do
	clang-format -i $file
done
