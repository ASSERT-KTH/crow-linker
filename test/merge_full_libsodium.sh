CURRENT="$PWD"
OUT_FOLDER="multivariants"
ORIGINAL_FOLDER=$1
shift

while [ -n "$1" ]; do

    case "$1" in
        -i)
            # Instrument functions for callgraph
            args=" --instrument-function "
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
VARIANTS=""
for module in $(ls out_group)
do
  if [ -d "out_group/$module" ]
  then
    VARIANTS="$VARIANTS,$(find "out_group/$module" -name "*_\[*.bc" | grep -E "_\[" | awk '{printf "%s,", $1}')"
    echo $module
  fi
done

# Link all originals
../third_party/llvm-Release-install/bin/llvm-link $(find $ORIGINAL_FOLDER -name "*.bc") -o "allinone.bc"

mkdir -p "$OUT_FOLDER/original"
mkdir -p "$OUT_FOLDER/instrumented"
mkdir -p "$OUT_FOLDER/multivariant"

cp "allinone.bc" "$OUT_FOLDER/original/allinone.bc"
../build/crow-linker "allinone.bc" "$OUT_FOLDER/instrumented/allinone.multivariant.i.bc" --override $args -crow-merge-debug-level=4 --replace-all-calls-by-the-discriminator -crow-merge-skip-on-error -crow-merge-bitcodes="$VARIANTS"
../build/crow-linker "allinone.bc" "$OUT_FOLDER/multivariant/allinone.multivariant.bc" --override -crow-merge-debug-level=4 --replace-all-calls-by-the-discriminator -crow-merge-skip-on-error -crow-merge-bitcodes="$VARIANTS"

#../third_party/llvm-Release-install/bin/llvm-dis "out_group/allinone.multivariant.i.bc" -o "out_group/allinone.multivariant.i.ll"

#find out_group -name "*.multivariant.bc" -exec cp -f {} $OUT_FOLDER/ \;
