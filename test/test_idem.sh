
../build/crow-linker recursive.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="recursive2.bc" -crow-merge-functions="a"


#../build/crow-linker wrandombytes.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,wrandombytes.bc" -crow-merge-functions="randombytes"


COUNT=$(llvm-dis t.bc -o - | grep -c "@a")

rm t.bc
