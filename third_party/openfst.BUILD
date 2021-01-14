package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

# OpenFST Has a BUILD file. It's missing some rules we need for Kaldi
# though. It's also broken
load("@rules_foreign_cc//tools/build_defs:configure.bzl", "configure_make")

configure_make(
    name = "openfst",
    configure_options = [
        "--enable-static",
        "--disable-shared",
        "--enable-far",
        "--enable-ngram-fsts",
        "--enable-grm",
    ],
    lib_source = "@openfst//:all",
    linkopts = ["-ldl"],
    static_libraries = [
        "libfst.a",
        "libfstfar.a",
        "libfstfarscript.a",
        "libfstmpdtscript.a",
        "libfstngram.a",
        "libfstpdtscript.a",
        "libfstscript.a",
    ],
)
