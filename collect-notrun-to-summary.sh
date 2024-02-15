#!/bin/sh

if [ -z "$1" ]; then
	echo "ERROR: $0 path-to-fstests [config_name]"
	exit 1
fi

here="$1"
tdir="$here/tests"
rdir="$here/results"
config=

if [ -n "$2" ]; then
	config="$2"
fi

cdir="$rdir/$config"

if ! [ -d "$cdir" ]; then
	echo "ERROR: config dir $cdir doe not exist"
	exit 1
fi

echo "<details><summary>Tests not run</summary>"
echo
echo '```'

find "$cdir" -type f -name '*.notrun' -exec cat '{}' \; |
        sort |
        uniq -c |
        sort -rn

echo '```'
echo
echo "</details>"
