load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_binary(
    name = "kv_service",
    srcs = ["main.cc"],
    deps = [
        "//src/endpoint",
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_macros",
        "@abseil-cpp//absl/strings:strings",
    ],
)
