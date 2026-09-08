load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "epoll_server",
    srcs = ["epoll_server.cc"],
    hdrs = ["epoll_server.h"],
    deps = [
        "@abseil-cpp//absl/cleanup:cleanup",
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/strings:strings",
    ],
)

cc_library(
    name = "consts",
    hdrs = ["consts.h"],
)

cc_binary(
    name = "kv_service",
    srcs = ["main.cc"],
    deps = [
        "//src/kv_service:consts",
        "//src/kv_service:epoll_server",
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/strings:strings",
    ],
)
