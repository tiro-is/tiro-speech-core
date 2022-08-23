// Copyright 2021 Tiro ehf.
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
#ifndef TIRO_SPEECH_SRC_ITN_PUNCTUATION_H_
#define TIRO_SPEECH_SRC_ITN_PUNCTUATION_H_

#include <torch/script.h>

#include <string>
#include <vector>

#include "src/itn/wordpiece.h"
#include "src/options.h"

namespace tiro_speech::itn {

struct ElectraPunctuatorConfig {
  std::string pytorch_jit_model_filename = "";
  int cls_token_id = 2;
  int sep_token_id = 0;
  WordPieceTokenizerConfig word_piece_opts{};

  void Register(OptionsItf* opts) {
    opts->Register("pytorch-jit-model", &pytorch_jit_model_filename,
                   "Traced PyTorch/TorchScript model");
    opts->Register("cls-token-id", &cls_token_id, "CLS token id");
    opts->Register("sep-token-id", &sep_token_id, "SEP token id");

    ParseOptions word_piece_po{"word-piece", opts};
    word_piece_opts.Register(&word_piece_po);
  };
};

class ElectraPunctuator {
 public:
  explicit ElectraPunctuator(const ElectraPunctuatorConfig& opts)
      : module_{torch::jit::load(opts.pytorch_jit_model_filename)},
        tokenizer_{opts.word_piece_opts},
        cls_token_id_{opts.cls_token_id},
        sep_token_id_{opts.sep_token_id} {}

  /**
   * Automatically predict and add punctuation to the ends of words.
   *
   * @note This can't be const since jit::Module::forward isn't. But it is
   *       probably thread-safe for the models we're using.
   */
  std::vector<std::string> Punctuate(const std::vector<std::string>& words,
                                     bool capitalize = false);

  std::vector<std::string> PunctuateWithContext(
      const std::vector<std::string>& words,
      const std::vector<std::string>& left_context, bool capitalize = false);

 private:
  torch::jit::Module module_;
  WordPieceTokenizer tokenizer_;
  int cls_token_id_;
  int sep_token_id_;
};

}  // namespace tiro_speech::itn

#endif  // TIRO_SPEECH_SRC_ITN_PUNCTUATION_H_
