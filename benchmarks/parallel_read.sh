#!/bin/bash
mount_dir=$1
to=$(($2 + $3 - 1))
for i in $(seq $2 $to)
do
    file="/$i"
    sudo ./read $mount_dir $file &
done
