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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "google/rpc/error_details.pb.h"
#include "google/rpc/status.pb.h"
#include "src/proto/tiro/speech/v1alpha/speech.grpc.pb.h"

using namespace tiro::speech::v1alpha;

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

template <class Response>
void PrintBestTranscript(const Response& response) {
  std::cerr << response.Utf8DebugString() << '\n';
  if (response.results_size() == 0) {
    throw std::runtime_error{"No results received"};
  }
  for (const SpeechRecognitionResult& res : response.results()) {
    if (res.alternatives_size() < 1) {
      continue;
    }
    SpeechRecognitionAlternative alt = res.alternatives(0);
    std::cout << alt.transcript();
  }
  std::cout << std::endl;
}

int main(int argc, char* argv[]) {
  try {
    const std::string usage{
        "Example client \n"
        "usage: tiro_speech_client wave-filename [recognition_config_pb "
        "[host:port]] \n"
        " e.g. lr-speech-client --use-ssl --ssl-root-cert=keys/dev.cert "
        "example.wav localhost:50051"};

    if (argc < 2 || argc > 4) {
      std::cerr << usage << '\n';
      return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; ++i) {
      if (std::string{"--help"} == argv[i]) {
        std::cerr << usage << '\n';
        return EXIT_SUCCESS;
      }
    }

    const std::string wave_filename = argv[1];
    const std::string recognition_config_filename = argc > 2 ? argv[2] : "";
    const std::string address = argc > 3 ? argv[3] : "localhost:50051";

    RecognitionConfig config;
    config.set_encoding(RecognitionConfig::LINEAR16);
    config.set_sample_rate_hertz(16000);
    config.set_language_code("is-IS");

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

    config.PrintDebugString();

    RecognitionAudio audio;
    audio.set_content(GetFileContents(wave_filename));

    auto creds = grpc::InsecureChannelCredentials();
    auto stub = Speech::NewStub(grpc::CreateChannel(address, creds));

    RecognizeRequest req;
    req.mutable_config()->CopyFrom(config);
    req.mutable_audio()->CopyFrom(audio);

    RecognizeResponse res;
    grpc::ClientContext ctx;
    grpc::Status stat = stub->Recognize(&ctx, req, &res);
    if (!stat.ok()) {
      std::cerr << "Couldn't send Recognize request" << '\n'
                << "Message: " << stat.error_message() << '\n';

      if (!stat.error_details().empty()) {
        std::cerr << "... Details:\n";
        google::rpc::Status status_details;
        status_details.ParseFromString(stat.error_details());
        for (const auto& detail : status_details.details()) {
          if (detail.Is<google::rpc::BadRequest>()) {
            google::rpc::BadRequest bad_request;
            detail.UnpackTo(&bad_request);
            std::cerr << bad_request.Utf8DebugString() << '\n';
          } else if (detail.Is<google::rpc::ErrorInfo>()) {
            google::rpc::ErrorInfo error_info;
            detail.UnpackTo(&error_info);
            std::cerr << error_info.Utf8DebugString() << '\n';
          } else {
            std::cerr << "Unknown error detail type: \n"
                      << detail.Utf8DebugString() << '\n';
          }
        }
      }

      return EXIT_FAILURE;
    }

    std::cout << res.Utf8DebugString() << '\n';

    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
