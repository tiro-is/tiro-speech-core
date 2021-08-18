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
#ifndef TIRO_SPEECH_SRC_API_SERVICES_H_
#define TIRO_SPEECH_SRC_API_SERVICES_H_

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "proto/tiro/speech/v1alpha/speech.grpc.pb.h"
#include "src/audio/audio-source.h"
#include "src/base.h"
#include "src/kaldi-model.h"
#include "src/recognizer.h"
#include "src/utils.h"

namespace tiro_speech {

class SpeechService final : public tiro::speech::v1alpha::Speech::Service {
 public:
  using RecognizeRequest = tiro::speech::v1alpha::RecognizeRequest;
  using RecognizeResponse = tiro::speech::v1alpha::RecognizeResponse;
  using StreamingRecognizeRequest =
      tiro::speech::v1alpha::StreamingRecognizeRequest;
  using StreamingRecognizeResponse =
      tiro::speech::v1alpha::StreamingRecognizeResponse;

  grpc::Status Recognize(grpc::ServerContext* context,
                         const RecognizeRequest* request,
                         RecognizeResponse* response) override;

  grpc::Status StreamingRecognize(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<StreamingRecognizeResponse,
                               StreamingRecognizeRequest>* stream) override;

  void RegisterModel(ModelId model_id,
                     const std::shared_ptr<const KaldiModel>& model);

 private:
  KaldiModelMap models_;
};

}  // end namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_API_SERVICES_H_
