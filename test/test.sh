
../build/crow-linker recursive.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="recursive.bc,recursive.bc" -crow-merge-functions="a"


#../build/crow-linker wrandombytes.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,wrandombytes.bc" -crow-merge-functions="randombytes_buf_deterministic"


rm o.ll
llvm-dis t.bc -o o.ll