# Tiro Speech Core

Tiro Speech Core is a speech recognition server that implements a
[gRPC](./proto/tiro/speech/v1alpha/speech.proto) API. Support for REST is
provided with through a gRPC REST gateway.

## Dependencies

Tiro Speech Core uses the Bazel build system and will fetch most required
dependencies over the network. It's possible to use either Intel MKL or OpenBLAS
as the linear algebra library. MKL has to be downloaded from Intel and installed
in `/opt/intel/mkl`. OpenBLAS is downloaded and compiled automatically.

## Bulding and Running

The following command builds the server along with all its dependencies and run
it with a pre-downloaded model (using MKL):

    bazel run -c opt //:tiro_speech_server -- --kaldi-models=path/to/model/dir --listen-address=0.0.0.0:50051
    
To use OpenBLAS instead:

    bazel run -c opt --@kaldi//:mathlib=openblas //:tiro_speech_server -- --kaldi-models=path/to/model/dir --listen-address=0.0.0.0:50051

Build and test with the example client:

    bazel run -c opt //:tiro_speech_client -- $PWD/examples/is_is-mbl_01-2011-12-02T14:22:29.744483.wav

Build and run the REST gateway:

    bazel run -c opt //rest-gateway/cmd:rest_gateway_server -- --endpoint=localhost:50051

The REST gateway server should now be running on port 8080.

## Development

Enable the git hooks to automatically format source code:

    git config core.hooksPath hooks

## License

Tiro Speech Core is licensed under the Apache License, Version 2.0. See
[LICENSE](LICENSE) for more details.

## Acknowledgements

This project was funded by the Language Technology Programme for Icelandic
2019-2023. The programme, which is managed and coordinated by Almannarómur, is
funded by the Icelandic Ministry of Education, Science and Culture.
