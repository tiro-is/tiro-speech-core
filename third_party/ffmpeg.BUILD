package(default_visibility = ["//visibility:public"])

load("@rules_foreign_cc//tools/build_defs:configure.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

# TODO(rkjaran): Split this up into targets for each ffmpeg library
#                (libavformat, libavcodec, etc)
FFMPEG_LIBS = [
    "avdevice",
    "avfilter",
    "avformat",
    "avcodec",
    "swresample",
    "swscale",
    "avutil",
]

configure_make(
    name = "ffmpeg",
    configure_env_vars = {
        "LDFLAGS": "-lpthread",
    },
    configure_options = [
        "--disable-static",
        "--enable-shared",
        "--enable-openssl",
        "--enable-zlib",
        "--enable-bzlib",
        "--disable-doc",
        "--disable-programs",
    ],
    lib_source = ":all",
    shared_libraries = [
        "lib{}.so".format(lib)
        for lib in FFMPEG_LIBS
    ],
    deps = [
        "@boringssl//:ssl",
        "@bzip2",
        "@zlib",
    ],
)
