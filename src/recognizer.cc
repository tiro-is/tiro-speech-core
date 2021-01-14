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
#include "src/recognizer.h"

#include <base/kaldi-error.h>
#include <fmt/ranges.h>
#include <fst/rmepsilon.h>
#include <fst/symbol-table.h>
#include <fstext/kaldi-fst-io.h>
#include <fstext/lattice-utils.h>
#include <lat/lattice-functions.h>
#include <lat/push-lattice.h>
#include <lat/word-align-lattice.h>
#include <lm/const-arpa-lm.h>
#include <nnet3/nnet-utils.h>
#include <online2/online-endpoint.h>
#include <util/kaldi-io.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "src/base.h"
#include "src/logging.h"

namespace tiro_speech {

namespace {

void RescoreLattice(kaldi::CompactLattice* mutatable_clat,
                    const kaldi::ConstArpaLm& const_arpa_lm) {
  assert(mutatable_clat != nullptr);
  kaldi::ConstArpaLmDeterministicFst const_arpa_fst{const_arpa_lm};
  fst::ScaleDeterministicOnDemandFst scaled_arpa_fst{1, &const_arpa_fst};

  kaldi::CompactLattice composed_clat;
  kaldi::ComposeCompactLatticeDeterministic(*mutatable_clat, &scaled_arpa_fst,
                                            &composed_clat);

  kaldi::Lattice composed_lat;
  fst::ConvertLattice(composed_clat, &composed_lat);
  fst::Invert(&composed_lat);
  fst::DeterminizeLattice(composed_lat, mutatable_clat);
}

}  // namespace

Recognizer::Recognizer(const KaldiModel& model)
    : Recognizer{model, model.initial_adaptation_state} {}

Recognizer::Recognizer(const KaldiModel& model,
                       const KaldiModel::AdaptationState& adaptation_state)
    : Recognizer{model, adaptation_state, {}} {}

Recognizer::Recognizer(const KaldiModel& model,
                       const KaldiModel::AdaptationState& adaptation_state,
                       std::vector<AlignedWord> left_context)
    : model_{model},
      adaptation_state_{adaptation_state},
      feature_pipeline_{model_.feature_info},
      silence_weighting_{std::make_unique<kaldi::OnlineSilenceWeighting>(
          model.trans_model, model_.feature_info.silence_weighting_config,
          model_.decodable_info->opts.frame_subsampling_factor)},
      decoder_{model.decoder_config, model.trans_model, *model.decodable_info,
               *model.decoding_graph, &feature_pipeline_},
      sample_rate_{GetSampleRate(model_.feature_info)},
      left_context_{std::move(left_context)} {
  for (const AlignedWord& ali : left_context_) {
    left_context_words_.push_back(ali.word_symbol);
  }
}

void Recognizer::SetAdaptationState(
    const KaldiModel::AdaptationState& adaptation_state) {
  feature_pipeline_.SetAdaptationState(adaptation_state);
}

KaldiModel::AdaptationState Recognizer::GetAdaptationState() const {
  feature_pipeline_.GetAdaptationState(&adaptation_state_);
  return adaptation_state_;
}

void Recognizer::ReadAdaptationState(std::istream& is, bool binary) {
  adaptation_state_.Read(is, binary);
  feature_pipeline_.SetAdaptationState(adaptation_state_);
}

void Recognizer::WriteAdaptationState(std::ostream& os, bool binary) {
  feature_pipeline_.GetAdaptationState(&adaptation_state_);
  adaptation_state_.Write(os, binary);
}

void Recognizer::Decode(const VectorBase& waveform) {
  feature_pipeline_.AcceptWaveform(sample_rate_, waveform);

  if (silence_weighting_->Active() &&
      feature_pipeline_.IvectorFeature() != nullptr) {
    silence_weighting_->ComputeCurrentTraceback(decoder_.Decoder());
    silence_weighting_->GetDeltaWeights(
        feature_pipeline_.NumFramesReady(),
        frame_offset_ * model_.decodable_config.frame_subsampling_factor,
        &delta_weights_);
    feature_pipeline_.IvectorFeature()->UpdateFrameWeights(delta_weights_);
  }
  decoder_.AdvanceDecoding();
}

bool Recognizer::HasEndpoint(bool single_utterance) {
  if (NumFramesDecoded() > 0) {
    if (single_utterance) {
      kaldi::OnlineEndpointConfig conf{};
      conf.silence_phones = model_.endpoint_config.silence_phones;
      // rule1 times out after 10 seconds of silence, even if we decoded
      // nothing.
      conf.rule1 = kaldi::OnlineEndpointRule(
          false, 10.0, std::numeric_limits<float>::infinity(), 0.0);
      conf.rule2 = kaldi::OnlineEndpointRule(true, 0.5, 2.0, 1.0);
      conf.rule3.min_utterance_length = std::numeric_limits<float>::infinity();
      conf.rule4.min_utterance_length = std::numeric_limits<float>::infinity();
      conf.rule4.min_utterance_length = std::numeric_limits<float>::infinity();
      return decoder_.EndpointDetected(conf);
    }
    return decoder_.EndpointDetected(model_.endpoint_config);
  }
  return false;
}

int32_t Recognizer::NumFramesDecoded() const {
  return decoder_.NumFramesDecoded();
}

void Recognizer::Finalize() {
  feature_pipeline_.InputFinished();
  decoder_.AdvanceDecoding();
  // decoder_.FinalizeDecoding();
}

void Recognizer::EndSegment() {
  // decoder_.FinalizeDecoding();
  frame_offset_ += decoder_.NumFramesDecoded();
}

void Recognizer::InitSegment() {
  decoder_.InitDecoding(frame_offset_);
  silence_weighting_ = std::make_unique<kaldi::OnlineSilenceWeighting>(
      model_.trans_model, model_.feature_info.silence_weighting_config,
      model_.decodable_info->opts.frame_subsampling_factor);
}

std::string Recognizer::GetBestHypothesis(bool end_of_utt) const {
  return tiro_speech::GetBestHypothesis(decoder_, model_, end_of_utt);
}

bool Recognizer::GetResults(int32_t max_alternatives,
                            std::vector<AlignedWord>* best_aligned,
                            std::vector<std::string>* transcripts,
                            bool end_of_utt) {
  assert(transcripts != nullptr);
  assert(max_alternatives > 0);
  assert(!(max_alternatives > 1 && !end_of_utt));

  if (best_aligned != nullptr) best_aligned->clear();
  transcripts->clear();

  std::vector<kaldi::Lattice> lats(1);
  if (max_alternatives > 1 || model_.const_arpa_valid) {
    lats = GetShortestPaths(max_alternatives,
                            /* end_of_utt */ false);  // don't force final-probs
  } else {
    decoder_.GetBestPath(/* end_of_utt */ false, &lats[0]);
  }

  if (lats.empty()) {
    TIRO_SPEECH_WARN("GetShortestPaths returned no paths!");
    return false;
  }

  // TODO(rkjaran): Clean up KaldiModel interface.
  if (!GetWordAlignments(model_, lats[0], best_aligned)) {
    TIRO_SPEECH_WARN("GetWordAlignment failed.");
    return false;
  }

  TIRO_SPEECH_DEBUG("Results lattice size = {}", lats.size());
  std::vector<std::string> first_word_symbols{};
  transcripts->resize(lats.size());
  for (size_t idx = 0; idx < transcripts->size(); ++idx) {
    kaldi::LatticeWeight weight;
    std::vector<int32> alignment;
    std::vector<int32> words;
    if (!fst::GetLinearSymbolSequence(lats[idx], &alignment, &words, &weight)) {
      return false;
    }
    std::vector<std::string> word_symbols{};
    for (const auto word_id : words) {
      if (word_id == 0) continue;
      word_symbols.push_back(model_.word_syms->Find(word_id));
    }
    if (idx == 0) first_word_symbols = word_symbols;
    transcripts->at(idx) = Join(word_symbols, " ");
  }
  if (end_of_utt) left_context_words_ = first_word_symbols;
  return true;
}

std::vector<kaldi::Lattice> Recognizer::GetShortestPaths(
    int32_t n_max, bool end_of_utt, bool constarpa_rescoring) const {
  assert(n_max > 0);

  kaldi::CompactLattice clat;
  if (decoder_.NumFramesDecoded() == 0) return {};

  decoder_.GetLattice(end_of_utt, &clat);

  if (constarpa_rescoring && model_.const_arpa_valid) {
    TIRO_SPEECH_DEBUG("Rescoring with ConstArpaLM.");
    RescoreLattice(&clat, model_.const_arpa_lm);
  }

  kaldi::Lattice lat;
  fst::ConvertLattice(clat, &lat);
  TIRO_SPEECH_DEBUG("GetShortestPaths ... converted lattice has {}",
                    lat.NumStates());

  kaldi::Lattice nbest_lat;
  fst::ShortestPath(lat, &nbest_lat, n_max);

  std::vector<kaldi::Lattice> nbest_lats;
  fst::ConvertNbestToVector(nbest_lat, &nbest_lats);
  TIRO_SPEECH_DEBUG("GetShortestPaths ... nbest from from lattice is {}",
                    nbest_lats.size());

  return nbest_lats;
}

const std::vector<AlignedWord>& Recognizer::GetLeftContext() const {
  return left_context_;
}

bool GetWordAlignments(const KaldiModel& model, const kaldi::Lattice& lat,
                       std::vector<AlignedWord>* word_alignments) {
  word_alignments->clear();
  kaldi::CompactLattice clat;

  fst::ConvertLattice(lat, &clat);
  fst::RmEpsilon(&clat);
  kaldi::CompactLattice aligned_clat;
  if (!kaldi::WordAlignLattice(clat, model.trans_model,
                               model.word_boundary_info,
                               /* max_states */ 0, &aligned_clat)) {
    if (aligned_clat.NumStates() == 0) {
      TIRO_SPEECH_WARN("WordAlignLattice failed, zero output states.");
      return false;
    }
    TIRO_SPEECH_WARN("WordAlignLattice: Lattice might have been forced out!");
  }

  std::vector<int32_t> words, begin_times, lengths;  // these are #frames
  kaldi::CompactLatticeToWordAlignment(aligned_clat, &words, &begin_times,
                                       &lengths);
  for (size_t i = 0; i < words.size(); i++) {
    if (words[i] == 0) continue;
    auto start_time = FramesToMillis(model, begin_times[i]);
    auto duration = FramesToMillis(model, lengths[i]);
    word_alignments->emplace_back(start_time, duration,
                                  model.word_syms->Find(words[i]));
  }
  return word_alignments->size() > 0;
}

std::string GetBestHypothesis(const kaldi::SingleUtteranceNnet3Decoder& decoder,
                              const KaldiModel& model, bool end_of_utt) {
  kaldi::Lattice best_path;
  decoder.GetBestPath(end_of_utt, &best_path);
  std::string hyp;
  GetLinearSymbolSequenceString(model, best_path, &hyp);
  return hyp;
}

bool GetNbestWithConf(
    const kaldi::CompactLattice& clat, size_t n, const KaldiModel& model,
    std::vector<std::pair<std::string, float>>* nbest_with_conf) {
  std::vector<kaldi::Lattice> nbest_lats;
  kaldi::Lattice lat;
  fst::ConvertLattice(clat, &lat);
  fst::NbestAsFsts(lat, n, &nbest_lats);

  TIRO_SPEECH_DEBUG("Got {} paths.", nbest_lats.size());
  if (nbest_lats.size() == 0) return false;

  std::vector<float> costs;
  for (size_t i = 0; i < nbest_lats.size(); i++) {
    kaldi::LatticeWeight weights;
    std::vector<int32> alignments;
    std::vector<int32> words;
    if (!fst::GetLinearSymbolSequence(nbest_lats[i], &alignments, &words,
                                      &weights)) {
      continue;
    }
    std::stringstream ss;
    costs.push_back(fst::ConvertToCost(weights));
    for (size_t j = 0; j < words.size(); j++) {
      if (words[j] == 0) continue;
      ss << model.word_syms->Find(words[j]) << " ";
    }
    nbest_with_conf->emplace_back(ss.str(), 0);
  }

  if (costs.size() > 1) {
    (*nbest_with_conf)[0].second = 1 - kaldi::Exp(-(costs[1] - costs[0]));
  } else if (costs.size() == 0) {
    return false;
  } else {
    (*nbest_with_conf)[0].second = 1;
  }

  return true;
}

std::chrono::milliseconds FramesToMillis(const KaldiModel& model,
                                         int64_t num_frames) {
  return std::chrono::milliseconds{static_cast<int64_t>(
      num_frames * model.feature_info.FrameShiftInSeconds() * 1000 *
      model.decodable_info->opts.frame_subsampling_factor)};
}

float GetSampleRate(const kaldi::OnlineNnet2FeaturePipelineInfo& feature_info) {
  // TODO(rkjaran): handle for other feat types
  if (feature_info.feature_type == "mfcc") {
    return feature_info.mfcc_opts.frame_opts.samp_freq;
  } else if (feature_info.feature_type == "plp") {
    return feature_info.plp_opts.frame_opts.samp_freq;
  } else if (feature_info.feature_type == "fbank") {
    return feature_info.fbank_opts.frame_opts.samp_freq;
  }

  TIRO_SPEECH_ERROR("Unsupported feature type: ", feature_info.feature_type);
  return -1.0f;  // never
}

float GetSampleRate(const KaldiModel& model) {
  return GetSampleRate(model.feature_info);
}

bool GetLinearSymbolSequenceString(const KaldiModel& model,
                                   const kaldi::Lattice& lat,
                                   std::string* sequence) {
  kaldi::LatticeWeight weight;
  std::vector<int32> alignment;
  std::vector<int32> words;
  if (!fst::GetLinearSymbolSequence(lat, &alignment, &words, &weight)) {
    return false;
  }
  std::stringstream ss;
  std::for_each(begin(words), end(words), [&](int32_t word_id) {
    ss << model.word_syms->Find(word_id) << " ";
  });
  *sequence = ss.str();
  return true;
}

}  // namespace tiro_speech
