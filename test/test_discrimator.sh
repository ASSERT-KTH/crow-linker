CURRENT="$PWD"


for module in $(ls out_group)
do
  if [ -d "out_group/$module" ]
  then

    variants=$(find "out_group/$module" -name "*_\[*.bc" | grep -E "_\[" | awk '{printf "%s,", $1}')
    fns=$(find "out_group/$module" -type d -depth 1 -exec bash -c 'echo $(basename {})' \; | awk '{printf "%s,", $1}')
    echo "" "$fns"



    originalmodule=$(find "out_group/$module" -depth 1 | grep "original.bc")

    echo $variants

    ../build/crow-linker $originalmodule "out_group/$module/$module.multivariant.bc" --override $args -crow-merge-debug-level=5 -merge-function-ptrs -crow-merge-skip-on-error -crow-merge-bitcodes="$variants" -crow-merge-functions="$fns"

      llvm-dis "out_group/$module/$module.multivariant.bc" -o "out_group/$module/$module.multivariant.ll"

      exit 0
  fi
done