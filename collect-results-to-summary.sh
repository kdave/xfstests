#!/bin/sh -x

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

for bad in $(find "results/$config" -type f -name '*.bad'); do
	echo "::notice::$bad"
        pbad=${bad#results/$config/}
        pbase=${pbad%.bad}
        pname=${pbase%.out}
        echo "<details><summary>Test: $pname DIFF</summary>"
        echo

        dmesg="$cdir/${pname}.dmesg"
        if [ -f "$dmesg" ]; then
                echo "<details><summary>Test: $pname DMESG</summary>"
                echo
                echo '```'
                cat "$dmesg"
                echo '```'
                echo
                echo "</details>"
                echo
        fi

        full="$cdir/${pname}.full"
        if [ -f "$full" ]; then
                echo "<details><summary>Test: $pname FULL</summary>"
                echo
                echo '```'
                cat "$full"
                echo '```'
                echo
                echo "</details>"
                echo
        fi

        # Diff
        echo '```'
        tst="$tdir/$pbase"
        diff -u "$tst" "$bad"
        echo '```'
        echo

        echo "</details>"
        exit
done
