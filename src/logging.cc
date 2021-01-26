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
#include "src/logging.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <base/kaldi-error.h>
#include <grpc/support/log.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

namespace tiro_speech::logging {

namespace {

static std::shared_ptr<spdlog::logger> g_logger = []() {
  auto logger = std::make_shared<spdlog::logger>(
      kDefaultLoggerName,
      std::make_shared<spdlog::sinks::ansicolor_stderr_sink_mt>());
  logger->set_pattern("%L %P:%t %Y-%m-%dT%T.%f  [%s:%#:%!()]  %v");
  return logger;
}();

void RegisterKaldiLogHandler() {
  kaldi::SetLogHandler([](const kaldi::LogMessageEnvelope& envelope,
                          const char* message) {
    switch (envelope.severity) {
      case kaldi::LogMessageEnvelope::kAssertFailed:
        GetDefaultLogger()->critical("kaldi:{}: {}", envelope.func, message);
        break;
      case kaldi::LogMessageEnvelope::kError:
        GetDefaultLogger()->error("kaldi:{}: {}", envelope.func, message);
        break;
      case kaldi::LogMessageEnvelope::kWarning:
        GetDefaultLogger()->warn("kaldi:{}: {}", envelope.func, message);
        break;
      case kaldi::LogMessageEnvelope::kInfo:
        GetDefaultLogger()->info("kaldi:{}: {}", envelope.func, message);
        break;
      default:  // severity > 0 == debug
        if (envelope.severity < 1) {
          GetDefaultLogger()->debug("kaldi:V{}:{}: {}", envelope.severity,
                                    envelope.func, message);
        } else {
          GetDefaultLogger()->trace("kaldi:V{}:{}: {}", envelope.severity,
                                    envelope.func, message);
        }
        break;
    }
  });
}

void RegisterGrpcLogHandler() {
  gpr_set_log_function([](gpr_log_func_args* args) -> void {
    switch (args->severity) {
      case ::GPR_LOG_SEVERITY_DEBUG:
        GetDefaultLogger()->debug("grpc:{} ", args->message);
        break;
      case ::GPR_LOG_SEVERITY_INFO:
        GetDefaultLogger()->info("grpc:{} ", args->message);
        break;
      case ::GPR_LOG_SEVERITY_ERROR:
        GetDefaultLogger()->error("grpc:{} ", args->message);
        break;
    }
  });
}

void SetKaldiLogLevel(LogLevel log_level) {
  switch (log_level) {
    case LogLevel::kTrace:
      kaldi::SetVerboseLevel(2);
      break;
    case LogLevel::kDebug:
      kaldi::SetVerboseLevel(1);
      break;
    default:
      kaldi::SetVerboseLevel(0);
      break;
  }
}

void SetGrpcLogLevel(LogLevel log_level) {
  switch (log_level) {
    case LogLevel::kTrace:
      [[fallthrough]];
    case LogLevel::kDebug:
      gpr_set_log_verbosity(::GPR_LOG_SEVERITY_DEBUG);
      break;
    case LogLevel::kError:
      gpr_set_log_verbosity(::GPR_LOG_SEVERITY_ERROR);
      break;
    default:
      gpr_set_log_verbosity(::GPR_LOG_SEVERITY_INFO);
      break;
  }
}

void RegisterAvLogHandler() {
  av_log_set_callback(
      [](void* avcl, int level, const char* fmt, va_list vl) -> void {
        static int prefix = 1;
        char line[512];
        av_log_format_line(avcl, level, fmt, vl, line, 512, &prefix);

        switch (level) {
          case AV_LOG_TRACE:
            GetDefaultLogger()->trace("av: {}", line);
            break;
          case AV_LOG_DEBUG:
            [[fallthrough]];
          case AV_LOG_WARNING:
            [[fallthrough]];
          case AV_LOG_INFO:
            GetDefaultLogger()->debug("av: {}", line);
            break;
          case AV_LOG_ERROR:
            GetDefaultLogger()->error("av: {}", line);
            break;
        }
      });
}

void SetAvLogLevel(LogLevel log_level) {
  switch (log_level) {
    case LogLevel::kTrace:
      av_log_set_level(AV_LOG_TRACE);
      break;
    case LogLevel::kDebug:
      av_log_set_level(AV_LOG_DEBUG);
      break;
    case LogLevel::kInfo:
      av_log_set_level(AV_LOG_INFO);
      break;
    case LogLevel::kWarning:
      av_log_set_level(AV_LOG_WARNING);
      break;
    case LogLevel::kError:
      [[fallthrough]];
    case LogLevel::kCritical:
      av_log_set_level(AV_LOG_ERROR);
      break;
    default:
      av_log_set_level(AV_LOG_INFO);
      break;
  }
}

}  // namespace

LogLevel ConvertLogLevel(spdlog::level::level_enum spdlevel) {
  switch (spdlevel) {
    case spdlog::level::trace:
      return LogLevel::kTrace;
    case spdlog::level::debug:
      return LogLevel::kDebug;
    case spdlog::level::info:
      return LogLevel::kInfo;
    case spdlog::level::warn:
      return LogLevel::kWarning;
    case spdlog::level::err:
      return LogLevel::kError;
    case spdlog::level::critical:
      return LogLevel::kCritical;
    default:
      return LogLevel::kInfo;
  }
}

spdlog::level::level_enum ConvertLogLevel(LogLevel log_level) {
  switch (log_level) {
    case LogLevel::kTrace:
      return spdlog::level::trace;
    case LogLevel::kDebug:
      return spdlog::level::debug;
    case LogLevel::kInfo:
      return spdlog::level::info;
    case LogLevel::kWarning:
      return spdlog::level::warn;
    case LogLevel::kError:
      return spdlog::level::err;
    case LogLevel::kCritical:
      return spdlog::level::critical;
    default:
      return spdlog::level::info;
  }
}

void SetLogLevel(LogLevel log_level) {
  RegisterKaldiLogHandler();
  SetKaldiLogLevel(log_level);

  RegisterGrpcLogHandler();
  SetGrpcLogLevel(log_level);

  RegisterAvLogHandler();
  SetAvLogLevel(log_level);

  GetDefaultLogger()->set_level(ConvertLogLevel(log_level));
}

void SetLogLevel(const std::string& log_level) {
  SetLogLevel(LogLevelFromString(log_level));
}

LogLevel LogLevelFromString(const std::string& log_level) {
  if (log_level == "TRACE" || log_level == "trace") return LogLevel::kTrace;
  if (log_level == "DEBUG" || log_level == "debug") return LogLevel::kDebug;
  if (log_level == "INFO" || log_level == "info") return LogLevel::kInfo;
  if (log_level == "WARNING" || log_level == "warning")
    return LogLevel::kWarning;
  if (log_level == "ERROR" || log_level == "error") return LogLevel::kError;

  throw std::runtime_error{"Invalid log level: " + log_level};
}

spdlog::logger* GetDefaultLogger() { return g_logger.get(); }

}  // namespace tiro_speech::logging
