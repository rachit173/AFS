#!/bin/bash
mount_dir=$1
to=$(($2 + $3 - 1))
for i in $(seq $2 $to)
do
    file="$mount_dir/scaling/$i"
    sudo ./read_local $file &
done
