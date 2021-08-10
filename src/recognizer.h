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
#ifndef TIRO_SPEECH_SRC_RECOGNIZER_H_
#define TIRO_SPEECH_SRC_RECOGNIZER_H_

#include <matrix/kaldi-vector.h>
#include <online2/online-ivector-feature.h>
#include <online2/online-nnet3-decoding.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/base.h"
#include "src/kaldi-model.h"
#include "src/options.h"
#include "src/utils.h"

namespace tiro_speech {

using VectorBase = kaldi::VectorBase<float>;

struct AlignedWord {
  std::int64_t start_time;
  std::int64_t duration;
  std::string word_symbol;

  AlignedWord(std::chrono::milliseconds start_time,
              std::chrono::milliseconds duration, std::string word_symbol)
      : start_time{start_time.count()},
        duration{duration.count()},
        word_symbol{word_symbol} {};

  AlignedWord() : start_time{0}, duration{0}, word_symbol{""} {}

  friend std::ostream& operator<<(std::ostream& os, const AlignedWord& ali) {
    os << "WordAlignment{" << ali.word_symbol << ", " << ali.start_time << ", "
       << ali.duration << "}";
    return os;
  }
};

class Recognizer final {
 public:
  explicit Recognizer(const KaldiModel& model);

  Recognizer(const KaldiModel& model,
             const KaldiModel::AdaptationState& adaptation_state);

  Recognizer(const KaldiModel& model,
             const KaldiModel::AdaptationState& adaptation_state,
             std::vector<AlignedWord> left_context);

  void SetAdaptationState(const KaldiModel::AdaptationState& adaptation_state);

  KaldiModel::AdaptationState GetAdaptationState() const;

  void ReadAdaptationState(std::istream& is, bool binary);

  void WriteAdaptationState(std::ostream& os, bool binary);

  /**
   * Decode the given waveform. `GetBestHypothesis()` returns the result.
   */
  void Decode(const VectorBase& waveform, bool flush = false);

  bool HasEndpoint(bool single_utterance = false);

  int32_t NumFramesDecoded() const;

  /**\brief Call this when no more calls to Decode() will be made.
   */
  void Finalize();

  /**
   * The pair of methods EndSegment() and InitSegment() can be used for long
   * form recognition.
   *
   * Example usage:
   * \code
   * while (more_data) {
   *   r.InitSegment();
   *   r.Decode(data_chunkPtr);
   *   if (r.DetectEndpoint()) {
   *     r.EndSegment();
   *     r.GetResults(...);
   *     break;
   *   }
   * }
   * \endcode
   */
  void EndSegment();
  void InitSegment();

  std::string GetBestHypothesis(bool end_of_utt = true) const;

  /**\brief Try to get the best transcripts and possibly a time alignment for
   * the best one.
   *
   * \param[in]  max_alternatives  Maximum number of alternative transcripts to
   *                               get.
   * \param[out] best_aligned If model supports it; Time aligned words for the
   *                          best transcript (i.e. first element of
   *                          transcripts).  Will not do alignment if nullptr.
   * \param[out] transcripts Best transcripts, 0 <= size <= #max_alternatives.
   *
   * \return false if it fails to get any transcripts.
   */
  bool GetResults(int32_t max_alternatives,
                  std::vector<AlignedWord>* best_aligned,
                  std::vector<std::string>* transcripts, bool end_of_utt = true,
                  bool punctuate = false);

  std::vector<kaldi::Lattice> GetShortestPaths(
      int32_t n_max, bool end_of_utt = true,
      bool constarpa_rescoring = true) const;

  /**\brief Get the recognizer \em active left context
   *
   * The Recognizer's active left context is updated for each call to
   * GetResults() where end_of_utt is true. This meant to be used when the a
   * Recognizer object is not being reused for multiple consecutive segments. In
   * that case we initialize the next Recognizer object with the returned left
   * context.
   *
   * \return A reference to the active left context
   */
  const std::vector<AlignedWord>& GetLeftContext() const;

 private:
  const KaldiModel& model_;
  mutable KaldiModel::AdaptationState adaptation_state_;
  kaldi::OnlineNnet2FeaturePipeline feature_pipeline_;
  std::unique_ptr<kaldi::OnlineSilenceWeighting> silence_weighting_;
  std::vector<std::pair<int32, float>> delta_weights_;
  kaldi::SingleUtteranceNnet3Decoder decoder_;
  float sample_rate_;
  std::vector<AlignedWord> left_context_{};
  std::vector<std::string> left_context_words_{};
  std::int32_t frame_offset_{0};
};

/**\brief Attempt to do word time alignment on the lattice lat.
 *
 * \param[in] model  Model used to obtain the lattice lat
 * \param[in] lat    Lattice to be aligned
 * \param[out] word_alignments  Word time alignment of lat, or empty vector if
 *                              the alignment failed.  Can't be null.
 *
 * \return true on success and false if the alignment failed for some reason.
 */
bool GetWordAlignments(const KaldiModel& model, const kaldi::Lattice& lat,
                       std::vector<AlignedWord>* word_alignments);

std::string GetBestHypothesis(const kaldi::SingleUtteranceNnet3Decoder& decoder,
                              const KaldiModel& model, bool end_of_utt = false);

bool GetNbestWithConf(
    const kaldi::CompactLattice& clat, size_t n, const KaldiModel& model,
    std::vector<std::pair<std::string, float>>* nbest_with_conf);

std::chrono::milliseconds FramesToMillis(const KaldiModel& model,
                                         int64_t num_frames);

float GetSampleRate(const kaldi::OnlineNnet2FeaturePipelineInfo& feature_info);

float GetSampleRate(const KaldiModel& model);

/**
 * Returns false if lattice isn't linear.
 */
bool GetLinearSymbolSequenceString(const KaldiModel& model,
                                   const kaldi::Lattice& lat,
                                   std::string* sequence);

}  // namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_RECOGNIZER_H_
