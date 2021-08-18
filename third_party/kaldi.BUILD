# Kaldi
#
# TODO(rkjaran): Split up the kaldi subdirs so we don't have to depend on and
#                build everything. (Or, perhaps we'll just grab kaldi into the tree
#                and write a BUILD file)
package(
    default_visibility = ["//visibility:public"],
)

load("@rules_foreign_cc//foreign_cc:configure.bzl", "configure_make")
load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("@//tools/cc:defs.bzl", "cc_static_library")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

config_setting(
    name = "opt_build",
    values = {
        "compilation_mode": "opt",
    },
)

string_flag(
    name = "mathlib",
    build_setting_default = "mkl",
    values = [
        "mkl",
        "openblas",
    ],
)

config_setting(
    name = "use_mkl",
    flag_values = {":mathlib": "mkl"},
)

config_setting(
    name = "use_openblas",
    flag_values = {":mathlib": "openblas"},
)

kaldi_libs = [
    # "kws",
    # "sgmm2",
    "rnnlm",
    "online2",
    "decoder",
    "lm",
    "nnet3",
    "chain",
    "nnet2",
    "nnet",
    "cudamatrix",
    "ivector",
    "lat",
    "hmm",
    "fstext",
    "feat",
    "transform",
    "gmm",
    "tree",
    "util",
    "matrix",
    "base",
]

base_configure_opts = [
    "--static",
    "--static-fst",
    "--fst-root=$$EXT_BUILD_DEPS$$",
    "--fst-version=1.8.1",  # TODO: obtain from WORKSPACE
    "--static-math",
    "--use-cuda=no",
]

# There's no OpenFST Bazel target for a libfst.a, so we make our own for Kaldi
cc_static_library(
    name = "libfst",
    deps = ["@openfst//:fst"],
)

openfst_deps = [
    ":libfst",
    "@openfst//:far",
    "@openfst//:ngram",
    "@openfst//:fstscript_print",
] + [
    "@openfst//:%s_lookahead-fst" % type
        for type in [
                "arc",
                "ilabel",
                "olabel",
        ]
]

mkl_configure_opts = [
    "--mathlib=MKL",
    "--mkl-libdir=$$EXT_BUILD_DEPS$$/lib",
    "--mkl-root=$$EXT_BUILD_DEPS$$",
]

openblas_configure_opts = [
    "--mathlib=OPENBLAS",
    "--openblas-root=$$EXT_BUILD_DEPS$$/openblas",
]

KALDI_DEFINES = [
    "KALDI_DOUBLEPRECISION=0",
    "HAVE_EXECINFO_H=0",
    "HAVE_CXXABI_H",
    "HAVE_OPENFST_GE_10600",
] + select({
    ":use_mkl": ["HAVE_MKL"],
    ":use_openblas": ["HAVE_OPENBLAS"],
})

configure_make(
    name = "kaldi",
    defines = KALDI_DEFINES,
    lib_source = "@kaldi//:all",
    configure_in_place = True,
    configure_options = base_configure_opts + select({
        ":use_mkl": mkl_configure_opts,
        ":use_openblas": openblas_configure_opts,
    }),
    targets = ["depend"] + kaldi_libs,
    postfix_script = """\
for lib in {}; do
  mkdir -p $$INSTALLDIR$$/include/$lib; cp $lib/*.h $$INSTALLDIR$$/include/$lib/
  cp $lib/kaldi-$lib.a $$INSTALLDIR$$/lib/
done
mkdir -p $$INSTALLDIR$$/include/itf && cp itf/*.h $$INSTALLDIR$$/include/itf/
""".format(" ".join(kaldi_libs)),
    out_include_dir = "include",
    out_lib_dir = "lib",
    out_static_libs = [
        "kaldi-" + lib + ".a"
        for lib in kaldi_libs
    ],
    deps = select({
        ":use_mkl": ["@intel_mkl//:mkl_x86_64"],
        ":use_openblas": ["@openblas//:openblas"],
    }) + openfst_deps,
    target_compatible_with = select({
        "@platforms//os:osx": ["@platforms//cpu:x86_64"],
        "@platforms//os:linux": ["@platforms//cpu:x86_64"],
        "//conditions:default": ["@platforms//:incompatible"],
    }),
)
