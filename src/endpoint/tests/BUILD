load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "mock_client_connection",
    hdrs = ["mock_client_connection.h"],
    deps = [
        "//src/endpoint:client_connection",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "epoll_server_test",
    size = "small",
    srcs = ["epoll_server_test.cc"],
    deps = [
        "//src/endpoint:epoll_server",
        "//src/rpc_framework/tests:mock_rpc_registry",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "socket_utils_test",
    size = "small",
    srcs = ["socket_utils_test.cc"],
    deps = [
        "//src/endpoint:socket_utils",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "client_connection_test",
    size = "small",
    srcs = ["client_connection_test.cc"],
    deps = [
        "//src/endpoint:client_connection",
        "//src/endpoint:consts",
        "//src/endpoint:socket_utils",
        "//src/rpc_framework/tests:mock_rpc_registry",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:status_matchers",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "connection_manager_test",
    size = "small",
    srcs = ["client_connection_test.cc"],
    deps = [
        "//src/endpoint:client_connection",
        "//src/endpoint/tests:mock_client_connection",
        "//src/rpc_framework/tests:mock_rpc_registry",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/status:status_matchers",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
