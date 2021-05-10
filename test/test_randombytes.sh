
source common.sh


get_variants "$1" "$2"

check4functions "$1" "${VARIANTS[@]}"


if [[ $UNIQUE_VARIANTS ]]
then

  echo $FUNCTIONS
  echo "======"

  CREATED_ONES=($(../build/crow-linker $ORIGINAL out.bc --override -crow-merge-debug-level=1 -crow-merge-skip-on-error -crow-merge-bitcodes="$UNIQUE_VARIANTS" -crow-merge-functions="$FUNCTIONS" 2>err.log))

  exit 0
  for c in "${CREATED_ONES[@]}"
  do
    echo $c
    /usr/local/opt/llvm/bin/llvm-extract --func=$c out.bc -o tmp.bc
    /usr/local/opt/llvm/bin/llvm-dis tmp.bc -o $c.dump.ll
  done


  else
    echo "No different variant found"
fi

#../build/crow-linker wrandombytes.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,wrandombytes.bc" -crow-merge-functions="randombytes"



