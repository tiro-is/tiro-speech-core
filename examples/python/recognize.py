import os
from typing import Literal, Tuple, Union
import grpc
from tiro.speech.v1alpha import speech_pb2, speech_pb2_grpc


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--server-url", "-c", default="speech.tiro.is:443", help="Server to connect to"
    )

    def supported_filetypes(fname: str,) -> Tuple[bytes, int]:
        _, ext = os.path.splitext(fname)
        if ext not in (".wav", ".flac", ".mp3"):
            raise ValueError("Unsupported extension!")
        encoding = speech_pb2.RecognitionConfig.LINEAR16
        if ext == ".flac":
            encoding = speech_pb2.RecognitionConfig.FLAC
        elif ext == ".mp3":
            encoding = speech_pb2.RecognitionConfig.MP3
        with open(fname, "rb") as f:
            return (f.read(), encoding)

    parser.add_argument("audio_file", type=supported_filetypes)
    parser.add_argument(
        "--sample-rate-hertz",
        "-r",
        type=int,
        default=16000,
        help="Sample rate of audio_file in Hertz",
    )
    parser.add_argument(
        "--no-timestamp-first",
        action="store_true",
        help="Disable timestamp output on top result",
    )
    parser.add_argument(
        "--nbest",
        "-n",
        metavar="N",
        type=int,
        default=2,
        help="Return up to N best results",
    )

    args = parser.parse_args()

    with grpc.secure_channel(
        args.server_url, grpc.ssl_channel_credentials()
    ) as channel:
        stub = speech_pb2_grpc.SpeechStub(channel)
        response = stub.Recognize(
            speech_pb2.RecognizeRequest(
                config=speech_pb2.RecognitionConfig(
                    encoding=args.audio_file[1],
                    language_code="is-IS",
                    sample_rate_hertz=args.sample_rate_hertz,
                    max_alternatives=args.nbest,
                    enable_word_time_offsets=not args.no_timestamp_first,
                ),
                audio=speech_pb2.RecognitionAudio(content=args.audio_file[0],),
            )
        )

        for result in response.results:
            for idx, alternative in enumerate(result.alternatives):
                out = alternative.transcript
                if not args.no_timestamp_first and idx == 0:
                    out = " ".join(
                        "{}<{}>".format(w.word, w.end_time.ToTimedelta())
                        for w in alternative.words
                    )
                print("{}\t{}".format(idx + 1, out))


if __name__ == "__main__":
    main()
