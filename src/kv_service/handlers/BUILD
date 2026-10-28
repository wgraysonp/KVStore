load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "kv_get_handler",
    srcs = ["kv_get_handler.cc"],
    hdrs = ["kv_get_handler.h"],
    visibility = [
        "//src/kv_service:__pkg__",
    ],
    deps = [
        "//src/rpc_framework",
        "//src/storage:key_value_store",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)
