# This is very minimal
cc_library(
    name = "torch_cpu",
    srcs = [
        "lib/libtorch_cpu.so",
        "lib/libc10.so",
        "lib/libgomp-75eea7e8.so.1",
    ],
    linkopts = [
        "-ltorch_cpu",
        "-lc10",
    ],
    hdrs = glob(["include/**/*.h"]),
    includes = [
        "include",
        "include/torch/csrc/api/include",
        "include/THC",
        "include/TH",
    ],
    visibility = ["//visibility:public"]
)
