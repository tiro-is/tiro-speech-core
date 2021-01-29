# Example usage of the gRPC API with Python (Linux)

First we create our virtual environment and install the requirements:

    python3 -m venv .venv
    . .venv/bin/activate
    pip install -r requirements-dev.txt

Then we have to generate Python code from the Protobuf service specification in
[../../proto/tiro/speech/v1alpha/speech.proto](../../proto/tiro/speech/v1alpha/speech.proto). Before
we do that we need some common Google API Protos since `speech.proto` depends on
those. Using `git` we can do:

    git clone --depth=1 https://github.com/googleapis/api-common-protos.git googleapis

And then we can generate the required code:

    python -m grpc_tools.protoc \
        --python_out=. --grpc_python_out=. -I../../proto -Igoogleapis \
        ../../proto/tiro/speech/v1alpha/speech.proto

This will generate the files `tiro/speech/v1alpha/speech_pb2_grpc.py` and
`tiro/speech/v1alpha/speech_pb2.py`. Which you might commit to into version
control. Note that the `googleapis` directory is only needed to generate the
`*pb2*.py` files and does not need to be committed into version control.

Among the things the above command generated are the necessary Python classes
and a client "stub" to call the `Recognize` RPC described in
[../../proto/tiro/speech/v1alpha/speech.proto](../../proto/tiro/speech/v1alpha/speech.proto). To
use it we import `grpc` and our generated modules:

    import grpc
    from tiro.speech.v1alpha.speech_pb2 import (RecognizeRequest,
                                                RecognitionConfig,
                                                RecognitionAudio)
    from tiro.speech.v1alpha.speech_pb2_grpc import SpeechStub

We can then use it to get the 2 best transcripts for a short WAV file sampled at
16 kHz:

    with grpc.secure_channel(
        server_url, grpc.ssl_channel_credentials()
    ) as channel:
        stub = SpeechStub(channel)
        response = stub.Recognize(
            RecognizeRequest(
                config=RecognitionConfig(
                    encoding=RecognitionConfig.LINEAR16,
                    language_code="is-IS",
                    sample_rate_hertz=16000,
                    max_alternatives=2,
                ),
                audio=RecognitionAudio(content=audio_file_content),
            )
        )

And we can print out the top transcript from the `response` object:

    >>> print(response.results[0].alternatives[0].transcript)
    gera lítið úr meintri spennu

Note that although the response could contain multiple results for sequential
segments of audio a call to `Recognize` will generally consider the whole audio
file a single segment.

See [recognize.py](./recognize.py) for a full working example.
