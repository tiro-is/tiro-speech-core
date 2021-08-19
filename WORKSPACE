workspace(name = "com_gitlab_tiro_is_tiro_speech_core")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load(
    "@com_gitlab_tiro_is_tiro_speech_core//:repositories.bzl",
    "extra_repositories",
    "tiro_speech_core_repositories",
)

tiro_speech_core_repositories()

extra_repositories()

new_local_repository(
    name = "intel_mkl",
    build_file = "//third_party:intel_mkl.BUILD",
    path = "/opt/intel/mkl",
)

# This obviously only works on Linux x86_64
new_local_repository(
    name = "gfortran_math_runtime",
    build_file = "//third_party:gfortran_math_runtime.BUILD",
    path = "/usr/lib/x86_64-linux-gnu",
)

#############################
# External assets for testing
http_file(
    name = "clarin_electra_punctuation_model_traced",
    sha256 = "bb081069e5b26aa198c71e6b2d7aa74f64f68bffeca714fcab014820012050a0",
    urls = ["https://storage.googleapis.com/tiro-is-public-assets/cadia-lvl/punctuation-prediction/14-09-2020/traced_electra.pt"],
    downloaded_file_path = "traced_electra.pt",
)

http_file(
    name = "clarin_electra_punctuation_vocab",
    sha256 = "cb152ddbcb4da9a37a1c4e7e8f13d2ff08d48e520f53ac1b277454d1ee3773c8",
    urls = [
        "https://repository.clarin.is/repository/xmlui/bitstream/handle/20.500.12537/52/vocab.txt",
        "https://storage.googleapis.com/tiro-is-public-assets/cadia-lvl/punctuation-prediction/14-09-2020/vocab.txt",
    ],
    downloaded_file_path = "vocab.txt",
)

###################
# rules_foreign_cc
#
# Needed for building non-trivial non-Bazel external dependencies
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

#####################
# Bazel rules for Go
#
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(go_version = "1.15.5")

#################################################
# Import dependencies of gRPC into the workspace.
#
# We have already declared our own versions of some of these in
# tiro_talgreinir_core_repositories()
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

# Extra deps calls go_register_toolchains, which is a no-op since we have
# already called it above
load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

###################
# rules_proto_grpc
#
# Complements the native proto rules
load("@rules_proto_grpc//:repositories.bzl", "rules_proto_grpc_repos", "rules_proto_grpc_toolchains")

rules_proto_grpc_toolchains()

rules_proto_grpc_repos()

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

rules_proto_dependencies()

rules_proto_toolchains()

########################
# GoogleAPIs Proto and gRPC libraries
#
# We generate libraries for the APIs we use and/or implement (except for Go,
# that's done elsewhere)
load(
    "@com_google_googleapis//:repository_rules.bzl",
    "switched_rules_by_language",
)

switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
    # csharp = True,
    # gapic = True,
    # go = True,
    grpc = True,
    # java = True,
    # nodejs = True,
    # php = True,
    python = True,
    # ruby = True,
)

########################################
# Missing dependencies of bazel_gazelle
#
# Used to generate BUILD rules from go.mod which is used by some dependencies
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

gazelle_dependencies()

###############################
# Dependecies for grpc-gateway
load(
    "@grpc_ecosystem_grpc_gateway//:repositories.bzl",
    grpc_gateway_go_repos = "go_repositories",
)

grpc_gateway_go_repos()

##############################
# Dependencies for io_bazel_rules_docker
load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)

container_repositories()

load("@io_bazel_rules_docker//repositories:deps.bzl", container_deps = "deps")

container_deps()

load(
    "@io_bazel_rules_docker//cc:image.bzl",
    _cc_image_repos = "repositories",
)

_cc_image_repos()

load(
    "@io_bazel_rules_docker//go:image.bzl",
    _go_image_repos = "repositories",
)

_go_image_repos()

load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_pull",
)

# Base image for our cc_images
container_pull(
    name = "ubuntu_groovy_x86_64",
    digest = "sha256:754eb641a1ba98a8b483c3595a14164fa4ed7f4b457e1aa05f13ce06f8151723",
    registry = "index.docker.io",
    repository = "library/ubuntu",
)

###########################
# rules_gitops dependencies
load("@com_adobe_rules_gitops//gitops:deps.bzl", "rules_gitops_dependencies")

rules_gitops_dependencies()

load("@com_adobe_rules_gitops//gitops:repositories.bzl", "rules_gitops_repositories")

rules_gitops_repositories()
