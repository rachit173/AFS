load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
  name = "server",
  srcs = ["server_main.cc"],
  deps = [
    "//protos:afs_cc_grpc",
    "@com_github_grpc_grpc//:grpc++",
  ],
)