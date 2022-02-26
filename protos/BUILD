package(default_visibility = ["//visibility:public"])

load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_proto_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")


proto_library(
    name = "afs_proto",
    srcs = ["afs.proto"],
)

cc_proto_library(
    name = "afs_cc_proto",
    deps = [":afs_proto"],
)

cc_grpc_library(
    name = "afs_cc_grpc",
    srcs = [":afs_proto"],
    grpc_only = True,
    deps = [":afs_cc_proto"],
)