
../build/crow-linker recursive.bc t.bc --override  --merge-only-if-different=false --replace-all-calls-by-the-discriminator -merge-function-ptrs  -crow-merge-debug-level=4 -crow-merge-skip-on-error -crow-merge-bitcodes="recursive.bc,recursive.bc" -crow-merge-functions="a"


#../build/crow-linker wrandombytes.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,wrandombytes.bc" -crow-merge-functions="randombytes"


COUNT=$(/usr/local/opt/llvm/bin/llvm-dis t.bc -o - | grep -c "@a")

/usr/local/opt/llvm/bin/llvm-dis t.bc -o -

rm t.bc
