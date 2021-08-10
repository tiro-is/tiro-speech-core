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
#include "src/api/services.h"

#include <base/kaldi-error.h>
#include <fmt/format.h>
#include <fstext/fstext-lib.h>
#include <lat/lattice-functions.h>
#include <online2/online-nnet2-feature-pipeline.h>
#include <online2/online-nnet3-decoding.h>
#include <online2/onlinebin-util.h>
#include <readerwriterqueue.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include "google/rpc/code.pb.h"
#include "google/rpc/error_details.pb.h"
#include "google/rpc/status.pb.h"
#include "proto/tiro/speech/v1alpha/speech.grpc.pb.h"
#include "src/api/utils.h"
#include "src/api/validation.h"
#include "src/audio/audio-source.h"
#include "src/audio/audio.h"
#include "src/audio/ffmpeg-wrapper.h"
#include "src/base.h"
#include "src/logging.h"
#include "src/recognizer.h"
#include "src/utils.h"
#include "src/vad.h"

// Declarations local to this TU
namespace {

using namespace std::chrono_literals;
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
      if (chunk.Dim() > 0) {
        utt_recognizer.Decode(chunk, /* flush */ !audio_src->HasMoreChunks());
      }
    }
    utt_recognizer.Finalize();

    std::vector<AlignedWord> first_alignments;
    std::vector<std::string> transcripts;
    const int max_alternatives = request->config().max_alternatives() == 0
                                     ? 1
                                     : request->config().max_alternatives();
    if (!utt_recognizer.GetResults(
            max_alternatives, &first_alignments, &transcripts,
            /* end_of_utt */ true,
            /* punctuate */ request->config().enable_automatic_punctuation())) {
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

namespace {
/// Processors for StreamingRecognize

const char kEndOfStream[] = "";

grpc::Status RunStreamingReader(
    grpc::ServerReaderWriter<tiro::speech::v1alpha::StreamingRecognizeResponse,
                             tiro::speech::v1alpha::StreamingRecognizeRequest>*
        stream,
    moodycamel::BlockingReaderWriterQueue<std::string>& chunks_buf,
    std::atomic<bool>& processor_cancelled) {
  auto retry_enqueue = [&chunks_buf, &processor_cancelled](
                           auto&& chunk, int max_attempts = 50) {
    for (int attempts = 1; !chunks_buf.try_enqueue(chunk) &&
                           attempts <= max_attempts && !processor_cancelled;
         attempts++) {
      std::this_thread::sleep_for(200ms * attempts);
      TIRO_SPEECH_DEBUG(
          "RunStreamingReader: retrying chunk enqueue operation, attempt {}.",
          attempts);
    }
  };

  tiro::speech::v1alpha::StreamingRecognizeRequest req;
  while (stream->Read(&req)) {
    grpc::Status stat = ErrorVecToStatus(Validate(req));
    if (!stat.ok()) {
      return stat;
    }
    if (req.audio_content().empty() || req.audio_content() == "END") {
      // Needed for websocket proxy, empty audio_content instead of
      // WritesDone
      break;
    }
    // If we can't enqueue because the queue is out of memory we will retry
    retry_enqueue(std::move(*req.mutable_audio_content()));
    if (processor_cancelled) {
      return grpc::Status::CANCELLED;
    }
  }

  chunks_buf.enqueue(kEndOfStream);
  return grpc::Status::OK;
}

class ChunkConverter {
 public:
  ChunkConverter(int sample_rate, int model_sample_rate)
      : sample_rate_{sample_rate},
        model_sample_rate_{model_sample_rate},
        lowpass_cutoff_{0.99f * 0.5f *
                        std::min(model_sample_rate, sample_rate)},
        lowpass_filter_width_{6},
        resampler_{sample_rate_, model_sample_rate_, lowpass_cutoff_,
                   lowpass_filter_width_} {}

  Vector ToWaveform(const std::string& chunk, bool more_data_available) {
    Vector waveform;
    Linear16BytesToWaveVector(chunk, &waveform);
    return waveform;

    if (sample_rate_ != model_sample_rate_) {
      Vector waveform_resampled;
      resampler_.Resample(waveform, !more_data_available, &waveform_resampled);
      return waveform_resampled;
    }

    return waveform;
  }

  // const int sample_rate = streaming_config.config().sample_rate_hertz();
  // const int model_sample_rate = GetSampleRate(recognizer_model.feature_info);

 private:
  int sample_rate_;
  int model_sample_rate_;
  float lowpass_cutoff_;
  int lowpass_filter_width_;
  kaldi::LinearResample resampler_;
};

grpc::Status RunStreamingProcessor(
    grpc::ServerReaderWriter<tiro::speech::v1alpha::StreamingRecognizeResponse,
                             tiro::speech::v1alpha::StreamingRecognizeRequest>*
        stream,
    moodycamel::BlockingReaderWriterQueue<std::string>& chunks_buf,
    const tiro::speech::v1alpha::StreamingRecognitionConfig& streaming_config,
    const KaldiModel& recognizer_model) {
  using tiro::speech::v1alpha::StreamingRecognitionResult;
  using tiro::speech::v1alpha::StreamingRecognizeResponse;

  kaldi::OnlineIvectorExtractorAdaptationState adaptation_state{
      recognizer_model.initial_adaptation_state};

  const int sample_rate = streaming_config.config().sample_rate_hertz();
  const int model_sample_rate = GetSampleRate(recognizer_model.feature_info);
  ChunkConverter chunk_converter{sample_rate, model_sample_rate};

  int result_index = -1;
  bool more_data = true;
  bool end_of_single_utterance = false;
  std::vector<AlignedWord> left_context{};

  auto add_results = [&](Recognizer& recognizer, bool is_final, int offset,
                         StreamingRecognizeResponse& res) -> grpc::Status {
    std::vector<AlignedWord> best_aligned;
    std::vector<std::string> transcripts;
    int max_alternatives = streaming_config.config().max_alternatives() == 0
                               ? 1
                               : streaming_config.config().max_alternatives();

    if (recognizer.GetResults(
            is_final ? max_alternatives : 1, &best_aligned, &transcripts,
            /* end_of_utt */ is_final,
            /* punctuate */
            streaming_config.config().enable_automatic_punctuation())) {
      StreamingRecognitionResult result{};
      result.set_is_final(is_final);

      if (transcripts.empty()) {
        return grpc::Status{grpc::StatusCode::INTERNAL, "Unexpected failure."};
      }
      if (streaming_config.config().enable_word_time_offsets()) {
        if (best_aligned.empty()) {
          TIRO_SPEECH_DEBUG(
              "Best aligned is empty. Emitting empty transcript.");
        }
        auto* alt = result.add_alternatives();
        alt->set_transcript(transcripts[0]);

        for (auto& ali : best_aligned) {
          ali.start_time += offset;
          Convert(ali, alt->add_words());
        }
      }
      for (auto it =
               std::cbegin(transcripts) +
               (streaming_config.config().enable_word_time_offsets() ? 1 : 0);
           it != std::cend(transcripts); ++it) {
        result.add_alternatives()->set_transcript(*it);
      }
      res.add_results()->CopyFrom(result);
    }
    return grpc::Status::OK;
  };

  milliseconds processed_time{0};

  while (result_index++, more_data && !end_of_single_utterance) {
    Recognizer recognizer{recognizer_model, adaptation_state, left_context};

    milliseconds segment_time{0};
    auto last_interim_result_time = std::chrono::system_clock::now();

    std::string last_transcript = "";
    bool endpoint_detected = false;

    Vad vad{kDefaultSampleRate, 30};
    milliseconds vad_offset{0};
    bool speech_started = false;

    while (!endpoint_detected) {
      std::string chunk;
      chunks_buf.wait_dequeue(chunk);
      more_data = (chunk != kEndOfStream);
      if (!more_data) {
        TIRO_SPEECH_DEBUG("No more data in queue.");
        break;
      }
      const int n_samples = chunk.size() / 2;  //< Assuming 16 bit samples.
      const milliseconds chunk_time =
          milliseconds{1000 * n_samples / sample_rate};
      segment_time += chunk_time;

      Vector waveform = chunk_converter.ToWaveform(chunk, more_data);

      if (!speech_started) {
        if ((speech_started = vad.HasSpeech(waveform))) {
          TIRO_SPEECH_DEBUG("Skipped {} ms of audio because of VAD",
                            vad_offset.count());
          // TODO(rkjaran): pad start of next segment with silence... correctly
          // Vector padded_waveform{waveform.Dim() + 2048};
          // padded_waveform.Range(2048, waveform.Dim()).CopyFromVec(waveform);
          // waveform.Resize(padded_waveform.Dim());
          // waveform.CopyFromVec(padded_waveform);
        } else {
          vad_offset += chunk_time;
        }
      }

      if (speech_started) {
        recognizer.Decode(waveform, !more_data);
        endpoint_detected = recognizer.HasEndpoint();

        if (streaming_config.interim_results() && !endpoint_detected) {
          auto now = std::chrono::system_clock::now();
          if (recognizer.NumFramesDecoded() > 0 &&
              now - last_interim_result_time > 350ms) {
            // TODO(rkjaran): use GetResults and send a time aligned response
            StreamingRecognizeResponse res;

            // XXX: the inefficient way of doing this... We should be checking
            //      the decoder traceback
            // TODO(rkjaran): Cleanup the Recognizer interface so that we can
            //                efficiently check for updates in the traceback.
            std::string cur_transcript = recognizer.GetBestHypothesis(false);
            if (cur_transcript != last_transcript) {
              last_transcript = std::move(cur_transcript);
              const auto offset =
                  duration_cast<milliseconds>(vad_offset + processed_time)
                      .count();
              grpc::Status status =
                  add_results(recognizer, /* is_final */ false, offset, res);
              if (!status.ok()) {
                return status;
              }
              if (res.results_size() > 0) {
                if (!stream->Write(res)) {
                  TIRO_SPEECH_DEBUG(
                      "Write failed. Client may have killed the connection");
                  return grpc::Status::CANCELLED;
                }
              }
              last_interim_result_time = now;
            }
          }
        }
      }
    }

    recognizer.Finalize();
    // TODO(rkjaran): only update adaptation_state when we decoded something and
    //                only if not in multispeaker mode.
    adaptation_state = recognizer.GetAdaptationState();

    if (recognizer.NumFramesDecoded() > 0) {
      StreamingRecognizeResponse res;

      const auto offset =
          duration_cast<milliseconds>(vad_offset + processed_time).count();

      if (grpc::Status status =
              add_results(recognizer, /* is_final */ true, offset, res);
          !status.ok()) {
        return status;
      }

      TIRO_SPEECH_DEBUG("Left context is now {}", recognizer.GetLeftContext());
      left_context = recognizer.GetLeftContext();

      if (!stream->Write(res)) {
        return grpc::Status::CANCELLED;
      }

      if (streaming_config.single_utterance()) {
        TIRO_SPEECH_DEBUG(
            "Asked for single utterance... ending recognition after "
            "first endpoint");
        end_of_single_utterance = true;

        StreamingRecognizeResponse res;
        res.set_speech_event_type(
            StreamingRecognizeResponse::END_OF_SINGLE_UTTERANCE);
        if (!stream->Write(res)) {
          return grpc::Status::CANCELLED;
        }
      }
    }

    processed_time += segment_time;
  }

  return grpc::Status::OK;
}

}  // namespace

grpc::Status SpeechService::StreamingRecognize(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<StreamingRecognizeResponse,
                             StreamingRecognizeRequest>* stream) {
  using tiro::speech::v1alpha::StreamingRecognitionConfig;
  try {
    moodycamel::BlockingReaderWriterQueue<std::string> chunks_buf;

    // Read config from first request (or return error)
    StreamingRecognizeRequest req;
    if (!stream->Read(&req)) {
      return grpc::Status::CANCELLED;
    }
    TIRO_SPEECH_DEBUG("Got StreamingRecognizeRequest: {}",
                      req.ShortDebugString());

    grpc::Status stat =
        ErrorVecToStatus(Validate(req, /* first_request */ true, &models_));
    if (!stat.ok()) {
      TIRO_SPEECH_DEBUG(
          "Error while validating first StreamingRecognizeRequest: details "
          "\"{}\"\"",
          stat.error_details());
      return stat;
    }

    // Keep a local copy of streaming_config
    StreamingRecognitionConfig streaming_config;
    streaming_config.CopyFrom(req.streaming_config());

    // We have to signal to the reader thread that the processing has been
    // cancelled
    std::atomic<bool> processor_cancelled{false};

    auto reader_task = std::packaged_task<grpc::Status()>{
        [stream, &chunks_buf, &processor_cancelled]() {
          return RunStreamingReader(stream, chunks_buf, processor_cancelled);
        }};
    std::future<grpc::Status> reader_status_fut = reader_task.get_future();
    std::thread reader_thread{std::move(reader_task)};

    auto processor_task = std::packaged_task<grpc::Status()>{
        [this, stream, &chunks_buf, &streaming_config]() {
          return RunStreamingProcessor(
              stream, chunks_buf, streaming_config,
              *models_.at(
                  {streaming_config.config().language_code(), "generic"}));
        }};

    std::future<grpc::Status> processor_status_fut =
        processor_task.get_future();
    std::thread processor_thread{std::move(processor_task)};

    grpc::Status ret_status = grpc::Status::OK;

    while (true) {
      if (!(reader_status_fut.valid() || processor_status_fut.valid())) {
        break;
      }

      if (reader_status_fut.valid()) {
        auto reader_ready = reader_status_fut.wait_for(100ms);
        if (reader_ready == std::future_status::ready) {
          grpc::Status _ret_status = reader_status_fut.get();
          if (!_ret_status.ok()) {
            ret_status = _ret_status;
            break;
          }
        }
      }

      if (processor_status_fut.valid()) {
        auto processor_ready = processor_status_fut.wait_for(100ms);
        if (processor_ready == std::future_status::ready) {
          grpc::Status _ret_status = processor_status_fut.get();
          if (!_ret_status.ok()) {
            ret_status = _ret_status;
            processor_cancelled = true;
            break;
          }
        }
      }
    }

    reader_thread.join();
    processor_thread.join();

    return ret_status;

  } catch (const std::exception& ex) {
    TIRO_SPEECH_WARN("Unhandled exception: {}", ex.what());
    return grpc::Status{grpc::StatusCode::INTERNAL,
                        "Unknown error in StreamingRecognize."};
  }
}

void SpeechService::RegisterModel(
    ModelId model_id, const std::shared_ptr<const KaldiModel>& model) {
  models_[std::move(model_id)] = model;
}

}  // end namespace tiro_speech
