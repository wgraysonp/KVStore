load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "epoll_server_test",
    size = "small",
    srcs = ["epoll_server_test.cc"],
    deps = [
        "//src/kv_service:epoll_server",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
