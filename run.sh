sudo fusermount -u mountdir
rm -rf mountdir
mkdir mountdir
bazel build -c fastbuild //client:client
./bazel-bin/client/client mountdir -d -o allow_other 