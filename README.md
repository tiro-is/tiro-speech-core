# Tiro Speech Core

Tiro Speech Core is a speech recognition server that implements a
[gRPC](./proto/tiro/speech/v1alpha/speech.proto) API. Support for REST is
provided with through a gRPC REST gateway.

## Dependencies

Tiro Speech Core uses the Bazel build system and will fetch all required
dependencies over the network.

## Bulding and Running

Build and run the server with a pre-downloaded model:

    bazel run //:tiro_speech_server -- --kaldi-models=path/to/model/dir --listen-address=0.0.0.0:50051

Build and test with the example client:

    bazel run //:tiro_speech_client -- path/to/example.mp3 

Build and run the REST gateway:

    bazel run //rest-gateway/cmd:rest_gateway_server -- --endpoint=localhost:50051

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
