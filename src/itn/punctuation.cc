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

#include "src/itn/punctuation.h"

#include <unicode/uchar.h>
#include <unicode/unistr.h>

namespace tiro_speech::itn {

namespace {
// TODO(rkjaran): read these from a file/option
// Hardcode these for now
const std::array<std::string, 4> kIdToLabel{"O", "COMMA", "PERIOD",
                                            "QUESTIONMARK"};
const std::array<std::string, 4> kIdToChar{"", ",", ".", "?"};
const std::array<bool, 4> kIdToCapitalizeNext{false, false, true, true};

}  // namespace

std::vector<std::string> ElectraPunctuator::Punctuate(
    const std::vector<std::string>& words, bool capitalize) {
  std::vector<std::string> word_pieces = tokenizer_.Tokenize(words);

  std::vector<int> input_ids{};
  input_ids.push_back(cls_token_id_);
  for (auto id : tokenizer_.TokensToIds(word_pieces)) {
    input_ids.push_back(id);
  }
  input_ids.push_back(sep_token_id_);

  torch::Tensor input_ids_tensor = torch::from_blob(
      input_ids.data(), {1, static_cast<int>(input_ids.size())},
      torch::TensorOptions{at::kInt});

  auto output = module_.forward({input_ids_tensor}).toTuple();

  torch::Tensor pred_ids_tensor =
      torch::argmax(output->elements()[0].toTensor(), 2, true).flatten();

  bool capitalize_next = false;
  for (std::size_t idx = 0; idx < word_pieces.size(); ++idx) {
    // First and last element are special tokens
    int char_id = pred_ids_tensor[idx + 1].item<int>();
    word_pieces[idx] += kIdToChar.at(char_id);
    if (capitalize_next) {
      auto upiece = icu::UnicodeString::fromUTF8(word_pieces[idx]);
      upiece.replace(0, 1, upiece.tempSubString(0, 1).toUpper());
    }
    capitalize_next = kIdToCapitalizeNext.at(char_id);
  }

  return tokenizer_.Merge(word_pieces);
}

}  // namespace tiro_speech::itn
