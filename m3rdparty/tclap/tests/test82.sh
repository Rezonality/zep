#!/bin/sh

# success - all unmatched args get slurped up in the UnlabeledMultiArg
../examples/test23 blah --blah -s=bill -i=9 -i=8 -B homer marge bart > tmp.out 2>&1

if cmp -s tmp.out $srcdir/test82.out; then
	exit 0
else
	exit 1
fi

