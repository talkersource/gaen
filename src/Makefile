# A simple Makefile used to directly compile GAEN sources
# Written by Sabin-Corneliu Buraga <busaco@infoiasi.ro> (c)2001-2002
# Version 0.5 - Last update: 18 May 2002

SRC=gaen

all: compile strip move				# to do all work

nosupport: nscompile strip move			# to compile with restrictions

nscompile:
	gcc $(SRC).c -o $(SRC)d -lcrypt -O2 -D GAEN__NO_SUPPORT__

compile:					# to compile sources
	gcc $(SRC).c -o $(SRC)d -lcrypt -O2 

strip:						# to strip symbols from executable
	strip $(SRC)d --strip-all

move:						# to move executable to GAEN 'root' directory
	mv $(SRC)d ..

arh:						# to make a gzipped tarball of GAEN sources
	tar -czf $(SRC).tgz $(SRC).?

unarh:
	tar -xzf $(SRC).tgz

clean:						# to remove backup files
	rm *~ ../$(SRC)d
