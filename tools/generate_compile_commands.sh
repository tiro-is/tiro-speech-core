#!/usr/bin/env bash
INSTALL_DIR="$HOME/.cache/bazel-compilation-database"
VERSION="0.4.5"
GENERATE_EXE="${INSTALL_DIR}/bazel-compilation-database-${VERSION}/generate.sh"

if [ ! -f "$GENERATE_EXE" ]; then
  # Download and symlink.
  (
    mkdir -p "${INSTALL_DIR}" \
      && cd "${INSTALL_DIR}" \
      && curl -L "https://github.com/grailbio/bazel-compilation-database/archive/${VERSION}.tar.gz" | tar -xz \
  )
fi

${GENERATE_EXE} -s
