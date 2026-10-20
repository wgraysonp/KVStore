load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "epoll_server",
    srcs = ["epoll_server.cc"],
    hdrs = ["epoll_server.h"],
    visibility = ["//src/endpoint/tests:__pkg__"],
    deps = [
        "//src/endpoint:client_connection",
        "//src/endpoint:consts",
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
    visibility = [
        "//src/endpoint/tests:__pkg__",
        "//src/storage:__pkg__",
    ],
)

cc_library(
    name = "client_connection",
    srcs = ["client_connection.cc"],
    hdrs = ["client_connection.h"],
    visibility = ["//src/endpoint/tests:__pkg__"],
    deps = [
        "//src/endpoint:consts",
        "//src/endpoint:socket_utils",
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
    visibility = ["//src/endpoint/tests:__pkg__"],
    deps = [
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "connection_manager",
    srcs = ["connection_manager.cc"],
    hdrs = ["connection_manager.h"],
    visibility = ["//src/endpoint/tests:__pkg__"],
    deps = [
        "//src/endpoint:client_connection",
        "//src/endpoint:consts",
        "//src/endpoint:socket_utils",
        "//src/rpc_framework",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:strings",
    ],
)

cc_library(
    name = "endpoint",
    visibility = ["//visibility:public"],
    deps = [
        "//src/endpoint:client_connection",
        "//src/endpoint:connection_manager",
        "//src/endpoint:consts",
        "//src/endpoint:epoll_server",
        "//src/endpoint:socket_utils",
    ],
)
