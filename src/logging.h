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
#ifndef TIRO_SPEECH_SRC_LOGGING_H_
#define TIRO_SPEECH_SRC_LOGGING_H_

#if defined(NDEBUG)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <map>
#include <string>

namespace tiro_speech::logging {

constexpr char kDefaultLoggerName[] = "tiro-talgreinir";

enum class LogLevel { kTrace, kDebug, kInfo, kWarning, kError, kCritical };

LogLevel ConvertLogLevel(spdlog::level::level_enum spdlevel);

spdlog::level::level_enum ConvertLogLevel(LogLevel log_level);

void SetLogLevel(LogLevel log_level);
void SetLogLevel(const std::string& log_level);

LogLevel LogLevelFromString(const std::string& log_level);

spdlog::logger* GetDefaultLogger();

}  // namespace tiro_speech::logging

#define TIRO_SPEECH_WARN(...) \
  SPDLOG_LOGGER_WARN(::tiro_speech::logging::GetDefaultLogger(), __VA_ARGS__)
#define TIRO_SPEECH_INFO(...) \
  SPDLOG_LOGGER_INFO(::tiro_speech::logging::GetDefaultLogger(), __VA_ARGS__)
#define TIRO_SPEECH_ERROR(...) \
  SPDLOG_LOGGER_ERROR(::tiro_speech::logging::GetDefaultLogger(), __VA_ARGS__)
#define TIRO_SPEECH_FATAL(...)                                       \
  SPDLOG_LOGGER_CRITICAL(::tiro_speech::logging::GetDefaultLogger(), \
                         __VA_ARGS__);                               \
  abort();
#define TIRO_SPEECH_DEBUG(...) \
  SPDLOG_LOGGER_DEBUG(::tiro_speech::logging::GetDefaultLogger(), __VA_ARGS__)

template <typename K, typename V>
std::ostream& operator<<(std::ostream& os, const std::multimap<K, V>& m) {
  os << "{";
  for (const auto& kv : m) {
    os << '"' << kv.first << "\": " << '"' << kv.second << "\",";
  }
  return os << "}";
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
  os << "[ ";
  for (const auto& elem : v) {
    os << elem << " ";
  }
  return os << "]";
}

#endif  // TIRO_SPEECH_SRC_LOGGING_H_
