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
#include "src/api/services.h"

#include <base/kaldi-error.h>
#include <fmt/format.h>
#include <fstext/fstext-lib.h>
#include <lat/lattice-functions.h>
#include <online2/online-nnet2-feature-pipeline.h>
#include <online2/online-nnet3-decoding.h>
#include <online2/onlinebin-util.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include "google/rpc/code.pb.h"
#include "google/rpc/error_details.pb.h"
#include "google/rpc/status.pb.h"
#include "src/api/utils.h"
#include "src/api/validation.h"
#include "src/audio/audio-source.h"
#include "src/audio/audio.h"
#include "src/audio/ffmpeg-wrapper.h"
#include "src/base.h"
#include "src/logging.h"
#include "src/proto/tiro/speech/v1alpha/speech.grpc.pb.h"
#include "src/recognizer.h"
#include "src/utils.h"
#include "src/vad.h"

// Declarations local to this TU
namespace {

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;

void Convert(const tiro_speech::AlignedWord& aligned_word,
             tiro::speech::v1alpha::WordInfo* word_info) {
  word_info->set_word(aligned_word.word_symbol);

  milliseconds start_time{aligned_word.start_time};
  word_info->mutable_start_time()->set_seconds(
      duration_cast<seconds>(start_time).count());
  word_info->mutable_start_time()->set_nanos(
      duration_cast<nanoseconds>(start_time % seconds{1}).count());

  milliseconds end_time{aligned_word.start_time + aligned_word.duration};
  word_info->mutable_end_time()->set_seconds(
      duration_cast<seconds>(end_time).count());
  word_info->mutable_end_time()->set_nanos(
      duration_cast<nanoseconds>(end_time % seconds{1}).count());
}

tiro_speech::AudioEncoding Convert(
    const tiro::speech::v1alpha::RecognitionConfig::AudioEncoding& external) {
  using tiro::speech::v1alpha::RecognitionConfig;
  switch (external) {
    case RecognitionConfig::ENCODING_UNSPECIFIED:
      return tiro_speech::AudioEncoding::ENCODING_UNSPECIFIED;
    case RecognitionConfig::FLAC:
      return tiro_speech::AudioEncoding::FLAC;
    case RecognitionConfig::LINEAR16:
      return tiro_speech::AudioEncoding::LINEAR16;
    case RecognitionConfig::MP3:
      return tiro_speech::AudioEncoding::MP3;
    default:
      throw std::runtime_error{"Unsupported encoding."};
  }
}

}  // namespace

namespace tiro_speech {

grpc::Status SpeechService::Recognize(grpc::ServerContext* context,
                                      const RecognizeRequest* request,
                                      RecognizeResponse* response) {
  using tiro::speech::v1alpha::RecognitionAudio;
  try {
    grpc::Status stat = ErrorVecToStatus(Validate(*request, &models_));
    if (!stat.ok()) {
      return stat;
    }

    std::shared_ptr<const KaldiModel> model =
        models_.at({request->config().language_code(), "generic"});
    Recognizer utt_recognizer{*model};

    std::unique_ptr<AudioSourceItf> audio_src{nullptr};
    try {
      switch (request->audio().audio_source_case()) {
        case RecognitionAudio::kContent: {
          audio_src = std::make_unique<ContentAudioSource>(
              AudioSourceInfo{AudioSourceInfo::Type::kContent,
                              Convert(request->config().encoding()),
                              request->config().sample_rate_hertz()},
              request->audio().content(),
              request->config().sample_rate_hertz());
          break;
        }
        case RecognitionAudio::kUri: {
          audio_src = CreateAudioSourceFromUri(
              AudioSourceInfo{AudioSourceInfo::Type::kUri,
                              Convert(request->config().encoding()),
                              request->config().sample_rate_hertz()},
              request->audio().uri());
          break;
        }
        default:
          TIRO_SPEECH_ERROR("This shouldn't happen");
          throw std::logic_error{""};
      }
    } catch (const AudioSourceError& err) {
      audio_src = nullptr;
    }

    if (audio_src == nullptr) {
      return grpc::Status{grpc::StatusCode::INVALID_ARGUMENT,
                          "Audio decoding error. Possibly in wrong format?"};
    }

    audio_src->Open();

    while (audio_src->HasMoreChunks()) {
      auto chunk = audio_src->NextChunk();
      if (chunk.Dim() > 0) utt_recognizer.Decode(chunk);
    }
    std::vector<AlignedWord> first_alignments;
    std::vector<std::string> transcripts;
    const int max_alternatives = request->config().max_alternatives() == 0
                                     ? 1
                                     : request->config().max_alternatives();
    utt_recognizer.Finalize();
    if (!utt_recognizer.GetResults(max_alternatives, &first_alignments,
                                   &transcripts)) {
      TIRO_SPEECH_WARN("Could not get transcripts.");
    }

    if (transcripts.empty()) {
      TIRO_SPEECH_ERROR("Couldn't generate transcripts.");
      return grpc::Status{grpc::StatusCode::INTERNAL,
                          "Something bad happened."};
    }

    using tiro::speech::v1alpha::SpeechRecognitionAlternative;
    using tiro::speech::v1alpha::SpeechRecognitionResult;

    SpeechRecognitionResult* res = response->add_results();
    if (request->config().enable_word_time_offsets()) {
      if (first_alignments.empty()) {
        TIRO_SPEECH_WARN("Couldn't get word alignment.");
        return grpc::Status{grpc::StatusCode::INTERNAL,
                            "Something bad happened."};
      }
      SpeechRecognitionAlternative* first_alternative = res->add_alternatives();
      for (const auto& ali : first_alignments) {
        first_alternative->set_transcript(transcripts[0]);
        Convert(ali, first_alternative->add_words());
      }
    }

    for (auto it = transcripts.cbegin() +
                   (request->config().enable_word_time_offsets() ? 1 : 0);
         it != transcripts.end(); ++it) {
      SpeechRecognitionAlternative* alt = res->add_alternatives();
      alt->set_transcript(*it);
    }

    return grpc::Status::OK;
  } catch (const AudioSourceError& ex) {
    TIRO_SPEECH_DEBUG("Caught AudioSourceError");
    return grpc::Status{
        grpc::StatusCode::FAILED_PRECONDITION,
        "Audio stream not decodable. Possibly unavailable or in unsupported "
        "format."};
  } catch (const std::exception& ex) {
    TIRO_SPEECH_WARN("Unhandled exception: {}", ex.what());
    return grpc::Status{grpc::StatusCode::INTERNAL,
                        "Unknown error in Recognize."};
  }
}

void SpeechService::RegisterModel(
    ModelId model_id, const std::shared_ptr<const KaldiModel>& model) {
  models_[std::move(model_id)] = model;
}

}  // end namespace tiro_speech
