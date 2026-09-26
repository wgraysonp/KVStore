load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "write_ahead",
    srcs = ["write_ahead.cc"],
    hdrs = ["write_ahead.h"],
    deps = ["//src/kv_service:data"],
)
