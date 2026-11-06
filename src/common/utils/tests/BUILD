load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_test(
    name = "socket_utils_test",
    size = "small",
    srcs = ["socket_utils_test.cc"],
    deps = [
        "//src/common/utils:socket_utils",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
