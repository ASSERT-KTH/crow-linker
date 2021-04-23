CURRENT="$PWD"

while [ -n "$1" ]; do

    case "$1" in
        -i)
            # Instrument functions for callgraph
            args=" --instrument-function "
        ;;
      -bb)
          # Instrument by BB
          args=" --instrument-bb "
        ;;
    esac
    shift
done


for module in $(ls out_group)
do
  if [ -d "out_group/$module" ]
  then

    variants=$(find "out_group/$module" -name "*_\[*.bc" | grep -E "_\[" | awk '{printf "%s,", $1}')
    fns=$(find "out_group/$module" -type d -depth 1 -exec bash -c 'echo $(basename {})' \; | awk '{printf "%s,", $1}')
    echo "" "$fns"


      #echo "$ALL"
      #functions=$(find "out_group/$module" -depth 1 | grep "merge.bc" | awk '{printf "%s,", $1}')


      #

      #if [[ $originalmodule ]]
      #then
        #echo $functions
        #echo $originalmodule
        #../build/crow-linker $originalmodule "out_group/$module/$module.multivariant.bc" --override $args --skip-function-names -crow-merge-debug-level=1 -crow-merge-skip-on-error -crow-merge-bitcodes="$functions" -crow-merge-functions="ASD" 2>"out_group/$module/$module.instrumentation.map.txt" || exit 1

        #llvm-dis "out_group/$module/$module.multivariant.bc" -o "out_group/$module/$module.multivariant.ll"

      #fi


    originalmodule=$(find "out_group/$module" -depth 1 | grep "original.bc")


    ../build/crow-linker $originalmodule "out_group/$module/$module.multivariant.bc" --override $args -crow-merge-debug-level=1 -crow-merge-skip-on-error -crow-merge-bitcodes="$variants" -crow-merge-functions="$fns" 2>"out_group/$module/$module.multivariant.map.txt" 1>"out_group/$module/$module.multivariant.function_names.txt" || exit 1

      llvm-dis "out_group/$module/$module.multivariant.bc" -o "out_group/$module/$module.multivariant.ll"
  fi
done