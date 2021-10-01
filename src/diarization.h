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
#ifndef TIRO_SPEECH_SRC_DIARIZATION_H_
#define TIRO_SPEECH_SRC_DIARIZATION_H_

#include <feat/feature-functions.h>
#include <feat/feature-mfcc.h>
#include <feat/online-feature.h>
#include <ivector/plda.h>
#include <ivector/voice-activity-detection.h>
#include <matrix/matrix-lib.h>
#include <nnet3/nnet-am-decodable-simple.h>
#include <nnet3/nnet-utils.h>
#include <util/common-utils.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "src/options.h"

namespace tiro_speech {

kaldi::nnet3::Nnet ReadInferenceNnet(const std::string &filename,
                                     const std::string &config_filename);

struct XvectorNnetOptions {
  std::string nnet_rxfilename = "exp/xvector_nnet_1a/final.raw";
  std::string extract_config_rxfilename = "exp/xvector_nnet_1a/extract.config";
  std::int32_t chunk_size = -1;
  std::int32_t min_chunk_size = 100;

  kaldi::nnet3::NnetSimpleComputationOptions nnet_opts;
  kaldi::nnet3::CachingOptimizingCompilerOptions compiler_opts;

  void Register(OptionsItf *opts) {
    opts->Register("nnet-rxfilename", &nnet_rxfilename, "Xvector raw nnet3.");
    opts->Register("extract-config-rxfilename", &extract_config_rxfilename,
                   "Config edits for Xvector model.");
    opts->Register(
        "chunk-size", &chunk_size,
        "Extracts xvectors from specified chunk-size, and averages.");
    opts->Register("min-chunk-size", &min_chunk_size,
                   "Minimum chunk-size allowed when extracting xvectors.");
    nnet_opts.Register(opts);
    compiler_opts.Register(opts);
  }
};

class XvectorNnet {
 public:
  using Xvector = kaldi::Vector<float>;

  explicit XvectorNnet(const XvectorNnetOptions &opts)
      : nnet_{ReadInferenceNnet(opts.nnet_rxfilename,
                                opts.extract_config_rxfilename)},
        compiler_{nnet_, opts.nnet_opts.optimize_config, opts.compiler_opts},
        xvector_dim_{nnet_.OutputDim("output")},
        chunk_size_{opts.chunk_size},
        min_chunk_size_{opts.min_chunk_size} {}

  // Not const because of caching
  Xvector Compute(const kaldi::MatrixBase<float> &features,
                  float window_seconds = 1.5f,
                  float period_seconds = 0.75f) const;

  void RunNnetComputation(const kaldi::MatrixBase<float> &features,
                          kaldi::Vector<float> *xvector) const;

 private:
  kaldi::nnet3::Nnet nnet_;
  mutable kaldi::nnet3::CachingOptimizingCompiler compiler_;
  std::int32_t xvector_dim_;
  std::int32_t chunk_size_;
  std::int32_t min_chunk_size_;
};

struct XvectorDiarizationDecoderOptions {
  kaldi::VadEnergyOptions vad_opts;
  kaldi::MfccOptions mfcc_opts;
  kaldi::SlidingWindowCmnOptions cmn_opts;
  XvectorNnetOptions xvector_nnet_opts;

  kaldi::PldaConfig plda_opts;
  std::string plda_filename;
  std::string centering_vector_filename;
  std::string whitening_matrix_filename;

  XvectorDiarizationDecoderOptions() {
    // Set different defaults
    cmn_opts.cmn_window = 300;
    cmn_opts.center = true;
    cmn_opts.normalize_variance = false;
  }

  void Register(OptionsItf *opts) {
    ParseOptions vad_po{"vad", opts};
    vad_opts.Register(&vad_po);

    ParseOptions mfcc_po{"mfcc", opts};
    mfcc_opts.Register(&mfcc_po);

    ParseOptions xvector_po{"xvector", opts};
    xvector_nnet_opts.Register(&xvector_po);

    ParseOptions plda_po{"plda", opts};
    plda_opts.Register(&plda_po);
    opts->Register("plda", &plda_filename, "PLDA model.");
    opts->Register("centering-vector", &centering_vector_filename,
                   "Vector for centering");
    opts->Register("whitening-matrix", &whitening_matrix_filename,
                   "Whitening matrix");
  }
};

struct XvectorDiarizationDecoderInfo {
  XvectorDiarizationDecoderOptions opts;
  XvectorNnet xvector_nnet;
  kaldi::Plda plda;
  kaldi::Vector<float> centering_vector;
  kaldi::Matrix<float> whitening_matrix;

  explicit XvectorDiarizationDecoderInfo(XvectorDiarizationDecoderOptions opts_)
      : opts{std::move(opts_)}, xvector_nnet{opts.xvector_nnet_opts}, plda{} {
    ReadKaldiObject(opts.plda_filename, &plda);
    kaldi::ReadKaldiObject(opts.centering_vector_filename, &centering_vector);
    kaldi::ReadKaldiObject(opts.whitening_matrix_filename, &whitening_matrix);
  }
};

/**
 * Segment frames by VAD decisions in \p voiced
 *
 * \returns a vector of pairs of {start frame, end frame}
 */
std::vector<std::pair<std::int32_t, std::int32_t>> SegmentByVad(
    const kaldi::Vector<float> &voiced, float silence_proportion = 0.2f);

struct DiarizationSegment {
  std::int32_t spk_id;
  std::int32_t start_frame;
  std::int32_t end_frame;
};

/**
 * Compute speaker identities for each feature frame in \p feats
 */
std::vector<DiarizationSegment> ComputeXvectorDiarization(
    const XvectorDiarizationDecoderInfo &info,
    const kaldi::Matrix<float> &feats, int num_speakers);

class XvectorDiarizationDecoder {
 public:
  explicit XvectorDiarizationDecoder(const XvectorDiarizationDecoderInfo &info)
      : info_{info}, online_features_{info.opts.mfcc_opts} {}

  void AcceptWaveform(float sample_freq,
                      const kaldi::VectorBase<float> &waveform);
  void InputFinished();

  float FrameShiftInSeconds() const {
    return online_features_.FrameShiftInSeconds();
  }

  /**
   * Compute speaker identities for each feature frame in \p feats
   */
  std::vector<DiarizationSegment> Compute(int num_speakers);

 private:
  const XvectorDiarizationDecoderInfo &info_;
  kaldi::OnlineMfcc online_features_;
};

/**
 * Compute MFCCs from waveform
 *
 * This will resample if necessary when mfcc_opts.allow_downsample and/or
 * mfcc_opts.allow_upsample are true
 */
kaldi::Matrix<float> WaveformToMfcc(const kaldi::MfccOptions &mfcc_opts,
                                    const kaldi::Vector<float> &waveform,
                                    float sample_freq);

}  // namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_DIARIZATION_H_
