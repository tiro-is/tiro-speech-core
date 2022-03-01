load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def tiro_speech_core_repositories():
    rules_cc_version = "daf6ace7cfeacd6a83e9ff2ed659f416537b6c74"
    rules_cc_sha256 = "b295cad8c5899e371dde175079c0a2cdc0151f5127acc92366a8c986beb95c76"
    http_archive(
        name = "rules_cc",
        urls = ["https://github.com/bazelbuild/rules_cc/archive/{}.zip".format(rules_cc_version)],
        strip_prefix = "rules_cc-{}".format(rules_cc_version),
        sha256 = rules_cc_sha256,
    )

    # commit d54c78ab86b40770ee19f0949db9d74a831ab9f0
    rules_foreign_cc_version = "0.7.1"
    rules_foreign_cc_sha256 = "7350440503d8e3eafe293229ce5f135e8f65a59940e9a34614714f6063df72c3"
    http_archive(
        name = "rules_foreign_cc",
        strip_prefix = "rules_foreign_cc-" + rules_foreign_cc_version,
        url = "https://github.com/bazelbuild/rules_foreign_cc/archive/{}.zip".format(rules_foreign_cc_version),
        sha256 = rules_foreign_cc_sha256,
    )

    kaldi_version = "89e59b93e631ed704a65b6ba04fdc789773f44dc"
    new_git_repository(
        name = "kaldi",
        remote = "https://github.com/tiro-is/kaldi.git",
        commit = kaldi_version,
        shallow_since = "1627474359 +0000",
        build_file = "//third_party:kaldi.BUILD",
        strip_prefix = "src",
        patch_cmds = [
            # Ignore extra arguments, so we can use configure_make from rules_foreign_cc
            "sed -i 's/*)  echo \"Unknown argument: $1, exiting\"; usage; exit 1 ;;/*) shift ;;/' configure"
        ],
    )

    openfst_version = "1.8.1"
    http_archive(
        name = "openfst",
        urls = [
            "http://www.openfst.org/twiki/pub/FST/FstDownload/openfst-{}.tar.gz".format(openfst_version),
        ],
        sha256 = "24fb53b72bb687e3fa8ee96c72a31ff2920d99b980a0a8f61dda426fca6713f0",
        strip_prefix = "openfst-" + openfst_version,
    )

    # OpenBLAS source code repository
    openblas_version = "0.3.7"
    http_archive(
        name = "openblas",
        urls = [
            "https://github.com/xianyi/OpenBLAS/archive/v{}.tar.gz".format(openblas_version),
        ],
        sha256 = "bde136122cef3dd6efe2de1c6f65c10955bbb0cc01a520c2342f5287c28f9379",
        build_file = "//third_party:openblas.BUILD",
        strip_prefix = "OpenBLAS-{}".format(openblas_version),
    )

    # spdlog
    spdlog_version = "1.8.0"
    http_archive(
        name = "spdlog",
        urls = [
            "https://github.com/gabime/spdlog/archive/v{}.tar.gz".format(spdlog_version),
        ],
        sha256 = "1e68e9b40cf63bb022a4b18cdc1c9d88eb5d97e4fd64fa981950a9cacf57a4bf",
        strip_prefix = "spdlog-{}".format(spdlog_version),
        build_file = "//third_party:spdlog.BUILD",
    )

    rules_python_version = "0.6.0"
    rules_python_sha256 = "a30abdfc7126d497a7698c29c46ea9901c6392d6ed315171a6df5ce433aa4502"
    http_archive(
        name = "rules_python",
        url = "https://github.com/bazelbuild/rules_python/archive/{v}.tar.gz".format(v = rules_python_version),
        strip_prefix = "rules_python-{v}".format(v = rules_python_version),
        sha256 = rules_python_sha256,
    )

    # gRPC
    #
    # Import this before we import "rules_proto_grpc" to control gRPC versioning
    grpc_version = "1.32.0"
    http_archive(
        name = "com_github_grpc_grpc",
        urls = ["https://github.com/grpc/grpc/archive/v{}.tar.gz".format(grpc_version)],
        sha256 = "f880ebeb2ccf0e47721526c10dd97469200e40b5f101a0d9774eb69efa0bd07a",
        strip_prefix = "grpc-{}".format(grpc_version),
    )

    # rules_proto_grpc
    rules_proto_grpc_version = "2.0.0"
    http_archive(
        name = "rules_proto_grpc",
        urls = [
            "https://github.com/rules-proto-grpc/rules_proto_grpc/archive/{}.tar.gz".format(rules_proto_grpc_version),
        ],
        sha256 = "d771584bbff98698e7cb3cb31c132ee206a972569f4dc8b65acbdd934d156b33",
        strip_prefix = "rules_proto_grpc-{}".format(rules_proto_grpc_version),
    )

    googleapis_version = "c8bfd324b41ad1f6f65fed124572f92fe116517b"
    http_archive(
        name = "com_google_googleapis",
        urls = [
            "https://github.com/googleapis/googleapis/archive/{}.zip".format(googleapis_version),
        ],
        sha256 = "63a25daee9f828582dc9266ab3f050f81b7089ce096c8c584ce85548c71ce7f1",
        strip_prefix = "googleapis-{}".format(googleapis_version),
    )

    ffmpeg_version = "4.3.1"
    http_archive(
        name = "ffmpeg",
        urls = ["https://ffmpeg.org/releases/ffmpeg-{}.tar.gz".format(ffmpeg_version)],
        sha256 = "45035f15d6f192772de2309c846e1d60472694f479679354a39c699719e53772",
        strip_prefix = "ffmpeg-{}".format(ffmpeg_version),
        build_file = "//third_party:ffmpeg.BUILD",
    )

    # Catch2
    catch2_version = "2.13.8"
    catch2_sha256 = None
    http_archive(
        name = "catch2",
        urls = ["https://github.com/catchorg/Catch2/archive/v{}.tar.gz".format(catch2_version)],
        sha256 = catch2_sha256,
        strip_prefix = "Catch2-{}".format(catch2_version),
    )

    # fmt
    fmt_version = "7.1.3"
    http_archive(
        name = "fmt",
        urls = [
            "https://github.com/fmtlib/fmt/releases/download/{fmt}/fmt-{fmt}.zip".format(fmt = fmt_version),
        ],
        sha256 = "5d98c504d0205f912e22449ecdea776b78ce0bb096927334f80781e720084c9f",
        strip_prefix = "fmt-{}".format(fmt_version),
        build_file = "//third_party:fmt.BUILD",
    )

    http_archive(
        name = "bzip2",
        urls = [
            "https://src.fedoraproject.org/repo/pkgs/bzip2/bzip2-1.0.6.tar.gz/00b516f4704d4a7cb50a1d97e6e8e15b/bzip2-1.0.6.tar.gz",
            "http://anduin.linuxfromscratch.org/LFS/bzip2-1.0.6.tar.gz",
            "https://fossies.org/linux/misc/bzip2-1.0.6.tar.gz",
            "http://www.bzip.org/1.0.6/bzip2-1.0.6.tar.gz",
        ],
        sha256 = "a2848f34fcd5d6cf47def00461fcb528a0484d8edef8208d6d2e2909dc61d9cd",
        strip_prefix = "bzip2-1.0.6",
        build_file = "//third_party:bzip2.BUILD",
    )

    bazel_deps_version = "457a92bc426f23146d4edeb72fb96fd214684426"
    http_archive(
        name = "com_github_mjbots_bazel_deps",
        urls = ["https://github.com/mjbots/bazel_deps/archive/{}.zip".format(bazel_deps_version)],
        sha256 = "520cbe8cc5e960ba39f079e9001b8f992e4614cab08720b27afa47a65fe8566b",
        strip_prefix = "bazel_deps-{}".format(bazel_deps_version),
    )

    bazel_gazelle_version = "0.22.2"
    http_archive(
        name = "bazel_gazelle",
        sha256 = "b85f48fa105c4403326e9525ad2b2cc437babaa6e15a3fc0b1dbab0ab064bc7c",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v{v}/bazel-gazelle-v{v}.tar.gz".format(v = bazel_gazelle_version),
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v{v}/bazel-gazelle-v{v}.tar.gz".format(v = bazel_gazelle_version),
        ],
    )

    # Needed for buildifier
    com_github_bazelbuild_buildtools_version = "3.5.0"
    http_archive(
        name = "com_github_bazelbuild_buildtools",
        strip_prefix = "buildtools-{}".format(com_github_bazelbuild_buildtools_version),
        url = "https://github.com/bazelbuild/buildtools/archive/{}.zip".format(com_github_bazelbuild_buildtools_version),
        sha256 = "f5b666935a827bc2b6e2ca86ea56c796d47f2821c2ff30452d270e51c2a49708",
    )

    io_bazel_rules_go_version = "0.24.11"
    http_archive(
        name = "io_bazel_rules_go",
        sha256 = "dbf5a9ef855684f84cac2e7ae7886c5a001d4f66ae23f6904da0faaaef0d61fc",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v{v}/rules_go-v{v}.tar.gz".format(v = io_bazel_rules_go_version),
            "https://github.com/bazelbuild/rules_go/releases/download/v{v}/rules_go-v{v}.tar.gz".format(v = io_bazel_rules_go_version),
        ],
    )

    grpc_ecosystem_grpc_gateway_version = "2.1.0"
    http_archive(
        name = "grpc_ecosystem_grpc_gateway",
        sha256 = "2bca9747a3726497bd2cce43502858be936e23f13b6be292eb42bd422018e439",
        urls = [
            "https://github.com/grpc-ecosystem/grpc-gateway/archive/v{}.tar.gz".format(grpc_ecosystem_grpc_gateway_version),
        ],
        strip_prefix = "grpc-gateway-{}".format(grpc_ecosystem_grpc_gateway_version),
    )

    io_bazel_rules_docker_version = "0.23.0"
    io_bazel_rules_docker_sha256 = "85ffff62a4c22a74dbd98d05da6cf40f497344b3dbf1e1ab0a37ab2a1a6ca014"
    http_archive(
        name = "io_bazel_rules_docker",
        sha256 = io_bazel_rules_docker_sha256,
        strip_prefix = "rules_docker-{}".format(io_bazel_rules_docker_version),
        urls = [
            "https://github.com/bazelbuild/rules_docker/releases/download/v{v}/rules_docker-v{v}.tar.gz".format(v = io_bazel_rules_docker_version),
        ],
    )

    # NOTE: This is a binary distribution that includes all of libtorch's
    #       dependencies which might cause conflicts. The repo has Bazel build
    #       rules that we might want to migrate to, especially if we want to
    #       target architectures other than x86_64
    libtorch_version = "1.8.1"
    http_archive(
        name = "pytorch",
        sha256 = "ca4ab52992a52e2ca281656ceda732c07ea4dacdccb7fdec987032b12aae7c57",
        urls = ["https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-static-with-deps-{}%2Bcpu.zip".format(libtorch_version)],
        strip_prefix = "libtorch",
        build_file = "//third_party:libtorch.BUILD",
    )


    http_archive(
        name = "icu",
        strip_prefix = "icu-release-64-2",
        sha256 = "10cd92f1585c537d937ecbb587f6c3b36a5275c87feabe05d777a828677ec32f",
        urls = [
            "https://github.com/unicode-org/icu/archive/release-64-2.zip",
        ],
        build_file = "//third_party:icu.BUILD",
    )

    moodycamel_readerwriterqueue_version = "1.0.5"
    moodycamel_readerwriterqueue_sha256 = "066cd2ba252ece4323a07e0f5d394fc45f793340d7d4112888986a1d5aea6901"
    http_archive(
        name = "moodycamel_readerwriterqueue",
        strip_prefix = "readerwriterqueue-{}".format(moodycamel_readerwriterqueue_version),
        urls = ["https://github.com/cameron314/readerwriterqueue/archive/v{}.zip".format(moodycamel_readerwriterqueue_version)],
        sha256 = moodycamel_readerwriterqueue_sha256,
        build_file = "//third_party:moodycamel_readerwriterqueue.BUILD",
    )

    org_opengrm_thrax_version = "1.3.6"
    org_opengrm_thrax_sha256 = "5f00a2047674753cba6783b010ab273366dd3dffc160bdb356f7236059a793ba"
    http_archive(
        name = "org_opengrm_thrax",
        urls = ["https://www.opengrm.org/twiki/pub/GRM/ThraxDownload/thrax-{}.tar.gz".format(org_opengrm_thrax_version)],
        sha256 = org_opengrm_thrax_sha256,
        strip_prefix = "thrax-{}".format(org_opengrm_thrax_version),
        repo_mapping = {
            "@org_openfst": "@openfst",
        },
    )

def extra_repositories():
    # com_grail_bazel_compdb_version = "0.4.5"
    # http_archive(
    #     name = "com_grail_bazel_compdb",
    #     strip_prefix = "bazel-compilation-database-{}".format(
    #         com_grail_bazel_compdb_version,
    #     ),
    #     urls = [
    #         "https://github.com/grailbio/bazel-compilation-database/archive/{}.tar.gz".format(
    #             com_grail_bazel_compdb_version,
    #         ),
    #     ],
    #     sha256 = "bcecfd622c4ef272fd4ba42726a52e140b961c4eac23025f18b346c968a8cfb4",
    # )

    # Needed for k8s deployment
    rules_gitops_version = "3ee1ae1f6f4efdd1a57b7a60474e9f61eeeeb8b7"
    rules_gitops_sha256 = "3ac020eb39724c760d469b1a281f8a816d3f56e8874399218c4578e76b67cfed"
    http_archive(
        name = "com_adobe_rules_gitops",
        sha256 = rules_gitops_sha256,
        strip_prefix = "rules_gitops-{}".format(rules_gitops_version),
        urls = ["https://github.com/adobe/rules_gitops/archive/{}.zip".format(rules_gitops_version)],
    )
