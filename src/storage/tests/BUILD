load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_test(
    name = "write_ahead_test",
    size = "small",
    srcs = ["write_ahead_test.cc"],
    deps = [
        "//src/kv_service:data",
        "//src/storage:write_ahead",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "key_value_store_test",
    size = "small",
    srcs = ["key_value_store_test.cc"],
    deps = [
        "//src/kv_service:data",
        "//src/storage:key_value_store",
        "//src/storage/tests:mock_write_ahead_log",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_write_ahead_log",
    hdrs = ["mock_write_ahead_log.h"],
    deps = [
        "//src/kv_service:data",
        "//src/storage:write_ahead",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
    ],
)
