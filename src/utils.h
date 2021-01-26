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
#ifndef TIRO_SPEECH_SRC_UTILS_H_
#define TIRO_SPEECH_SRC_UTILS_H_

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <type_traits>

#include "src/base.h"

namespace google::protobuf {
class Timestamp;
}  // namespace google::protobuf

namespace tiro_speech {

inline float Linear16ToSeconds(const std::string& bytes, float sample_rate) {
  return (bytes.size() / 2) / sample_rate;
}

/**
 * Reads a WAVE file and removes the 44 byte header returning only the
 * "data" portion.
 *
 * This assumes a single channel linear PCM WAVE. No validation, watch
 * out!
 */
std::string ReadWaveFile(const std::string& filename);

std::string GetFileContents(const std::string& filename);

/**
 * Converts a google::protobuf::Timestamp into std::tm, throwing away nanosecond
 * precision.  Throws `runtime_error' on conversion failure.
 */
std::tm to_tm(const google::protobuf::Timestamp& timestamp);

template <typename Clock = std::chrono::system_clock>
std::tm to_tm(const std::chrono::time_point<Clock>& time_point);

struct no_copy_or_move {
  no_copy_or_move() = default;
  no_copy_or_move(const no_copy_or_move&) = delete;
  no_copy_or_move(no_copy_or_move&&) = delete;
  const no_copy_or_move& operator=(const no_copy_or_move&) = delete;
  no_copy_or_move&& operator=(no_copy_or_move&&) = delete;
};

std::string Join(const std::vector<std::string>& seq,
                 const std::string_view delim);

}  // namespace tiro_speech

#include "src/utils-inl.h"

#endif  // TIRO_SPEECH_SRC_UTILS_H_
