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
#ifndef TIRO_SPEECH_KALDI_MODEL_H_
#define TIRO_SPEECH_KALDI_MODEL_H_

#include <fmt/format.h>
#include <lat/word-align-lattice.h>
#include <lm/const-arpa-lm.h>
#include <nnet3/decodable-simple-looped.h>
#include <online2/online-nnet3-decoding.h>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "src/options.h"

namespace tiro_speech {

struct KaldiModelConfig {
  using NnetOptions = kaldi::nnet3::NnetSimpleLoopedComputationOptions;

  std::string nnet3_rxfilename{"final.mdl"};            //< opt:REQUIRED
  std::string fst_rxfilename{"graph/HCLG.fst"};         //< opt:REQUIRED
  std::string word_syms_rxfilename{"graph/words.txt"};  //< opt:REQUIRED
  std::string align_lexicon_rxfilename;                 //< opt:OPTIONAL
  std::string const_arpa_rxfilename;                    //< opt:OPTIONAL
  std::string initial_ivector_rxfilename;               //< opt:OPTIONAL
  std::string language_code;                            //< opt:REQUIRED

  kaldi::OnlineEndpointConfig endpoint_config;
  kaldi::OnlineNnet2FeaturePipelineConfig feature_config;
  NnetOptions decodable_config;
  kaldi::LatticeFasterDecoderConfig decoder_config;

  /// These are needed for word alignment
  kaldi::WordBoundaryInfoNewOpts word_boundary_config;
  std::string word_boundary_rxfilename{"graph/phones/word_boundary.int"};

  void Register(OptionsItf* opts) {
    opts->Register("nnet3-rxfilename", &nnet3_rxfilename,
                   "Filename (possibly extended) of nnet3 acoustic model");
    opts->Register("fst-rxfilename", &fst_rxfilename,
                   "Filename (possibly extended) of decoding graph (HCLG).");
    opts->Register("word-syms-rxfilename", &word_syms_rxfilename,
                   "Filename of word symbol table.");
    opts->Register("align-lexicon-int", &align_lexicon_rxfilename,
                   "Filename of aligned lexicon with integer symbol ids. If "
                   "model uses word-position phone markers the lexicon must "
                   "also use them.  Leave empty if model doesn't need to "
                   "support time aligning words.");
    opts->Register(
        "const-arpa-rxfilename", &const_arpa_rxfilename,
        "Filename of ConstARPA language model to use for rescoring.");
    opts->Register("initial-ivector-rxfilename", &initial_ivector_rxfilename,
                   "I-vector to use for initialization instead of the zero "
                   "i-vector");
    opts->Register("language-code", &language_code,
                   "Language code (BCP-47) of "
                   "model. E.g. 'is-IS' or 'is-x-radiology-ct'");
    endpoint_config.Register(opts);
    feature_config.Register(opts);
    decoder_config.Register(opts);
    decodable_config.Register(opts);

    kaldi::ParseOptions word_boundary_opts{"word-boundary", opts};
    word_boundary_config.Register(&word_boundary_opts);
    opts->Register("word-boundary-int-rxfilename", &word_boundary_rxfilename,
                   "Word boundary file, necessary for word alignment.");
  }

  void Check() const;
};

/**
 * KaldiModel encapsulates necessary and shareable model
 * resources and configuration.
 */
struct KaldiModel {
  using DecodingGraph = fst::Fst<fst::StdArc>;
  using AdaptationState = kaldi::OnlineIvectorExtractorAdaptationState;
  using Decodable = kaldi::nnet3::DecodableNnetSimpleLoopedInfo;

  explicit KaldiModel(const KaldiModelConfig& config);

  /** \brief Loads a KaldiModel into memory from a filesystem path
   *
   * \param [in] model_path Path to a model directory. The directory has to
   *                        contain at least a file called "main.conf" which
   *                        contains values for the config flags registered in
   *                        KaldiModelConfig.
   */
  static std::shared_ptr<KaldiModel> Read(const std::string_view model_path);

  kaldi::TransitionModel trans_model;
  kaldi::nnet3::AmNnetSimple am_nnet;
  std::shared_ptr<DecodingGraph> decoding_graph;
  std::shared_ptr<fst::SymbolTable> word_syms;
  std::shared_ptr<const Decodable> decodable_info;
  kaldi::ConstArpaLm const_arpa_lm;
  bool const_arpa_valid;

  const kaldi::OnlineEndpointConfig endpoint_config;
  const kaldi::OnlineNnet2FeaturePipelineConfig feature_config;
  const kaldi::LatticeFasterDecoderConfig decoder_config;
  const KaldiModelConfig::NnetOptions decodable_config;

  const kaldi::WordBoundaryInfo word_boundary_info;

  kaldi::OnlineNnet2FeaturePipelineInfo feature_info;
  AdaptationState initial_adaptation_state;

  const KaldiModelConfig config;
};

std::shared_ptr<KaldiModel> CreateKaldiModel(const std::string_view model_path);

struct ModelId {
  std::string language_code;
  std::string model_name;

  friend bool operator==(const ModelId& lhs, const ModelId& rhs) {
    return lhs.language_code == rhs.language_code &&
           lhs.model_name == rhs.model_name;
  }

  friend std::ostream& operator<<(std::ostream& os, const ModelId& model_id) {
    return os << fmt::format("{{ language_code: '{}' model_name: '{}' }}",
                             model_id.language_code, model_id.model_name);
  }
};

using KaldiModelMap =
    std::unordered_map<ModelId, std::shared_ptr<const KaldiModel>>;

}  // namespace tiro_speech

// Inject specialization of std::hash for ModelId into namespace std
namespace std {
template <>
struct hash<tiro_speech::ModelId> {
  std::size_t operator()(const tiro_speech::ModelId& model_id) const noexcept {
    std::size_t h1 =
        std::hash<decltype(model_id.model_name)>{}(model_id.language_code);
    std::size_t h2 =
        std::hash<decltype(model_id.model_name)>{}(model_id.model_name);
    return h1 ^ (h2 << 1);
  }
};
}  // namespace std

#endif  // TIRO_SPEECH_KALDI_MODEL_H_
