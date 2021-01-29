# Example Dockerfile that build everything from source
FROM debian:buster-slim AS build
ARG BAZEL_VERSION=4.0.0
RUN apt-get update -qq \
        && apt-get install -yqq build-essential gfortran g++ curl git \
        && rm -rf /var/lib/{apt,dpkg,log,cache}/
RUN curl --fail -L --output /usr/bin/bazel https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-linux-x86_64 \
        && chmod a+x /usr/bin/bazel
WORKDIR /src
COPY . .
RUN bazel build //:tiro_speech_server -c opt --@kaldi//:mathlib=openblas

FROM debian:buster-slim
COPY --from=build /src/bazel-bin/tiro_speech_server /bin/tiro_speech_server
RUN apt-get update -qq && apt-get install -yqq build-essential libgfortran5 libquadmath0 \
        && rm -rf /var/lib/{apt,dpkg,log,cache}/
ENTRYPOINT ["/bin/tiro_speech_server"]
