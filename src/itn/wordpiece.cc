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
#include "src/itn/wordpiece.h"

#include <fmt/format.h>

#include <fstream>
#include <string>
#include <string_view>

#include "src/base.h"

namespace tiro_speech::itn {

WordPieceTokenizer::WordPieceTokenizer(const WordPieceTokenizerConfig& opts)
    : unk_token_{opts.unk_token},
      max_input_chars_per_word_{opts.max_input_chars_per_word} {
  std::ifstream vocab_file{opts.vocab_filename};
  if (!vocab_file.is_open()) {
    throw std::runtime_error{
        fmt::format("Could't open file '{}'", opts.vocab_filename)};
  }
  for (std::string line; std::getline(vocab_file, line);) {
    vocab_.push_back(line);
    const Id id = vocab_.size() - 1;
    vocab_map_[std::move(line)] = id;
  }
}

std::vector<WordPieceTokenizer::WordPieceToken> WordPieceTokenizer::Tokenize(
    const std::vector<WordToken>& words) const {
  std::vector<WordPieceToken> word_pieces{};
  for (const auto& token : words) {
    if (static_cast<int>(token.size()) > max_input_chars_per_word_) {
      word_pieces.push_back(unk_token_);
      continue;
    }

    bool is_bad = false;
    std::size_t start_char = 0;
    std::vector<WordPieceToken> sub_tokens{};
    while (start_char < token.size()) {
      std::size_t end_char = token.size();
      std::string current_substr = "";
      while (start_char < end_char) {
        std::string substr = token.substr(start_char, end_char - start_char);
        if (start_char > 0) {
          substr = "##" + substr;
        }
        if (vocab_map_.count(substr) == 1) {
          current_substr = substr;
          break;
        }
        end_char -= 1;
      }
      if (current_substr.empty()) {
        is_bad = true;
        break;
      }
      sub_tokens.push_back(current_substr);
      start_char = end_char;
    }

    if (is_bad) {
      word_pieces.push_back(unk_token_);
    } else {
      for (const auto& piece : sub_tokens) {
        word_pieces.push_back(piece);
      }
    }
  }

  return word_pieces;
}

std::vector<WordPieceTokenizer::WordToken> WordPieceTokenizer::Merge(
    const std::vector<WordPieceTokenizer::WordPieceToken>& word_pieces) const {
  std::vector<WordToken> words{};
  for (const std::string_view piece : word_pieces) {
    if (piece.size() > 2 && piece.substr(0, 2) == "##") {
      if (words.size() == 0)
        throw std::invalid_argument{
            "First WordPiece element can't be a suffix"};
      words.back().append(piece.substr(2));
    } else {
      words.emplace_back(piece);
    }
  }
  return words;
}

std::vector<WordPieceTokenizer::Id> WordPieceTokenizer::TokenizeToIds(
    const std::vector<WordToken>& words) const {
  // TODO(rkjaran): This will probably be used more frequently than Tokenize so
  //                we should implement it the other way around if performance
  //                is a problem.
  return TokensToIds(Tokenize(words));
}

std::vector<WordPieceTokenizer::WordPieceToken> WordPieceTokenizer::IdsToTokens(
    const std::vector<Id>& ids) const {
  std::vector<WordPieceToken> tokens{};
  for (const auto& id : ids) {
    tokens.push_back(vocab_.at(id));
  }
  return tokens;
}

std::vector<WordPieceTokenizer::Id> WordPieceTokenizer::TokensToIds(
    const std::vector<WordPieceTokenizer::WordPieceToken>& tokens) const {
  std::vector<Id> ids{};
  for (const auto& token : tokens) {
    ids.push_back(vocab_map_.at(token));
  }
  return ids;
}

}  // namespace tiro_speech::itn
