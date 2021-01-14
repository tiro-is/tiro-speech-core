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
#include "src/utils.h"

#include <google/protobuf/timestamp.pb.h>

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tiro_speech {

using namespace std::string_literals;

std::string ReadWaveFile(const std::string& filename) {
  std::ifstream is{filename, std::ios::binary | std::ios::ate};
  if (!is.is_open()) {
    throw std::runtime_error{"Couldn't open file."};
  }
  is.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  std::size_t size = static_cast<std::size_t>(is.tellg()) - 44;
  std::string bytes(size, '\0');
  is.seekg(44);
  is.read(&bytes[0], size);

  return bytes;
}

std::string GetFileContents(const std::string& filename) {
  std::string contents;
  std::ifstream is{filename, std::ios::ate};
  is.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  auto size = is.tellg();
  is.seekg(0);
  contents.resize(size, '\0');
  is.read(&contents[0], size);
  return contents;
}

std::tm to_tm(const google::protobuf::Timestamp& timestamp) {
  std::time_t epoch_seconds = static_cast<std::time_t>(timestamp.seconds());
  std::tm* converted_timestamp = std::gmtime(&epoch_seconds);
  if (converted_timestamp == nullptr) {
    throw std::runtime_error{
        "Could not convert google::protobuf::Timestamp to std::tm"};
  }
  return *converted_timestamp;
}

std::string Join(const std::vector<std::string>& seq,
                 const std::string_view delim) {
  std::string ret{};
  for (auto it = seq.cbegin(); it != seq.cend(); ++it) {
    ret += *it;
    if (it != seq.cend() - 1) ret += delim;
  }
  return ret;
}

}  // end namespace tiro_speech
