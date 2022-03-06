sudo fusermount -u mountdir
rm -rf mountdir
mkdir mountdir
bazel build -c fastbuild //client:client
./bazel-bin/client/client localhost:50051 mountdir -d -o allow_other 
