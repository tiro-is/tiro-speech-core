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
#include "src/api/validation.h"

#include <string>
using namespace std::string_literals;

#include <fmt/format.h>

#include "google/rpc/code.pb.h"
#include "google/rpc/error_details.pb.h"
#include "google/rpc/status.pb.h"
#include "src/audio/audio-source.h"
#include "src/logging.h"

namespace tiro_speech {

MessageValidationStatus Validate(
    const tiro::speech::v1alpha::RecognitionConfig& config,
    const KaldiModelMap* models) {
  using tiro::speech::v1alpha::RecognitionConfig;

  MessageValidationStatus errors;
  // Field 'encoding':
  switch (config.encoding()) {
    case RecognitionConfig::ENCODING_UNSPECIFIED:
      errors.emplace_back("encoding",
                          "The field 'encoding' must be specified.");
      break;
    case RecognitionConfig::LINEAR16:
      [[fallthrough]];
    case RecognitionConfig::MP3:
      [[fallthrough]];
    case RecognitionConfig::FLAC:
      break;
    default:
      errors.emplace_back("encoding", "Unsupported encoding specified.");
  }

  // Field 'language_code':
  if (!config.language_code().empty()) {
    if (models != nullptr &&
        models->count({config.language_code(), "generic"}) < 1) {
      errors.emplace_back(
          "language_code",
          fmt::format("Unsupported value '{}' for field 'language_code'.",
                      config.language_code()));
    }
  } else {
    errors.emplace_back("language_code", "Field 'language_code' is required. ");
  }

  auto in_range = [](auto rate, auto low, auto high) -> bool {
    return !(rate < low || rate > high);
  };

  // Field 'max_alternatives':
  if (!in_range(config.max_alternatives(), 0, 30)) {
    errors.emplace_back("max_alternatives",
                        "Valid values for field 'max_alternatives' are in "
                        "range [1;30]");
  }

  // Field 'speech_contexts':  Nothing to check
  // Field 'enable_word_time_offsets': Should always be supported
  // Field 'metadata': Nothing to check

  return errors;
}

MessageValidationStatus Validate(
    const tiro::speech::v1alpha::RecognizeRequest& request,
    const KaldiModelMap* models) {
  MessageValidationStatus errors;
  // Field 'config':
  if (request.has_config()) {
    auto config_errors = Validate(request.config(), models);
    if (!config_errors.empty()) {
      errors.emplace_back(
          "config", "Error validating field 'config'. See error details.");
      for (auto& err : config_errors) {
        errors.emplace_back(fmt::format("config.{}", err.first), err.second);
      }
    }
  } else {
    errors.emplace_back("config", "Field 'config' is missing.");
  }

  // Field 'audio':
  using tiro::speech::v1alpha::RecognitionAudio;
  if (request.has_audio()) {
    switch (request.audio().audio_source_case()) {
      case RecognitionAudio::AUDIO_SOURCE_NOT_SET:
        errors.emplace_back("audio", "Field 'audio' empty.");
        break;
      case RecognitionAudio::kUri:
        if (!IsUriSupported(request.audio().uri())) {
          errors.emplace_back("audio.uri", "Scheme missing or unsupported.");
        }
        break;
      case RecognitionAudio::kContent:
        // TODO(rkjaran): Validate content (against config).
        TIRO_SPEECH_DEBUG("Not validating audio source of type 'content'");
        break;
    }
  } else {
    errors.emplace_back("audio", "Field 'audio' is missing.");
  }

  // Validate required fields
  return errors;
}

MessageValidationStatus Validate(
    const tiro::speech::v1alpha::StreamingRecognizeRequest& request,
    bool first_request, const KaldiModelMap* models) {
  using tiro::speech::v1alpha::RecognitionConfig;
  using tiro::speech::v1alpha::StreamingRecognizeRequest;

  MessageValidationStatus errors;

  // Field oneof 'streaming_request':
  switch (request.streaming_request_case()) {
    case StreamingRecognizeRequest::STREAMING_REQUEST_NOT_SET:
      errors.emplace_back("oneof_streaming_request",
                          "Either 'streaming_config' or 'audio_content' needs "
                          "to be set.");
      break;
    case StreamingRecognizeRequest::kStreamingConfig:
      if (!first_request) {
        errors.emplace_back("streaming_config",
                            "Only the first message can contain the field"
                            " 'streaming_config'");
      } else {
        // Validate field 'streaming_config':

        // Field 'streaming_config.config'
        if (request.streaming_config().has_config()) {
          auto config_errors =
              Validate(request.streaming_config().config(), models);
          if (request.streaming_config().config().encoding() !=
              RecognitionConfig::LINEAR16) {
            errors.emplace_back("streaming_config.config.encoding",
                                "LINEAR16 currently only supported encoding for"
                                " StreamingRecognize");
          }
          if (!config_errors.empty()) {
            errors.emplace_back(
                "streaming_config.config",
                "Error validating field "
                "'streaming_config.config'. See error details.");
            for (const auto& err_pair : config_errors) {
              errors.emplace_back("streaming_config.config."s + err_pair.first,
                                  err_pair.second);
            }
          }
        } else {
          errors.emplace_back("streaming_config.config",
                              "Required field 'streaming_config.config' "
                              "missing");
        }

        // Field 'streaming_config.single_utterance': No checks needed.
        // Field 'streaming_config.interim_results': No checks needed.
      }
      break;
    case StreamingRecognizeRequest::kAudioContent:
      if (first_request) {
        errors.emplace_back("audio_content",
                            "First message can only contain the field "
                            "'streaming_config'");
      } else {
        // TODO(rkjaran): Validate audio_content according to
        //                streaming_config.config
      }
      break;
  }

  return errors;
}

grpc::Status ErrorVecToStatus(const MessageValidationStatus& errors) {
  if (!errors.empty()) {
    google::rpc::Status detailed_error;
    detailed_error.set_code(google::rpc::Code::INVALID_ARGUMENT);
    detailed_error.set_message("Invalid request. See details");
    auto* bad_request_detail = detailed_error.add_details();
    google::rpc::BadRequest bad_request;
    for (const auto& err : errors) {
      auto* field_violation = bad_request.add_field_violations();
      field_violation->set_field(err.first);
      field_violation->set_description(err.second);
    }
    bad_request_detail->PackFrom(bad_request, "types.talgreinir.is");
    return grpc::Status{grpc::StatusCode::INVALID_ARGUMENT,
                        detailed_error.message(),
                        detailed_error.SerializeAsString()};
  }
  return grpc::Status::OK;
}

}  // namespace tiro_speech
