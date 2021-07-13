CURRENT="$PWD"
OUT_FOLDER="multivariants"
ORIGINAL_FOLDER=$1
shift

while [ -n "$1" ]; do

    case "$1" in
        -i)
            # Instrument functions for callgraph
            args="$args --instrument-function "
        ;;
      -bb)
          # Instrument by BB
          args="$args --instrument-bb "
        ;;
      -n1)
          # Instrument by BB
          args="$args -merge-function-switch-cases "
        ;;

      -o)
          # Instrument by BB
          OUT_FOLDER=$2
          mkdir -p $OUT_FOLDER
          shift
        ;;
    esac
    shift
done



find find "out_group" -name "*_\[*.bc" | wc -l

CURRENT=1
MULTIVARIANTS=""
for module in $(ls out_group)
do
  if [ -d "out_group/$module" ]
  then
    variants=$(find "out_group/$module" -name "*_\[*.bc" | grep -E "_\[" | awk '{printf "%s,", $1}')

    originalmodule=$(find "out_group/$module" -depth 1 | grep "original.bc")

    #echo ../build/crow-linker $originalmodule "out_group/$module/$module.multivariant.bc" --override $args --replace-all-calls-by-the-discriminator -crow-merge-id-start=$CURRENT -crow-merge-debug-level=5 -crow-merge-skip-on-error -crow-merge-bitcodes="$variants"
    ../build/crow-linker $originalmodule "out_group/$module/$module.multivariant.bc" --override --merge-function-switch-cases --replace-all-calls-by-the-discriminator -crow-merge-id-start=$CURRENT -crow-merge-debug-level=5 --variants-no-inline -crow-merge-skip-on-error -crow-merge-bitcodes="$variants"  2>"out_group/$module/$module.multivariant.map.txt" 1>"out_group/$module/$module.multivariant.function_names.txt" || (echo $module && exit 1)
    # Link all variants first to avoid aliasing
    echo $module
     ../third_party/llvm-Release-install/bin/llvm-dis "out_group/$module/$module.multivariant.bc" -o "out_group/$module/$module.multivariant.ll"
    CURRENT=$(bc <<< "$CURRENT + 1000")

    MULTIVARIANTS="$MULTIVARIANTS,out_group/$module/$module.multivariant.bc"

   # exit 1
  fi
done

echo $MULTIVARIANTS
# Merge all in one big fat

# Link all originals
../third_party/llvm-Release-install/bin/llvm-link $(find $ORIGINAL_FOLDER -name "*.bc") -o "allinone.bc"

rm "$OUT_FOLDER/instrumented/allinone.multivariant.i.bc"
rm "$OUT_FOLDER/multivariant/allinone.multivariant.bc"
rm "$OUT_FOLDER/original/allinone.bc"

mkdir -p "$OUT_FOLDER/original"
mkdir -p "$OUT_FOLDER/instrumented"
mkdir -p "$OUT_FOLDER/multivariant"

cp "allinone.bc" "$OUT_FOLDER/original/allinone.bc"

../third_party/llvm-Release-install/bin/llvm-dis "allinone.bc" -o "allinone.bc.ll"

../build/crow-linker "allinone.bc" "$OUT_FOLDER/instrumented/allinone.multivariant.i.bc" --instrument-function --replace-function --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="$MULTIVARIANTS" 1>i.out.txt  2> i.map.txt

../third_party/llvm-Release-install/bin/llvm-dis "$OUT_FOLDER/instrumented/allinone.multivariant.i.bc" -o "allinone.multivariant.i.ll"

../build/crow-linker "allinone.bc" "$OUT_FOLDER/multivariant/allinone.multivariant.bc" --replace-function --override -crow-merge-debug-level=1 -crow-merge-skip-on-error -crow-merge-bitcodes="$MULTIVARIANTS"

../third_party/llvm-Release-install/bin/llvm-dis "$OUT_FOLDER/multivariant/allinone.multivariant.bc" -o "allinone.multivariant.ll"

#find out_group -name "*.multivariant.bc" -exec cp -f {} $OUT_FOLDER/ \;
