# The soname for this lib is incorect
genrule(
    name = "patched_libgomp",
    srcs = ["lib/libgomp-75eea7e8.so.1"],
    outs = ["libgomp-75eea7e8.so.1"],
    cmd = "cp $< $@ && patchelf --remove-needed libgomp.so.1 --set-soname libgomp-75eea7e8.so.1 $@"
)

cc_library(
    name = "torch_cpu",
    hdrs = glob(["include/**/*.h"]),
    srcs = [
        "lib/libtorch_cpu.so",
        ":patched_libgomp",
        "lib/libc10.so",
    ],
    linkstatic = True,
    includes = [
        "include",
        "include/torch/csrc/api/include",
        "include/THC",
        "include/TH",
    ],
    visibility = ["//visibility:public"]
)
