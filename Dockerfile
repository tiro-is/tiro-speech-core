# Example Dockerfile that build everything from source
FROM debian:11-slim AS build
ARG BAZEL_VERSION=4.0.0
RUN apt-get update -qq \
    && DEBIAN_FRONTEND=noninteractive apt-get install -yqq \
    build-essential g++-9 curl git nasm patchelf ca-certificates zlib1g-dev
RUN curl https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS-2019.PUB \
    | gpg --dearmor | tee /usr/share/keyrings/intel-sw-products.gpg > /dev/null
RUN echo "deb [signed-by=/usr/share/keyrings/intel-sw-products.gpg] https://apt.repos.intel.com/mkl all main" \
    | tee /etc/apt/sources.list.d/mkl.list
RUN apt-get update -qq && \
    DEBIAN_FRONTEND=noninteractive apt-get install -yqq \
    intel-mkl-64bit-2020.0-088 \
    && rm -rf /var/lib/{apt,dpkg,log,cache}/

RUN curl --fail -L --output /usr/bin/bazel https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-linux-x86_64 \
        && chmod a+x /usr/bin/bazel
WORKDIR /src
COPY . .
RUN bazel build //:tiro_speech_server -c opt --@kaldi//:mathlib=mkl
RUN mkdir -p solibs && find bazel-bin/tiro_speech_server.runfiles -name '*.so*' -exec sh -c 'cp `readlink -ne "$1"` solibs/' sh '{}' \;

FROM debian:11-slim
COPY --from=build /src/bazel-bin/tiro_speech_server /bin/tiro_speech_server
COPY --from=build /src/solibs/* /usr/lib/
ENTRYPOINT ["/bin/tiro_speech_server"]
