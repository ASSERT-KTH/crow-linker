CURRENT="$PWD"


module="ed25519_ref10"
variants=$(find "out_group/$module" -name "*_\[*.bc" | grep -E "_\[" | awk '{printf "%s,", $1}')
fns=$(find "out_group/$module" -type d -depth 1 -exec bash -c 'echo $(basename {})' \; | awk '{printf "%s,", $1}')
echo "" "$fns"



originalmodule=$(find "out_group/$module" -depth 1 | grep "original.bc")

echo $variants

../build/crow-linker $originalmodule "$module.multivariant.bc" --override $args --replace-all-calls-by-the-discriminator -crow-merge-debug-level=6 -merge-function-ptrs -crow-merge-skip-on-error -crow-merge-bitcodes="$variants" -crow-merge-functions="ge25519_p2_0"
echo "================================="
#/Users/javierca/Documents/Develop/wasi-sdk-10.0/bin/wasm-ld "$module.multivariant.bc" --no-entry --export-all --allow-undefined -o multivariant.wasm

../third_party/llvm-Release-install/bin/llvm-dis "$module.multivariant.bc" -o discrmination.ll
#$WASM2WAT multivariant.wasm -o multivariant.wat
