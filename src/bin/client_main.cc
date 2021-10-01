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
#include <google/protobuf/text_format.h>
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
using namespace std::chrono_literals;

#include "google/cloud/speech/v1/cloud_speech.grpc.pb.h"
#include "google/rpc/error_details.pb.h"
#include "google/rpc/status.pb.h"
#include "proto/tiro/speech/v1alpha/speech.grpc.pb.h"

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

template <typename Response>
void PrintBestTranscript(const Response& response,
                         std::ostream& os = std::cout) {
  std::cerr << response.Utf8DebugString() << '\n';
  if (response.results_size() == 0) {
    throw std::runtime_error{"No results received"};
  }
  for (const auto& res : response.results()) {
    if (res.alternatives_size() < 1) {
      continue;
    }
    auto alt = res.alternatives(0);
    os << alt.transcript();
  }
  os << std::endl;
}

void PrintErrorWithDetails(const grpc::Status& stat,
                           std::ostream& os = std::cerr) {
  os << "Couldn't send Recognize request" << '\n'
     << "Message: " << stat.error_message() << '\n';

  if (!stat.error_details().empty()) {
    os << "... Details:\n";
    google::rpc::Status status_details;
    status_details.ParseFromString(stat.error_details());
    for (const auto& detail : status_details.details()) {
      if (detail.Is<google::rpc::BadRequest>()) {
        google::rpc::BadRequest bad_request;
        detail.UnpackTo(&bad_request);
        os << bad_request.Utf8DebugString() << '\n';
      } else if (detail.Is<google::rpc::ErrorInfo>()) {
        google::rpc::ErrorInfo error_info;
        detail.UnpackTo(&error_info);
        os << error_info.Utf8DebugString() << '\n';
      } else {
        os << "Unknown error detail type: \n"
           << detail.Utf8DebugString() << '\n';
      }
    }
  }
}

struct ProgramOptions {
  bool use_ssl = false;
  bool streaming = false;
  bool print_help = false;
  bool punctuation = false;
  bool use_google_cloud_api = false;

  ProgramOptions() = default;
  ProgramOptions(int argc, char* argv[]) { Parse(argc, argv); }

  int NumArgs() { return argc_; }
  std::string Argv(int i) { return argv_.at(i); }

 private:
  int argc_;
  std::vector<std::string> argv_;

  void Parse(int argc, char* argv[]) {
    argc_ = argc - 1;
    for (int i = 1; i < argc; ++i) {
      if (argv[i][0] != '-') {
        argv_.push_back(argv[i]);
      } else if (std::string{"--use-ssl"} == argv[i]) {
        use_ssl = true;
        argc_--;
      } else if (std::string{"--streaming"} == argv[i]) {
        streaming = true;
        argc_--;
      } else if (std::string{"--help"} == argv[i]) {
        print_help = true;
        argc_--;
      } else if (std::string{"--punctuation"} == argv[i]) {
        punctuation = true;
        argc_--;
      } else if (std::string{"--use-google-cloud-api"} == argv[i]) {
        use_google_cloud_api = true;
        argc_--;
      } else {
        std::cerr << "Invalid option\n";
        exit(EXIT_FAILURE);
      }
    }
  }
};

// // Example async client:
// class Streamer
//     : grpc::experimental::ClientBidiReactor<StreamingRecognizeRequest,
//                                             StreamingRecognizeResponse> {
//  public:
//   explicit Streamer(Speech::Stub* stub, const RecognitionConfig& config,
//                     std::string&& audio_content)
//       : audio_content_{audio_content} {
//     stub->experimental_async()->StreamingRecognize(&ctx_, this);
//
//     req_.mutable_streaming_config()->mutable_config()->CopyFrom(config);
//     req_.mutable_streaming_config()->set_interim_results(true);
//
//     StartRead(&res_);
//     FirstWrite();
//     StartCall();
//   }
//
//   grpc::Status Wait() {
//     std::unique_lock<std::mutex> l{mu_};
//     cv_.wait(l, [this] { return done_; });
//     return std::move(status_);
//   }
//
//   void OnDone(const grpc::Status& stat) override {
//     {
//       std::lock_guard<std::mutex> l{mu_};
//       status_ = stat;
//       done_ = true;
//     }
//     cv_.notify_one();
//   }
//
//   void OnReadDone(bool ok) override {
//     if (ok) {
//       std::cerr << res_.Utf8DebugString();
//       if (res_.results().size() > 0 && res_.results(0).is_final()) {
//         PrintBestTranscript(res_);
//       }
//       StartRead(&res_);
//     }
//   }
//
//   void OnWriteDone(bool ok) override {
//     NextWrite();
//   }
//
//  private:
//   grpc::ClientContext ctx_;
//   StreamingRecognizeResponse res_;
//   StreamingRecognizeRequest req_;
//   StreamingRecognitionConfig streaming_config_;
//
//   std::mutex mu_;
//   std::condition_variable cv_;
//   bool done_ = false;
//   grpc::Status status_;
//
//   void FirstWrite() {
//     StartWrite(&req_);
//   }
//
//   std::chrono::milliseconds delay_{50};
//   std::string audio_content_;
//   std::size_t content_byte_offset_ = 0;
//   std::size_t content_chunk_size_ = 1024;
//
//   void NextWrite() {
//     if (content_byte_offset_ >= audio_content_.size()) {
//       StartWritesDone();
//     } else {
//       std::string chunk = audio_content_.substr(
//           content_byte_offset_,
//           std::min(content_chunk_size_ * 2,
//                    audio_content_.size() - content_byte_offset_));
//       req_.set_audio_content(std::move(chunk));
//       content_byte_offset_ += content_chunk_size_ * 2;
//       StartWrite(&req_);
//       std::this_thread::sleep_for(delay_);
//     }
//   }
// };

template <typename RecognitionConfig, typename Speech,
          typename RecognitionAudio, typename RecognizeRequest,
          typename RecognizeResponse, typename StreamingRecognizeResponse,
          typename StreamingRecognizeRequest>
int templated_main(ProgramOptions& opts) {
  const std::string wave_filename = opts.Argv(0);
  const std::string recognition_config_filename =
      opts.NumArgs() > 1 ? opts.Argv(1) : "";
  const std::string address =
      opts.NumArgs() > 2 ? opts.Argv(2) : "localhost:50051";

  RecognitionConfig config;
  config.set_encoding(RecognitionConfig::LINEAR16);
  config.set_sample_rate_hertz(16000);
  config.set_language_code("is-IS");
  config.set_enable_automatic_punctuation(opts.punctuation);
  config.mutable_diarization_config()->set_enable_speaker_diarization(true);

  if (!recognition_config_filename.empty()) {
    const std::string content = GetFileContents(recognition_config_filename);
    if (!config.ParseFromString(content)) {
      if (!google::protobuf::TextFormat::ParseFromString(content, &config)) {
        std::cerr << "Couldn't parse RecognitionConfig from "
                  << recognition_config_filename << '\n';
        return EXIT_FAILURE;
      }
    }
  }

  std::cerr << config.Utf8DebugString() << '\n';

  std::shared_ptr<grpc::ChannelCredentials> creds;
  if (opts.use_ssl) {
    creds = grpc::SslCredentials(grpc::SslCredentialsOptions{});
  } else {
    creds = grpc::InsecureChannelCredentials();
  }
  auto stub = Speech::NewStub(grpc::CreateChannel(address, creds));

  if (!opts.streaming) {
    RecognizeRequest req;
    req.mutable_config()->CopyFrom(config);

    RecognitionAudio audio;
    if (wave_filename.substr(0, 7) == "http://" ||
        wave_filename.substr(0, 8) == "https://") {
      audio.set_uri(wave_filename);
    } else {
      audio.set_content(GetFileContents(wave_filename));
    }
    req.mutable_audio()->CopyFrom(audio);

    RecognizeResponse res;
    grpc::ClientContext ctx;
    grpc::Status stat = stub->Recognize(&ctx, req, &res);
    if (!stat.ok()) {
      PrintErrorWithDetails(stat);
      return EXIT_FAILURE;
    }
    PrintBestTranscript(res);
  } else {
    // Streamer streamer{&*stub, config, GetFileContents(wave_filename)};
    // grpc::Status stat = streamer.Wait();
    // if (!stat.ok()) {
    //   std::cerr << "Stream failed.\n";
    //   PrintErrorWithDetails(stat);
    //   return EXIT_FAILURE;
    // }
    grpc::ClientContext ctx;
    ctx.set_wait_for_ready(true);
    auto stream = stub->StreamingRecognize(&ctx);

    std::thread reader{[&stream]() {
      StreamingRecognizeResponse res;
      stream->WaitForInitialMetadata();
      while (stream->Read(&res)) {
        std::cerr << res.Utf8DebugString();
        if (res.results_size() > 0) {
          if (res.results(0).is_final()) {
            std::cout << '\r' << res.results(0).alternatives(0).transcript()
                      << '\n';
          } else {
            std::string partial = "";
            for (const auto& partial_result : res.results()) {
              partial += partial_result.alternatives(0).transcript();
            }
            std::cout << "\r" << partial;
          }
        }
      }
      std::cerr << "done reading\n";
    }};

    const std::chrono::milliseconds delay{2};
    const std::string audio_content = GetFileContents(wave_filename);
    std::size_t content_byte_offset_ = 0;
    const std::size_t content_chunk_size_ = 4096;

    StreamingRecognizeRequest req;
    req.mutable_streaming_config()->mutable_config()->CopyFrom(config);
    req.mutable_streaming_config()->set_interim_results(true);

    if (!stream->Write(req)) {
      std::cerr << "Initial write failed\n";
      return EXIT_FAILURE;
    }

    for (; content_byte_offset_ < audio_content.size();
         content_byte_offset_ += content_chunk_size_ * 2) {
      std::string chunk = audio_content.substr(
          content_byte_offset_,
          std::min(content_chunk_size_ * 2,
                   audio_content.size() - content_byte_offset_));
      req.set_audio_content(std::move(chunk));
      if (!stream->Write(req)) {
        std::cerr << "Write failed\n";
        return EXIT_FAILURE;
      }
      std::this_thread::sleep_for(delay);
    }
    stream->WritesDone();
    reader.join();
    if (grpc::Status stat = stream->Finish(); !stat.ok()) {
      PrintErrorWithDetails(stat);
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
  try {
    const std::string usage{
        "Example client \n"
        "usage: tiro_speech_client [options] wave-filename "
        "[recognition_config_pb "
        "[host:port]] \n"
        " e.g. lr-speech-client --use-ssl example.wav example_config.pbtxt "
        "localhost:50051"};

    ProgramOptions opts{argc, argv};

    if (opts.print_help) {
      std::cerr << usage << '\n';
      return EXIT_SUCCESS;
    }

    if (opts.NumArgs() < 1 || opts.NumArgs() > 3) {
      std::cerr << usage << '\n';
      return EXIT_FAILURE;
    }

    if (opts.use_google_cloud_api) {
      using namespace google::cloud::speech::v1;
      return templated_main<RecognitionConfig, Speech, RecognitionAudio,
                            RecognizeRequest, RecognizeResponse,
                            StreamingRecognizeResponse,
                            StreamingRecognizeRequest>(opts);
    } else {
      using namespace tiro::speech::v1alpha;
      return templated_main<RecognitionConfig, Speech, RecognitionAudio,
                            RecognizeRequest, RecognizeResponse,
                            StreamingRecognizeResponse,
                            StreamingRecognizeRequest>(opts);
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
