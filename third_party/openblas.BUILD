package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

# OpenBLAS
load("@rules_foreign_cc//tools/build_defs:make.bzl", "make")

make(
    name = "openblas",
    lib_source = "@openblas//:all",
    linkopts = [
        "-lgfortran",
        "-lquadmath",
    ],
    make_commands = [
        "make PREFIX=$$INSTALLDIR$$ USE_LOCKING=1 USE_THREAD=0 DYNAMIC_ARCH=1 all install",
    ],
    static_libraries = [
        "libopenblas.a",
        "libopenblas-r0.3.7.a",
    ],
)
