load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "data",
    hdrs = ["data.h"],
    visibility = ["//src/rpc_framework/tests:__pkg__"],
)

cc_library(
    name = "rpc_interface",
    hdrs = ["rpc_interface.h"],
    visibility = ["//src/rpc_framework/tests:__pkg__"],
    deps = [
        "//src/rpc_framework:data",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/status:statusor",
    ],
)

cc_library(
    name = "rpc_registry",
    srcs = ["rpc_registry.cc"],
    hdrs = ["rpc_registry.h"],
    visibility = ["//src/rpc_framework/tests:__pkg__"],
    deps = [
        "//src/rpc_framework:data",
        "//src/rpc_framework:rpc_interface",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/status:statusor",
    ],
)
