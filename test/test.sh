$OPT -load ../build/libCrowMergePass.dylib -crowmerge wrandombytes.bc -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,b2" -crow-merge-functions="randombytes_buf" -o t.bc

llvm-dis t.bc -o -