#!/bin/sh

# failure, still looking for -n 
../examples/test22 asdf  asdf -r fds xxx > tmp.out 2>&1

if cmp -s tmp.out $srcdir/test81.out; then
	exit 0
else
	exit 1
fi

