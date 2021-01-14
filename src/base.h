// Copyright 2020 Tiro ehf.
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
#ifndef TIRO_SPEECH_SRC_BASE_H_
#define TIRO_SPEECH_SRC_BASE_H_

#include <filesystem>
#include <string_view>
#include <vector>

namespace tiro_speech {

constexpr int kDefaultSampleRate = 16000;
constexpr const char* kDefaultLanguageCode = "is-IS";

/**
 * Split a string by a single character, returning a vector of views. If \a c is
 * not found in the string the first element of the returned vector is equal to
 * \a to_split.
 *
 * Note that the string_view elements reference the original string.
 */
const std::vector<std::string_view> Split(const std::string_view to_split,
                                          const char c = ' ');

}  // namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_BASE_H_
