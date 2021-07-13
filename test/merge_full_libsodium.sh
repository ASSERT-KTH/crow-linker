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
          args="$args --merge-function-switch-cases "
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
    VARIANTS="$VARIANTS,$(find "out_group/$module" -name "*_\[*.bc" | grep -E "_\[" | awk '{printf "%s,", $1}')"
    echo $module
  fi
done

# Link all originals
llvm-link $(find $ORIGINAL_FOLDER -name "*.bc") -o "allinone.bc"

mkdir -p "$OUT_FOLDER/original"
mkdir -p "$OUT_FOLDER/instrumented"
mkdir -p "$OUT_FOLDER/multivariant"

rm "$OUT_FOLDER/instrumented/allinone.multivariant.i.bc"
rm "$OUT_FOLDER/multivariant/allinone.multivariant.bc"
rm "$OUT_FOLDER/original/allinone.bc"

cp "allinone.bc" "$OUT_FOLDER/original/allinone.bc"

../third_party/llvm-Release-install/bin/llvm-dis "allinone.bc" -o "allinone.bc.ll"

echo "Building instrumented multivariant $args"
../build/crow-linker "allinone.bc" "$OUT_FOLDER/instrumented/allinone.multivariant.i.bc" -complete-replace=false -merge-function-switch-cases --replace-all-calls-by-the-discriminator --instrument-function --override -crow-merge-debug-level=1 -crow-merge-skip-on-error  -crow-merge-bitcodes="$VARIANTS" 1>i.out.txt  2> i.map.txt || exit 1

../third_party/llvm-Release-install/bin/llvm-dis "$OUT_FOLDER/instrumented/allinone.multivariant.i.bc" -o "allinone.multivariant.i.ll"

echo "Building  multivariant $args"
../build/crow-linker "allinone.bc" "$OUT_FOLDER/multivariant/allinone.multivariant.bc" -complete-replace=false -merge-function-switch-cases --replace-all-calls-by-the-discriminator --instrument-function --override -crow-merge-debug-level=1 -crow-merge-skip-on-error  -crow-merge-bitcodes="$VARIANTS"

#find out_group -name "*.multivariant.bc" -exec cp -f {} $OUT_FOLDER/ \;
