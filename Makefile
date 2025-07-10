main:
	mkdir -p build
	gcc src/dynaswap.c src/mem_usage.c src/psi.c src/swaphandler.c -luuid -lproc2 -o build/dynaswap