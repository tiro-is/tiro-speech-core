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
#include "src/server.h"

#include <grpcpp/channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
// #include <grpcpp/ext/channelz_service_plugin.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "src/api/services.h"
#include "src/base.h"
#include "src/kaldi-model.h"
#include "src/logging.h"
#include "src/proto/tiro/speech/v1alpha/speech.grpc.pb.h"
#include "src/recognizer.h"
#include "src/utils.h"

namespace {

struct PluginRegistry {
  PluginRegistry() {
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // grpc::channelz::experimental::InitChannelzService();
  }
};

[[maybe_unused]] static const PluginRegistry plugin_registry{};

}  // namespace

namespace tiro_speech {

SpeechServerInfo::SpeechServerInfo(const SpeechServerOptions& opts)
    : options_{opts} {
  opts.Check();
  if (opts.use_tls) {
    tls_server_cert_ = GetFileContents(opts.tls_server_cert_filename);
    tls_server_key_ = GetFileContents(opts.tls_server_key_filename);
    tls_ca_cert_ = GetFileContents(opts.tls_ca_cert_filename);
  }

  if (!opts.kaldi_models.empty()) {
    for (const auto model_path : Split(opts.kaldi_models, ',')) {
      TIRO_SPEECH_INFO("Loading model from {}", model_path);
      std::shared_ptr<KaldiModel> model = KaldiModel::Read(model_path);
      models_.emplace(model->config.language_code, model);
    }
  } else {
    TIRO_SPEECH_WARN("Not loading any models! This is usually a bad warning!");
  }
}

std::shared_ptr<grpc::ServerCredentials>
SpeechServerInfo::CreateSpeechServerCredentials() const {
  std::shared_ptr<grpc::ServerCredentials> creds;
  if (Options().use_tls) {
    TIRO_SPEECH_INFO("Using TLS server credentials");
    assert(!tls_server_key_.empty());
    assert(!tls_server_cert_.empty());
    grpc::SslServerCredentialsOptions::PemKeyCertPair key_pair;
    key_pair.private_key = tls_server_key_;
    key_pair.cert_chain = tls_server_cert_;

    grpc::SslServerCredentialsOptions ssl_opts{
        Options().tls_require_client_cert
            ? GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY
            : GRPC_SSL_DONT_REQUEST_CLIENT_CERTIFICATE};
    ssl_opts.pem_key_cert_pairs.push_back(key_pair);

    if (!tls_ca_cert_.empty()) {
      TIRO_SPEECH_INFO("Using provided root CA cert");
      ssl_opts.pem_root_certs = tls_ca_cert_;
    }

    creds = grpc::SslServerCredentials(ssl_opts);
  } else {
    TIRO_SPEECH_WARN(
        "NOT using TLS server credentials. Hopefully you're using a TLS "
        "termination proxy.");
    creds = grpc::InsecureServerCredentials();
  }

  return creds;
}

SpeechServer::SpeechServer(const SpeechServerInfo& info) : info_{info} {
  for (const auto& model_pair : info_.Models()) {
    RegisterModel(model_pair.first, model_pair.second);
  }
}

void SpeechServer::RegisterModel(
    const std::shared_ptr<const KaldiModel>& model) {
  RegisterModel(model->config.language_code, model);
}

void SpeechServer::RegisterModel(
    const std::string& language_code,
    const std::shared_ptr<const KaldiModel>& model) {
  if (rpc_server_ != nullptr)
    TIRO_SPEECH_WARN(
        "Registering a model after starting server does not have any "
        "effect.");
  models_[language_code] = model;
  TIRO_SPEECH_INFO("Registered model for language code '{}'", language_code);
}

void SpeechServer::Start() {
  auto speech_service = std::make_unique<tiro_speech::SpeechService>();
  for (const auto& model_pair : models_) {
    speech_service->RegisterModel(model_pair.first, model_pair.second);
  }

  auto creds = info_.CreateSpeechServerCredentials();
  grpc::ServerBuilder builder;
  builder.AddListeningPort(info_.Options().listen_address, creds)
      .SetMaxReceiveMessageSize(268435500)
      .RegisterService(speech_service.get());

  services_.push_back(std::move(speech_service));

  rpc_server_ = builder.BuildAndStart();
}

void SpeechServer::Wait() { rpc_server_->Wait(); }

void SpeechServer::Shutdown() { rpc_server_->Shutdown(); }

}  // namespace tiro_speech
