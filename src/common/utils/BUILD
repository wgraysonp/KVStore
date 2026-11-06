load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "socket_utils",
    srcs = ["socket_utils.cc"],
    hdrs = ["socket_utils.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "utils",
    deps = ["//src/common/utils:socket_utils"],
    visibility = ["//visibility:public"],
)
