
../build/crow-linker recursive.bc t.bc --override --merge-only-if-different=false -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="recursive.bc,recursive.bc" -crow-merge-functions="a"


#../build/crow-linker wrandombytes.bc t.bc --override -crow-merge-debug-level=2 -crow-merge-skip-on-error -crow-merge-bitcodes="wrandombytes.bc,wrandombytes.bc" -crow-merge-functions="randombytes"


COUNT=$(/usr/local/opt/llvm/bin/llvm-dis t.bc -o - | grep -c "@a")

/usr/local/opt/llvm/bin/llvm-dis t.bc -o -

rm t.bc

if [[ $COUNT != 6 ]]
then
  echo "ERROR, we replaced 3 recursive function like"
  cat "recursive.c"
  echo "New module should have 3 different functions"
  exit 1
fi