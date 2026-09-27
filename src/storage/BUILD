load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "write_ahead",
    srcs = ["write_ahead.cc"],
    hdrs = ["write_ahead.h"],
    visibility = ["//src/storage/tests:__pkg__"],
    deps = [
        "//src/kv_service:data",
        "@abseil-cpp//absl/cleanup:cleanup",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)
