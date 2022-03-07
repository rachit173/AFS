# $1: crash type
#   1 - crash_open_before_fetch
#   2 - crash_open_after_fetch
#   3 - crash_read_before_read
#   4 - crash_read_after_read
#   5 - crash_write_before_write
#   6 - crash_write_after_write
#   7 - crash_flush_before_store
#   8 - crash_flush_after_store

sudo fusermount -u mountdir
rm -rf mountdir
mkdir mountdir
bazel build -c fastbuild //client:client

if [ -z "$1" ]; then
  ./bazel-bin/client/client localhost:50051 mountdir -d -o allow_other
else
  ./bazel-bin/client/client localhost:50051 mountdir -d -o allow_other "$1"
fi
