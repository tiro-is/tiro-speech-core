cc_library(
    name = "mkl_headers",
    hdrs = glob([
        "include/**/*.h",
    ]),
    includes = ["include/"],
)

cc_library(
    name = "mkl_x86_64",
    srcs = [
        ":lib/intel64/libmkl_core.a",
        ":lib/intel64/libmkl_intel_lp64.a",
        ":lib/intel64/libmkl_sequential.a",
    ],
    # Annoyingly there's a circular dependency between these libs, so we have to
    # resort to this hack
    linkopts = [
        "-Wl,--start-group " +
        "$(location :lib/intel64/libmkl_intel_lp64.a) " +
        "$(location :lib/intel64/libmkl_sequential.a) " +
        "$(location :lib/intel64/libmkl_core.a) " +
        "-Wl,--end-group",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":mkl_headers",
    ],
)
