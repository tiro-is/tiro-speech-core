package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

# OpenBLAS
load("@rules_foreign_cc//foreign_cc:make.bzl", "make")

OPENBLAS_TARGETS = [
    "NEHALEM",
    "SANDYBRIDGE",
    "HASWELL",
    "SKYLAKEX",
]

make(
    name = "openblas",
    out_binaries = [
        "tests/zblat1",
        "tests/sblat1",
        "tests/cblat1",
    ],
    lib_source = "@openblas//:all",
    linkopts = [
        "-lgfortran",
        "-lquadmath",
    ],
    args = [
        "PREFIX=$$INSTALLDIR$$",
        "USE_LOCKING=1",
        "USE_THREAD=0",
        "DYNAMIC_ARCH=1",
        "DYNAMIC_LIST='{}'".format(" ".join([t for t in OPENBLAS_TARGETS])),
    ],
    out_shared_libs = [
        "libopenblas.so",
        "libopenblas-r0.3.7.so",
    ],
    out_static_libs = [
        "libopenblas.a",
        "libopenblas-r0.3.7.a",
    ],
    deps = [
        "@gfortran_math_runtime//:gfortran",
        "@gfortran_math_runtime//:quadmath",
    ],
)
