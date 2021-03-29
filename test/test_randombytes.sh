
source common.sh


get_variants "argon2" "bcs"

echo $FUNCTIONS $UNIQUE_VARIANTS

check4functions $ORIGINAL ${VARIANTS[@]}


if [[ $UNIQUE_VARIANTS ]]
then

  echo $FUNCTIONS $UNIQUE_VARIANTS
  ../build/crow-linker $ORIGINAL t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="$UNIQUE_VARIANTS" -crow-merge-functions="$FUNCTIONS"

  llvm-dis t.bc

  rm t.bc
  else
    echo "No different variant found"
fi

#../build/crow-linker wrandombytes.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,wrandombytes.bc" -crow-merge-functions="randombytes"



