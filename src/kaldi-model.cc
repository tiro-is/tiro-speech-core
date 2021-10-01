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
#include "src/kaldi-model.h"

#include <nnet3/nnet-utils.h>

#include <filesystem>
#include <system_error>

#include "src/itn/formatter.h"
#include "src/logging.h"
#include "src/options.h"
#include "src/scoped-chdir.h"
#include "src/utils.h"

namespace tiro_speech {

void KaldiModelConfig::Check() const {
  bool error_occured = false;
  if (nnet3_rxfilename.empty()) {
    error_occured = true;
    TIRO_SPEECH_ERROR("Flag nnet3-rxfilename can't be empty.");
  }
  if (fst_rxfilename.empty()) {
    error_occured = true;
    TIRO_SPEECH_ERROR("Flag fst-rxfilename can't be empty.");
  }
  if (word_syms_rxfilename.empty()) {
    error_occured = true;
    TIRO_SPEECH_ERROR("Flag word_syms_rxfilename can't be empty.");
  }
  if (language_code.empty()) {
    error_occured = true;
    TIRO_SPEECH_ERROR("Flag language_code can't be empty.");
  }
  if ((!formatter_config.lexicon_fst_filename.empty() &&
       formatter_config.lexicon_fst_filename.empty()) ||
      (formatter_config.lexicon_fst_filename.empty() &&
       !formatter_config.lexicon_fst_filename.empty())) {
    error_occured = true;
    TIRO_SPEECH_ERROR(
        "Both formatter.lexicon-fst AND formatter.rewrite-fst have to be set");
  }
  if (diarization_enabled &&
      (xvector_diarization_config.plda_filename.empty() ||
       xvector_diarization_config.whitening_matrix_filename.empty() ||
       xvector_diarization_config.centering_vector_filename.empty() ||
       xvector_diarization_config.xvector_nnet_opts.nnet_rxfilename.empty())) {
    error_occured = true;
    TIRO_SPEECH_ERROR(
        "Flag diarization_enabled set but missing one of "
        "diarization.plda_filename, diarization.centering_vector_filename, "
        "diarization.whitening_matrix_filename and "
        "diarization.nnet_rxfilename");
  }

  if (error_occured) {
    TIRO_SPEECH_FATAL("Got invalid configuration for KaldiModel.");
  }
}

KaldiModel::KaldiModel(const KaldiModelConfig& config)
    : decoding_graph{fst::ReadFstKaldiGeneric(config.fst_rxfilename)},
      word_syms{fst::SymbolTable::ReadText(config.word_syms_rxfilename)},
      const_arpa_valid{false},
      endpoint_config{config.endpoint_config},
      feature_config{config.feature_config},
      decoder_config{config.decoder_config},
      decodable_config{config.decodable_config},
      word_boundary_info{config.word_boundary_config,
                         config.word_boundary_rxfilename},
      feature_info{feature_config},
      initial_adaptation_state{feature_info.ivector_extractor_info},
      config{config} {
  if (!config.initial_ivector_rxfilename.empty()) {
    TIRO_SPEECH_INFO(
        "Using non-zero i-vector (rxfn == '{}') to initial i-vector "
        "extraction.",
        config.initial_ivector_rxfilename);
    bool binary;
    kaldi::Input ki{config.initial_ivector_rxfilename, &binary};
    initial_adaptation_state.Read(ki.Stream(), binary);
  }

  bool binary;
  kaldi::Input ki(config.nnet3_rxfilename, &binary);
  trans_model.Read(ki.Stream(), binary);
  am_nnet.Read(ki.Stream(), binary);
  kaldi::nnet3::SetBatchnormTestMode(true, &(am_nnet.GetNnet()));
  kaldi::nnet3::SetDropoutTestMode(true, &(am_nnet.GetNnet()));
  kaldi::nnet3::CollapseModel(kaldi::nnet3::CollapseModelConfig(),
                              &(am_nnet.GetNnet()));

  decodable_info =
      std::make_shared<const Decodable>(decodable_config, &am_nnet);

  if (!config.const_arpa_rxfilename.empty()) {
    kaldi::ReadKaldiObject(config.const_arpa_rxfilename, &const_arpa_lm);
    const_arpa_valid = true;
  }

  if (!config.punctuator_config.pytorch_jit_model_filename.empty() &&
      !config.punctuator_config.word_piece_opts.vocab_filename.empty()) {
    punctuator =
        std::make_shared<itn::ElectraPunctuator>(config.punctuator_config);
  }

  if (!config.formatter_config.lexicon_fst_filename.empty() &&
      !config.formatter_config.rewrite_fst_filename.empty()) {
    formatter = std::make_shared<itn::Formatter>(config.formatter_config);
  }

  if (config.diarization_enabled) {
    diarization_info = std::make_shared<XvectorDiarizationDecoderInfo>(
        config.xvector_diarization_config);
  }
}

std::shared_ptr<KaldiModel> KaldiModel::Read(
    const std::string_view model_path) {
  ScopedChdir _{model_path};
  KaldiModelConfig model_config;
  ParseOptions po{""};
  model_config.Register(&po);
  // TODO(rkjaran): throw known exception if this isn't found

  if (!std::filesystem::exists("main.conf")) {
    throw std::filesystem::filesystem_error{
        fmt::format(
            "'{}/main.conf' does not exist! Is this not a packaged model?",
            model_path),
        std::make_error_code(std::errc::no_such_file_or_directory)};
  }

  po.ReadConfigFile("main.conf");  // Standard name for model config files.
  po.PrintConfig(std::cerr);

  return std::make_shared<KaldiModel>(model_config);
}

}  // namespace tiro_speech
