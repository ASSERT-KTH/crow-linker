
source common.sh


get_variants "argon2" "bcs"


check4functions $ORIGINAL ${VARIANTS[@]}


../build/crow-linker $ORIGINAL t.bc --override -crow-merge-debug-level=0 -crow-merge-skip-on-error -crow-merge-bitcodes="$UNIQUE_VARIANTS" -crow-merge-functions="$FUNCTIONS"


#../build/crow-linker wrandombytes.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,wrandombytes.bc" -crow-merge-functions="randombytes"


llvm-dis t.bc

rm t.bc
