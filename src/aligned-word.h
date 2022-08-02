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
#ifndef TIRO_SPEECH_SRC_ALIGNED_WORD_H_
#define TIRO_SPEECH_SRC_ALIGNED_WORD_H_

#include <chrono>
#include <cstdint>
#include <ostream>
#include <string>

namespace tiro_speech {

struct AlignedWord {
  std::int64_t start_time;
  std::int64_t duration;
  std::string word_symbol;

  AlignedWord(std::chrono::milliseconds start_time,
              std::chrono::milliseconds duration, std::string word_symbol)
      : start_time{start_time.count()},
        duration{duration.count()},
        word_symbol{word_symbol} {};

  AlignedWord() : start_time{0}, duration{0}, word_symbol{""} {}

  bool operator!=(const AlignedWord& other) const {
    return !(start_time == other.start_time && duration == other.duration &&
             word_symbol == other.word_symbol);
  }

  friend std::ostream& operator<<(std::ostream& os, const AlignedWord& ali) {
    os << "WordAlignment{" << ali.word_symbol << ", " << ali.start_time << ", "
       << ali.duration << "}";
    return os;
  }
};

}  // namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_ALIGNED_WORD_H_
