CURRENT="$PWD"
OUT_FOLDER="multivariants"
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

CURRENT=1
for module in $(ls out_group)
do
  if [ -d "out_group/$module" ]
  then
    echo $CURRENT
    variants=$(find "out_group/$module" -name "*_\[*.bc" | grep -E "_\[" | awk '{printf "%s,", $1}')
    fns=$(find "out_group/$module" -type d -depth 1 -exec bash -c 'echo $(basename {})' \; | awk '{printf "%s,", $1}')
    echo "$module" "$fns"

    originalmodule=$(find "out_group/$module" -depth 1 | grep "original.bc")


    ../build/crow-linker $originalmodule "out_group/$module/$module.multivariant.bc" --override $args -crow-merge-id-start=$CURRENT -crow-merge-debug-level=1 -crow-merge-skip-on-error -crow-merge-bitcodes="$variants" -crow-merge-functions="$fns" 2>"out_group/$module/$module.multivariant.map.txt" 1>"out_group/$module/$module.multivariant.function_names.txt" || exit 1

    llvm-dis "out_group/$module/$module.multivariant.bc" -o "out_group/$module/$module.multivariant.ll"
    CURRENT=$(bc <<< "$CURRENT + 1000")
  fi
done

find out_group -name "*.multivariant.bc" -exec cp -f {} $OUT_FOLDER/ \;
