#!/bin/bash

echo `date` > results.txt

for i in `seq 1 100`; do
    echo $i >> results.txt
    ./build/select >> results.txt 2>&1
done