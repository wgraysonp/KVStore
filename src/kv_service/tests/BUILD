load("@rules_cc//cc:defs.bzl", "cc_test")


cc_test(
    name = "epoll_server_test",
    size = "small",
    srcs = ["epoll_server_test.cc"],
    deps = [
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "//src/kv_service:epoll_server"
    ],
)