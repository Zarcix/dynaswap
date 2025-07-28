main:
	mkdir -p build
	gcc src/*.c -luuid -lproc2 -lconfig -o build/dynaswap