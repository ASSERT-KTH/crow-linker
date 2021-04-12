
source common.sh


get_variants "$1" "$2"

check4functions "$1" "${VARIANTS[@]}"


if [[ $UNIQUE_VARIANTS ]]
then

  echo $FUNCTIONS
  echo "======"

  ../build/crow-linker $ORIGINAL codecs.all.bc --override -crow-merge-debug-level=1 -crow-merge-skip-on-error -crow-merge-bitcodes="$UNIQUE_VARIANTS" -crow-merge-functions="sodium_bin2base64"

  exit 0
  for c in "${CREATED_ONES[@]}"
  do
    echo $c
    llvm-extract --func=$c out.bc -o tmp.bc
    llvm-dis tmp.bc -o $c.dump.ll
  done


  else
    echo "No different variant found"
fi

#../build/crow-linker wrandombytes.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,wrandombytes.bc" -crow-merge-functions="randombytes"



