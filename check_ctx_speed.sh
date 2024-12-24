#!bin/bash 
set -e
dir=build-bench
new=$1
args="--tasks 1000 --rounds 2000 --strategy random --threads 250 --verbose false --switches 100"
git switch $new
cmake -G Ninja -B $dir -DCMAKE_BUILD_TYPE=Release 
cmake --build $dir --target verifying/targets/atomic_register
echo "new impl from $new branch"
time ./$dir/verifying/targets/atomic_register $args
git switch master
cmake -G Ninja -B $dir -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
cmake --build $dir --target verifying/targets/atomic_register >/dev/null 2>&1
echo "base impl"
time ./$dir/verifying/targets/atomic_register $args
rm -rf $dir
git switch $new
