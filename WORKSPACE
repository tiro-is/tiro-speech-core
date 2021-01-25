workspace(name = "com_gitlab_tiro_is_tiro_speech_core")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(
    "@com_gitlab_tiro_is_tiro_speech_core//:repositories.bzl",
    "tiro_speech_core_repositories",
)

tiro_speech_core_repositories()

###################
# rules_foreign_cc
#
# Needed for building non-trivial non-Bazel external dependencies
load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")

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
