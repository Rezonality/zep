#!/bin/sh

# success (everything but -n mike should be ignored)
../examples/test22 asdf -n mike asdf fds xxx > tmp.out 2>&1

if cmp -s tmp.out $srcdir/test80.out; then
	exit 0
else
	exit 1
fi

