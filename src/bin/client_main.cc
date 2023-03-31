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
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>
using json = nlohmann::json;

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
  bool interim_results = false;
  bool only_final =false;
  bool only_timestamp_output = false;
  bool json = false;

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
      if (std::strncmp(&argv[i][0], "--", 2) != 0) {
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
      } else if (std::string{"--interim-results"} == argv[i]) {
        interim_results = true;
        argc_--;
      } else if (std::string{"--only-final"} == argv[i]) {
        only_final = true;
        argc_--;
      } else if (std::string{"--only-timestamp-output"} == argv[i]) {
        only_timestamp_output = true;
        argc_--;
      } else if (std::string{"--json"} == argv[i]) {
        json = true;
        argc_--;
      } else {
        std::cerr << "Invalid option\n";
        exit(EXIT_FAILURE);
      }
    }
  }
};


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
    grpc::ClientContext ctx;
    ctx.set_wait_for_ready(true);
    auto stream = stub->StreamingRecognize(&ctx);
    json json_object = {};
    std::thread reader{[&stream, &opts, &json_object]() {
      try {
        StreamingRecognizeResponse res;
        stream->WaitForInitialMetadata();
        int i = 0;
        while (stream->Read(&res)) {
          if (opts.json && res.results(0).is_final()) {
            json_object[i] = {};
            const auto& result = res.results(0);
            for (int j = 0; j < result.alternatives_size(); ++j) {
              const auto& alternative = result.alternatives(j);
              const auto& words = alternative.words();
              json words_obj = json::array();
              for (int y = 0; y < words.size(); ++y) {
                const auto& word_info = words.Get(y);
                words_obj.push_back(
                    {{"word", word_info.word()},
                     {"start_time",
                      {{"seconds", word_info.start_time().seconds()},
                       {"nanos", word_info.start_time().nanos()}}},
                     {"end_time",
                      {{"seconds", word_info.end_time().seconds()},
                       {"nanos", word_info.end_time().nanos()}}}});
              }
              json_object[i][j]["words"] = words_obj;
            }
            ++i;
          } else if (!opts.only_final) {
            std::cout << res.Utf8DebugString();
          } else {
            if (!opts.only_timestamp_output && res.results_size() > 0) {
              if (opts.interim_results && res.results(0).is_final()) {
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
          if (!opts.only_timestamp_output && !opts.json && !opts.only_final) {
            std::cout << '\r' << res.results(0).alternatives(0).transcript()
                      << '\n';
          }
        }
        std::cout << json_object.dump(2)
                  << std::endl;  // Print the JSON objects with
                                 // an indentation of 2 spaces
      } catch (const std::exception& e) {
        std::cerr << "reader failure\n";
        std::cerr << e.what() << '\n';
        exit(EXIT_FAILURE);
      }
    }};

    std::ifstream wave_stream{
        wave_filename == "-" ? "/dev/stdin" : wave_filename, std::ios::binary};
    if (!wave_stream.is_open()) {
      std::cerr << "Could not open stream from '" << wave_filename << "'\n";
      return EXIT_FAILURE;
    }

    StreamingRecognizeRequest req;
    req.mutable_streaming_config()->mutable_config()->CopyFrom(config);
    req.mutable_streaming_config()->set_interim_results(opts.interim_results);

    if (!stream->Write(req)) {
      std::cerr << "Initial write failed\n";
      return EXIT_FAILURE;
    }

    const std::size_t content_chunk_size_bytes = 1024 * 2;
    req.mutable_audio_content()->resize(content_chunk_size_bytes);
    while (!wave_stream
                .read(req.mutable_audio_content()->data(),
                      content_chunk_size_bytes)
                .eof()) {
      if (!stream->Write(req)) {
        std::cerr << "Write failed\n";
        return EXIT_FAILURE;
      }
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
        " e.g. tiro-speech-core --use-ssl example.wav "
        "example_config.pbtxt "
        "localhost:50051\n\n"
        "Options:\n\n"
        "--use-ssl                     Enable SSL for secure "
        "communication.\n"
        "--streaming                   Enable streaming mode.\n"
        "--help                        Display this help message.\n"
        "--punctuation                 Enable punctuation in the "
        "output.\n"
        "--use-google-cloud-api        Use the Google Cloud API for "
        "processing.\n"
        "--interim-results             Enable interim results in the "
        "output, "
        "only used in streaming mode.\n"
        "--only-timestamp-output       Only output the transcript object "
        "with"
        "timestamps for each word. Only used in streaming mode.\n"
        "--only-final                  Only output the final transcript"
        "Only used in streaming mode.\n"
        "--json                        Output the transcript in JSON "
        "format. "
        "Only used in streaming mode.\n\n"
        "Example config.pbtxt file:\n\n"
        "encoding: LINEAR16\n"
        "sample_rate_hertz: 16000\n"
        "language_code: 'is-IS'\n"
        "max_alternatives: 2\n"
        "enable_word_time_offsets: true\n"
        "enable_automatic_punctuation: true"};
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
