package(default_visibility = ["//visibility:public"])

load("@rules_foreign_cc//foreign_cc:configure.bzl", "configure_make")

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
    env = {
        "LDFLAGS": "-lpthread",
    },
    configure_options = [
        "--enable-static",
        # "--enable-shared",
        "--enable-openssl",
        "--enable-zlib",
        "--enable-bzlib",
        "--disable-doc",
        "--disable-programs",
        "--disable-autodetect",
        "--disable-symver",
    ],
    lib_source = ":all",
    # TODO(rkjaran): Reenable shared libs when I figure out how to force ffmpeg
    # to link to shared libssl when building shared libs
    # out_shared_libs = [ "lib{}.so".format(lib) for lib in FFMPEG_LIBS ],
    out_static_libs = [
        "lib{}.a".format(lib)
        for lib in FFMPEG_LIBS
    ],
    deps = [
        "@boringssl//:ssl",
        "@bzip2//:bz2",
        "@zlib",
    ],
)
