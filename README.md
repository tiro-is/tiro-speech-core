# Tiro Speech Core

## Dependencies

Tiro Speech Core uses the Bazel build system and will fetch all required
dependencies over the network.

## Building

Build the server application:

    bazel build //:tiro_speech_server
    
Build a simple example C++ client:

    bazel build //:tiro_speech_client

## Running

Run the server with a pre-downloaded model:

    bazel run //:tiro_speech_server -- --kaldi-models=path/to/model/dir --listen-address=0.0.0.0:50051

Test with the example client:

    bazel run //:tiro_speech_client -- path/to/example.mp3 

## Development

Enable the git hooks to automatically format source code:

    git config core.hooksPath hooks
