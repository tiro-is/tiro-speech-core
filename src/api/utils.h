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
#ifndef TIRO_SPEECH_SRC_API_UTILS_H_
#define TIRO_SPEECH_SRC_API_UTILS_H_

#include <grpcpp/support/string_ref.h>

#include <string>

std::string to_string(const grpc::string_ref& ref) {
  return {ref.begin(), ref.end()};

}  // namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_API_UTILS_H_
