#!/bin/sh
cd "/home/diver/sources/C/NSQCuC/"
j=$(date)
git add .
git commit -m "$1 $j"
git push git@github.com:Vladgobelen/NSQCuC.git
git add .
git commit -m "$1 $j"
git push

