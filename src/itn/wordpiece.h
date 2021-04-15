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
#ifndef TIRO_SPEECH_SRC_ITN_WORDPIECE_H_
#define TIRO_SPEECH_SRC_ITN_WORDPIECE_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "src/options.h"

namespace tiro_speech::itn {

struct WordPieceTokenizerConfig {
  std::string vocab_filename = "";
  std::string unk_token = "[UNK]";
  int max_input_chars_per_word = 100;

  void Register(OptionsItf* opts) {
    opts->Register("vocab", &vocab_filename, "WordPiece vocabulary file.");
    opts->Register("unk-token", &unk_token,
                   "WordPiece token for unknown words.");
    opts->Register(
        "max-input-chars-per-word", &max_input_chars_per_word,
        "Max input characters per word for WordPiece word splitting. "
        "A word longer than this will be mapped to the unknown token");
  }
};

class WordPieceTokenizer {
 public:
  using WordToken = std::string;
  using WordPieceToken = std::string;
  using Id = std::size_t;

  WordPieceTokenizer(const WordPieceTokenizerConfig& opts);

  /**
   * Tokenize/split "full" tokens into word pieces
   */
  std::vector<WordPieceToken> Tokenize(
      const std::vector<WordToken>& words) const;
  std::vector<Id> TokenizeToIds(const std::vector<WordToken>& words) const;

  /**
   * Merge subwords into words
   */
  std::vector<WordToken> Merge(
      const std::vector<WordPieceToken>& word_pieces) const;

  /**
   * Convert between tokens and IDs and vice versa.
   *
   * @note These two do not handle OOVs in any way.
   */
  std::vector<WordPieceToken> IdsToTokens(const std::vector<Id>& ids) const;
  std::vector<Id> TokensToIds(const std::vector<WordPieceToken>& tokens) const;

 private:
  const std::string unk_token_;
  const int max_input_chars_per_word_;

  std::vector<WordPieceToken> vocab_;
  std::unordered_map<WordPieceToken, Id> vocab_map_;
};

}  // namespace tiro_speech::itn

#endif  // TIRO_SPEECH_SRC_ITN_WORDPIECE_H_
