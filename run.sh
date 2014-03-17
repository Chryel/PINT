#!/bin/bash

echo `date` > results.txt

for i in `seq 1 100`; do
    echo $i >> results.txt
    ./build/test >> results.txt 2>&1
done