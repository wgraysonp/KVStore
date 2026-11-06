load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "client_connection",
    srcs = ["client_connection.cc"],
    hdrs = ["client_connection.h"],
    visibility = ["//src/endpoint/tests:__pkg__"],
    deps = [
        "//src/common:consts",
        "//src/common/utils:socket_utils",
        "//src/rpc_framework",
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings:strings",
    ],
)

cc_library(
    name = "connection_manager",
    srcs = ["connection_manager.cc"],
    hdrs = ["connection_manager.h"],
    visibility = ["//src/endpoint/tests:__pkg__"],
    deps = [
        "//src/common:consts",
        "//src/common/utils:socket_utils",
        "//src/endpoint:client_connection",
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
    ],
)
