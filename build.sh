#!/bin/bash
set +xe

find "src" -type f -iname "*.cpp" | while read file; do
    clang++ -std=gnu++20 -c $file -Iinclude -o $(basename $file .cpp).o
done

mkdir -p build

clang++ -std=gnu++20 *.o -o build/lynx

rm *.o
