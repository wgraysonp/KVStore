load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "mock_handler",
    hdrs = ["mock_handler.h"],
    deps = [
        "//src/rpc_framework:rpc_interface",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "mock_handler_factory",
    hdrs = ["mock_handler_factory.h"],
    deps = [
        "//src/rpc_framework:rpc_interface",
        "//src/rpc_framework/tests:mock_handler",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
    ],
)

cc_test(
    name = "rpc_registry_test",
    size = "small",
    srcs = ["rpc_registry_test.cc"],
    deps = [
        "//src/rpc_framework:rpc_interface",
        "//src/rpc_framework:rpc_registry",
        "//src/rpc_framework/tests:mock_handler",
        "//src/rpc_framework/tests:mock_handler_factory",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
