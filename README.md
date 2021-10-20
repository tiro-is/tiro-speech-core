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

To only build (note that `opt` stands for an optimized compilation mode):

    bazel build -c opt //:tiro_speech_server

The output binary should now be in `bazel-bin/tiro_speech_server`.

Build and test with the example client:

    bazel run -c opt //:tiro_speech_client -- $PWD/examples/is_is-mbl_01-2011-12-02T14:22:29.744483.wav $PWD/examples/config.pbtxt 0.0.0.0:50051

Build and run the REST gateway:

    bazel run -c opt //rest-gateway/cmd:rest_gateway_server -- --endpoint=localhost:50051

The REST gateway server should now be running on port 8080.

## Preparing a model for use with the server

Currently, Tiro Speech Core only supports the use of Kaldi chain models. To
prepare a model for use with the server one needs to use the script
[tools/models/prepare_chain_dist.sh](tools/models/prepare_chain_dist.sh) which
has the usage:

    Usage: tools/models/prepare_chain_dist.sh <output-dir>
      --lang-dir <str|data/lang>
      --ivector-extractor-dir <str|exp/nnet3/extractor>
      --nnet-dir <str|exp/chain/tdnn_sp_bi>
      --graph-dir <str|exp/chain/tdnn_sp_bi/graph>
      --lang-code <str|>      # BCP-47 language code for model
      --description <str|>  # short description of the model
      --mfcc-config <str|conf/mfcc_hires.conf>
      --fbank-config <str|> # Set either fbank-config or mfcc-config
      --const-arpa <str|>    # ConstArpa model for rescoring, empty string == no rescoring
      --model-name <str|>    # descriptive model name, used as a key


## Do I have to run this my self??

No. The service is available at `speech.tiro.is:443`.

    bazel run -c opt //:tiro_speech_client -- --use-ssl $PWD/examples/is_is-mbl_01-2011-12-02T14:22:29.744483.wav $PWD/examples/config.pbtxt speech.tiro.is:443

## Compiling the denormilization FST's
Tiro Speech Core uses [OpenGram Thrax Grammer](https://www.openfst.org/twiki/bin/view/GRM/Thrax) for denormalization. The rules are located in `src/itn/`. 

The `abbriviate` target compiles the grammar rules along with the mappings. This will result in a finite-state archive (.far) in `bazel-bin/src/itn/`. 

    bazel build :abbreviate

We need to extract the FST. The following command will create `ABBREVIATE.fst` which should be stored along with the model. 

    bazel run -c opt @openfst//:farextract -- --filename_prefix=$PWD/models/norm --filename_suffix=.fst --keys=ABBREVIATE $PWD/bazel-bin/src/itn/abbreviate.far 

Next, compile the rewrite FST. Provide the symbol table located in `graph`. Note that `<space>` has to be an entry in the symbol table. 
    
    bazel run -c opt :prepare_lexicon_fst -- $PWD/models/graph/words.txt $PWD/models/norm/ABBREVIATE.fst $PWD/models/norm/rewrite_lex.fst

Finally, add the following flags with the associate path into the `main.conf` model fileþ 

    --formatter.rewrite-fst=norm/ABBREVIATE.fst
    --formatter.lexicon-fst=norm/rewrite_lex.fst


### Example usage of the REST gateway

Using `curl` on Linux:

    cat <<EOF | curl -XPOST https://speech.tiro.is/v1alpha/speech:recognize -d@-
    {
        "config": {
            "languageCode": "is-IS",
            "sampleRateHertz": "16000",
            "encoding": "LINEAR16",
            "maxAlternatives": 2,
            "enableWordTimeOffsets": true
        },
        "audio": {
            "content": "$(base64 -w0 < examples/is_is-mbl_01-2011-12-02T14:22:29.744483.wav)"
        }
    }
    EOF

Which returns the following:

    {
      "results": [
        {
          "alternatives": [
            {
              "transcript": "gera lítið úr meintri spennu",
              "confidence": 0,
              "words": [
                {
                  "startTime": "1.020s",
                  "endTime": "1.289s",
                  "word": "gera",
                  "confidence": 0
                },
                {
                  "startTime": "1.289s",
                  "endTime": "1.649s",
                  "word": "lítið",
                  "confidence": 0
                },
                {
                  "startTime": "1.650s",
                  "endTime": "1.799s",
                  "word": "úr",
                  "confidence": 0
                },
                {
                  "startTime": "1.799s",
                  "endTime": "2.129s",
                  "word": "meintri",
                  "confidence": 0
                },
                {
                  "startTime": "2.130s",
                  "endTime": "2.610s",
                  "word": "spennu",
                  "confidence": 0
                }
              ]
            },
            {
              "transcript": "gerir lítið úr meintri spennu",
              "confidence": 0,
              "words": []
            }
          ]
        }
      ]
    }

### Example usage of the gRPC API 

See [examples/python/README.md](examples/python/README.md) for a Python example.

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
