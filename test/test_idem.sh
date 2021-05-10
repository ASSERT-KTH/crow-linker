rm t.bc

clang -c -emit-llvm recursive.c -o recursive.bc

../build/crow-linker recursive.bc t.bc --instrument-function  --override -crow-merge-debug-level=3 -crow-merge-skip-on-error -crow-merge-bitcodes="recursive2.bc" -crow-merge-functions="a"


#../build/crow-linker wrandombytes.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,wrandombytes.bc" -crow-merge-functions="randombytes"
/usr/local/opt/llvm/bin/llvm-dis t.bc -o -
if [[ $(/usr/local/opt/llvm/bin/llvm-dis t.bc -o - | grep -q "_cb71P5H47J3A") ]]
then
  echo "Success"
fi
#rm t.bc
