cc_library(
    name = "spdlog",
    hdrs = glob(["include/**/*.h"]),
    defines = ["SPDLOG_FMT_EXTERNAL"],
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "@fmt",
    ],
)
