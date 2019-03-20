.PHONY: all check

all: test-linux test-win

check: test-linux test-win
	./test-linux
	./test-win

test-linux: test-linux.c cfgpath.h
	$(CC) -O0 -g -o $@ $<

test-win: test-win.c cfgpath.h shlobj.h
	$(CC) -O0 -g -o $@ $< -I.
