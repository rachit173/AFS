load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
package(default_visibility = ["//visibility:public"])

cc_library(
    name = "fuse",
    srcs = ["Fuse.cpp"],
    hdrs = ["Fuse.h", "Fuse-impl.h"],
    copts = [
        "-I/usr/local/include/fuse3/",
        "-std=c++17",
        # "-I/usr/local/include/fuse3/",
    ],
)

cc_binary(
  name = "client",
  srcs = ["client_main.cc"],
  deps = [
    "//protos:afs_cc_grpc",
    "@com_github_grpc_grpc//:grpc++",
  ],
  copts = [
    "-I/usr/local/include/fuse3/",
    "-std=c++17",
    # "-I/usr/local/include/fuse3/",
 ],
 defines = [
    "_FILE_OFFSET_BITS=64",
    "FUSE_USE_VERSION=31"
 ],
 linkstatic = True,
 linkopts = ["-ldl", "-lpthread", "-lfuse"],
)
