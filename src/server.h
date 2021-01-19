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
#ifndef TIRO_SPEECH_SRC_SERVER_H_
#define TIRO_SPEECH_SRC_SERVER_H_

#include <grpcpp/server.h>

#include <memory>
#include <string>

#include "src/api/services.h"
#include "src/base.h"
#include "src/kaldi-model.h"
#include "src/logging.h"
#include "src/options.h"
#include "src/recognizer.h"

namespace tiro_speech {

struct SpeechServerOptions {
  std::string listen_address = "0.0.0.0:50051";
  std::string tls_server_cert_filename = "";
  std::string tls_server_key_filename = "";
  std::string tls_ca_cert_filename = "";
  bool tls_require_client_cert = false;
  bool use_tls = false;
  std::string kaldi_models = "";

  /// Decoder config that gets applied to all KaldiModels
  kaldi::LatticeFasterDecoderConfig decoder_config;

  void Register(OptionsItf* opts) {
    opts->Register("listen-address", &listen_address,
                   "Listen on address hostname:port");
    opts->Register("tls-server-cert", &tls_server_cert_filename,
                   "Path to TLS server certificate.");
    opts->Register("tls-server-key", &tls_server_key_filename,
                   "Path to TLS server private key.");
    opts->Register("tls-ca-cert", &tls_ca_cert_filename,
                   "Path to TLS certificate authority cert.");
    opts->Register("tls-require-client-cert", &tls_require_client_cert,
                   "Require client to present a valid certificate.");
    opts->Register("use-tls", &use_tls, "Enable TLS.");
    opts->Register(
        "kaldi-models", &kaldi_models,
        "Paths to Kaldi model directories, seperated by commas ','.");

    ParseOptions decoder_po{"decoder", opts};
    decoder_config.Register(&decoder_po);
  }

  void Check() const {
    if (use_tls) {
      if (tls_server_cert_filename.empty() || tls_server_key_filename.empty()) {
        TIRO_SPEECH_ERROR(
            "tls-server-key or tls-server-cert empty while use-tls=true");
      }
    }
  }
};

class SpeechServerInfo {
  friend class SpeechServer;

 public:
  explicit SpeechServerInfo(
      const SpeechServerOptions& opts = SpeechServerOptions{});

  const SpeechServerOptions& Options() const { return options_; }

  std::shared_ptr<grpc::ServerCredentials> CreateSpeechServerCredentials()
      const;

  const KaldiModelMap& Models() const { return models_; }

 private:
  const SpeechServerOptions& options_;
  std::string tls_server_cert_;
  std::string tls_server_key_;
  std::string tls_ca_cert_;
  // XXX: This is an expensive object... having SpeechServerInfo own it is
  //      probably not wise.
  KaldiModelMap models_;
};

/** \class SpeechServer
 *
 * Usage example:
 *
 *     auto model = std::make_shared<KaldiModel>()
 *     SpeechServer server{};
 *     server.RegisterModel()
 */
class SpeechServer {
 public:
  explicit SpeechServer(const SpeechServerInfo& info);

  /** \brief Register recognizer models with the server
   *
   * Only one model per language code (\a KaldiModelConfig) is
   * supported. Adding a model with the same language code as an existing model
   * overwrites the old model.
   *
   * \todo There should be a possibility of adding multiple models with \a
   *       SpeechServerOptions.
   */
  void RegisterModel(const std::shared_ptr<const KaldiModel>& model);
  void RegisterModel(ModelId model_id,
                     const std::shared_ptr<const KaldiModel>& model);

  /** \brief Initialize the server and start serving.
   *
   * No models registered with RegisterModel() after Start() is called will be
   * used.
   */
  void Start();

  /**
   * Block current thread until server exits.
   */
  void Wait();

  /**
   * Gracefully shuts down the server.
   */
  void Shutdown();

 private:
  KaldiModelMap models_;
  const SpeechServerInfo& info_;
  std::vector<std::unique_ptr<grpc::Service>> services_;
  std::unique_ptr<grpc::Server> rpc_server_;
};

}  // namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_SERVER_H_
