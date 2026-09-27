load("@rules_cc//cc:defs.bzl", "cc_test")

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
