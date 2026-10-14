load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "epoll_server",
    srcs = ["epoll_server.cc"],
    hdrs = ["epoll_server.h"],
    visibility = ["//src/kv_service/tests:__pkg__"],
    deps = [
        "//src/kv_service:client_connection",
        "//src/kv_service:consts",
        "//src/rpc_framework",
        "@abseil-cpp//absl/cleanup:cleanup",
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:strings",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "consts",
    hdrs = ["consts.h"],
    visibility = ["//src/storage:__pkg__"],
)

cc_library(
    name = "data",
    hdrs = ["data.h"],
    visibility = [
        "//src/storage:__pkg__",
        "//src/storage/tests:__pkg__",
    ],
)

cc_library(
    name = "client_connection",
    srcs = ["client_connection.cc"],
    hdrs = ["client_connection.h"],
    deps = [
        "//src/kv_service:consts",
        "//src/kv_service:socket_utils",
        "//src/rpc_framework",
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:strings",
    ],
)

cc_library(
    name = "socket_utils",
    srcs = ["socket_utils.cc"],
    hdrs = ["socket_utils.h"],
    deps = [
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
    ],
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
