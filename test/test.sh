


../build/crow-linker wrandombytes.bc t.bc -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,b2" -crow-merge-functions="randombytes_buf"



llvm-dis t.bc -o o.ll