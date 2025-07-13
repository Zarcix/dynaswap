main:
	mkdir -p build
	gcc src/*.c -luuid -lproc2 -o build/dynaswap