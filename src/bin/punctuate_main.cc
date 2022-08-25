// Copyright 2022 Tiro ehf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "src/itn/punctuation.h"
#include "src/logging.h"
#include "src/options.h"
#include "src/utils.h"

using namespace tiro_speech;
using namespace tiro_speech::itn;

int main(int argc, char* argv[]) {
  try {
    const char* usage =
        "Usage: tiro_speech_punctuate [options] < input_text.txt > output.txt"
        "\n\n"
        "E.g. tiro_server_punctuate \\\n"
        "       --capitalize \\\n"
        "       "
        "--pytorch-jit-model=external/clarin_electra_punctuation_model_traced/"
        "file/traced_electra.pt \\\n"
        "       "
        "--word-piece.vocab=external/clarin_electra_punctuation_vocab/file/"
        "vocab.txt";
    ParseOptions po{usage};

    bool do_capitalize = false;
    po.Register("capitalize", &do_capitalize, "Also do basic capitalization.");

    ElectraPunctuatorConfig punctuator_opts{};
    punctuator_opts.Register(&po);

    po.Read(argc, argv);

    if (po.NumArgs() > 0) {
      TIRO_SPEECH_ERROR("Program takes no arguments: \n\n{}", usage);
      return EXIT_FAILURE;
    }

    if (punctuator_opts.pytorch_jit_model_filename.empty() ||
        punctuator_opts.word_piece_opts.vocab_filename.empty()) {
      TIRO_SPEECH_ERROR(
          "You need to specify --pytorch-jit-model and --word-piece.vocab");
      return EXIT_FAILURE;
    }

    ElectraPunctuator punctuator{punctuator_opts};

    for (std::string line; std::getline(std::cin, line);) {
      std::vector<std::string> words;
      for (auto w : Split(line)) {
        words.emplace_back(w);
      }
      if (words.empty() || words[0].empty()) {
        continue;
      }
      auto punctuated_words = punctuator.Punctuate(words, do_capitalize);
      std::cout << Join(punctuated_words, " ") << '\n';
    }

    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    TIRO_SPEECH_ERROR(e.what());
    return EXIT_FAILURE;
  }
}
