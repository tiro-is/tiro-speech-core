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
#include "src/base.h"

#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif
#include <cerrno>
#include <string_view>
#include <system_error>

namespace tiro_speech {

const std::vector<std::string_view> Split(const std::string_view to_split,
                                          const char c) {
  std::vector<std::string_view> views;
  for (size_t pos = 0;;) {
    size_t new_pos = to_split.find(c, pos);
    if (new_pos != std::string_view::npos) {
      views.push_back(to_split.substr(pos, new_pos - pos));
    } else {
      views.push_back(to_split.substr(pos));
      break;
    }
    pos = new_pos + 1;
  }
  return views;
}

}  // namespace tiro_speech
