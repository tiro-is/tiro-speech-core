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
#include "src/diarization.h"

#include <base/kaldi-common.h>
#include <ivector/agglomerative-clustering.h>
#include <matrix/matrix-lib.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "src/plda-scoring-utils.h"

namespace tiro_speech {

kaldi::nnet3::Nnet ReadInferenceNnet(const std::string &filename,
                                     const std::string &config_filename) {
  kaldi::nnet3::Nnet nnet;
  kaldi::ReadKaldiObject(filename, &nnet);
  bool binary;
  kaldi::Input is{config_filename, &binary};
  nnet.ReadConfig(is.Stream());
  SetBatchnormTestMode(true, &nnet);
  SetDropoutTestMode(true, &nnet);
  CollapseModel(kaldi::nnet3::CollapseModelConfig(), &nnet);
  return nnet;
}

XvectorNnet::Xvector XvectorNnet::Compute(
    const kaldi::MatrixBase<float> &features, float window_seconds,
    float period_seconds) const {
  std::int32_t num_rows = features.NumRows();
  std::int32_t feat_dim = features.NumCols();
  std::int32_t this_chunk_size = chunk_size_;

  if (num_rows < chunk_size_ || chunk_size_ == -1) {
    this_chunk_size = num_rows;
  }

  std::int32_t num_chunks =
      std::ceil(num_rows / static_cast<float>(this_chunk_size));

  Xvector xvector_avg{xvector_dim_, kaldi::kSetZero};
  float tot_weight = 0.0;

  // Iterate over the feature chunks.
  for (std::int32_t chunk_indx = 0; chunk_indx < num_chunks; chunk_indx++) {
    // If we're nearing the end of the input, we may need to shift the
    // offset back so that we can get this_chunk_size frames of input to
    // the nnet.
    std::int32_t offset =
        std::min(this_chunk_size, num_rows - chunk_indx * this_chunk_size);
    kaldi::SubMatrix<float> sub_features{features, chunk_indx * this_chunk_size,
                                         offset, 0, feat_dim};
    kaldi::Vector<float> xvector;
    tot_weight += offset;

    // Pad input if the offset is less than the minimum chunk size
    if (offset < min_chunk_size_) {
      kaldi::Matrix<float> padded_features{min_chunk_size_, feat_dim};
      std::int32_t left_context = (min_chunk_size_ - offset) / 2;
      std::int32_t right_context = min_chunk_size_ - offset - left_context;
      for (std::int32_t i = 0; i < left_context; i++) {
        padded_features.Row(i).CopyFromVec(sub_features.Row(0));
      }
      for (std::int32_t i = 0; i < right_context; i++) {
        padded_features.Row(min_chunk_size_ - i - 1)
            .CopyFromVec(sub_features.Row(offset - 1));
      }
      padded_features.Range(left_context, offset, 0, feat_dim)
          .CopyFromMat(sub_features);
      RunNnetComputation(padded_features, &xvector);
    } else {
      RunNnetComputation(sub_features, &xvector);
    }
    xvector_avg.AddVec(offset, xvector);
  }
  xvector_avg.Scale(1.0 / tot_weight);

  return xvector_avg;
}

void XvectorNnet::RunNnetComputation(const kaldi::MatrixBase<float> &features,
                                     kaldi::Vector<float> *xvector) const {
  kaldi::nnet3::ComputationRequest request;
  request.need_model_derivative = false;
  request.store_component_stats = false;
  request.inputs.push_back(
      kaldi::nnet3::IoSpecification("input", 0, features.NumRows()));
  kaldi::nnet3::IoSpecification output_spec;
  output_spec.name = "output";
  output_spec.has_deriv = false;
  output_spec.indexes.resize(1);
  request.outputs.resize(1);
  request.outputs[0].Swap(&output_spec);
  std::shared_ptr<const kaldi::nnet3::NnetComputation> computation(
      compiler_.Compile(request));
  kaldi::nnet3::Nnet *nnet_to_update = nullptr;  // we're not doing any update.
  kaldi::nnet3::NnetComputer computer(kaldi::nnet3::NnetComputeOptions(),
                                      *computation, nnet_, nnet_to_update);
  kaldi::CuMatrix<float> input_feats_cu(features);
  computer.AcceptInput("input", &input_feats_cu);
  computer.Run();
  kaldi::CuMatrix<float> cu_output;
  computer.GetOutputDestructive("output", &cu_output);
  xvector->Resize(cu_output.NumCols());
  xvector->CopyFromVec(cu_output.Row(0));
}

std::vector<std::pair<std::int32_t, std::int32_t>> SegmentByVad(
    const kaldi::Vector<float> &voiced, float silence_proportion) {
  auto &A = voiced;
  auto N = voiced.Dim();
  kaldi::Vector<float> S(N);
  kaldi::Vector<float> E(N + 1);

  for (auto n = 0; n < N; ++n) {
    if (A(n) == 0) {
      if (n > 0 && !kaldi::ApproxEqual(A(n - 1), 0)) {
        E(n) = 1;
      }
    } else {
      if (n == 0 || kaldi::ApproxEqual(A(n - 1), 0)) {
        S(n) = 1;
      }
    }
  }
  if (!kaldi::ApproxEqual(A(N - 1), 0)) {
    E(N) = 1;
  }

  std::int32_t num_nonsil_frames = 0;
  bool in_segment;

  std::vector<std::int32_t> active_frames;
  for (std::int32_t n = 0; n <= N; ++n) {
    if (n < N && kaldi::ApproxEqual(S(n), 1)) {
      in_segment = true;
      active_frames.push_back(n);
    }
    if (kaldi::ApproxEqual(E(n), 1)) {
      in_segment = false;
    }
    if (n < N) {
      if (in_segment) {
        num_nonsil_frames++;
      }
    }
  }

  std::int32_t target_segment_frames =
      num_nonsil_frames / (1.0 - silence_proportion);
  std::int32_t num_segment_frames = num_nonsil_frames;
  while (num_segment_frames < target_segment_frames) {
    bool changed = false;
    for (std::size_t i = 0; i < active_frames.size(); ++i) {
      auto n = active_frames[i];
      if (kaldi::ApproxEqual(E(n), 1) && n < N &&
          !kaldi::ApproxEqual(S(n), 1)) {
        E(n) = 0;
        E(n + 1) = 1;
        active_frames[i] = n + 1;
        num_segment_frames++;
        changed = true;
      }
      if (n < N && kaldi::ApproxEqual(S(n), 1) && n > 0 &&
          !kaldi::ApproxEqual(E(n), 1)) {
        S(n) = 0;
        S(n - 1) = 1;
        active_frames[i] = n - 1;
        num_segment_frames++;
        changed = true;
      }
      if (num_segment_frames == target_segment_frames) {
        break;
      }
    }
    if (!changed) {
      break;
    }
  }

  std::vector<std::pair<std::int32_t, std::int32_t>> voiced_segments;
  for (std::int32_t n = 0; n < N; ++n) {
    assert(!(E(n) == 1 && S(n) != 1));
    if (S(n) == 1) {
      std::int32_t p;
      for (p = n + 1; p < N && !kaldi::ApproxEqual(E(p), 1); ++p) {
        assert(S(p) == 0);
      }
      assert(E(p) == 1);
      voiced_segments.emplace_back(n, p);
      if (p < N && kaldi::ApproxEqual(S(p), 1)) {
        n = p - 1;
      } else {
        n = p;
      }
    }
  }
  return voiced_segments;
}

std::vector<DiarizationSegment> ComputeXvectorDiarization(
    const XvectorDiarizationDecoderInfo &info,
    const kaldi::Matrix<float> &feats, int num_speakers) {
  std::int32_t num_rows = feats.NumRows();
  std::int32_t feat_dim = feats.NumCols();

  kaldi::Vector<float> voiced;
  kaldi::ComputeVadEnergy(info.opts.vad_opts, feats, &voiced);

  kaldi::Matrix<float> cmvn_feat(feats.NumRows(), feats.NumCols(),
                                 kaldi::kUndefined);
  kaldi::SlidingWindowCmn(info.opts.cmn_opts, feats, &cmvn_feat);

  const auto segmentation = SegmentByVad(voiced);
  // TODO(rkjaran): Create overlapping segments. "The speaker boundary between
  // two overlapping segments by different speakers is placed at the midpoint
  // between the end of the first segment and the start of the second
  // segment." from make_rttm.pyq

  std::vector<kaldi::Vector<float>> xvectors;
  for (auto [start, end] : segmentation) {
    xvectors.push_back(info.xvector_nnet.Compute(
        kaldi::SubMatrix<float>(feats, start, end - start, 0, feat_dim)));
  }

  if (segmentation.size() < 2) {
    return {
        DiarizationSegment{1, segmentation[0].first, segmentation[0].second}};
  }

  kaldi::Matrix<float> plda_scores =
      kaldi::ScorePlda(info.opts.plda_opts, kaldi::Plda{info.plda},
                       info.whitening_matrix, info.centering_vector, xvectors);
  plda_scores.Scale(-1);

  const float max_spk_fraction = 1.0;
  const float first_pass_max_utterances =
      std::numeric_limits<std::int16_t>::max();

  std::vector<std::int32_t> spk_ids;
  if (1.0 / num_speakers <= max_spk_fraction && max_spk_fraction <= 1.0) {
    AgglomerativeCluster(plda_scores, std::numeric_limits<float>::max(),
                         num_speakers, first_pass_max_utterances,
                         max_spk_fraction, &spk_ids);
  } else {
    AgglomerativeCluster(plda_scores, std::numeric_limits<float>::max(),
                         num_speakers, first_pass_max_utterances, 1.0,
                         &spk_ids);
  }

  std::vector<DiarizationSegment> diarization_segments;
  for (std::size_t idx = 0; idx < spk_ids.size(); ++idx) {
    diarization_segments.push_back(DiarizationSegment{
        spk_ids[idx], segmentation[idx].first, segmentation[idx].second});
  }
  return diarization_segments;
}

void XvectorDiarizationDecoder::AcceptWaveform(
    float sample_freq, const kaldi::VectorBase<float> &waveform) {
  online_features_.AcceptWaveform(sample_freq, waveform);
}

void XvectorDiarizationDecoder::InputFinished() {
  online_features_.InputFinished();
}

std::vector<DiarizationSegment> XvectorDiarizationDecoder::Compute(
    int num_speakers) {
  kaldi::Matrix<float> feats(online_features_.NumFramesReady(),
                             online_features_.Dim());
  // just get all available frames
  for (std::int32_t frame_idx = 0;
       frame_idx < online_features_.NumFramesReady(); ++frame_idx) {
    kaldi::SubVector<float> feat{feats, frame_idx};
    online_features_.GetFrame(frame_idx, &feat);
  }

  return ComputeXvectorDiarization(info_, feats, num_speakers);
}

kaldi::Matrix<float> WaveformToMfcc(const kaldi::MfccOptions &mfcc_opts,
                                    const kaldi::Vector<float> &waveform,
                                    float sample_freq) {
  kaldi::OnlineMfcc online_features{mfcc_opts};
  online_features.AcceptWaveform(sample_freq, waveform);
  online_features.InputFinished();
  kaldi::Matrix<float> feats(online_features.NumFramesReady(),
                             online_features.Dim());
  // just get all available frames
  for (std::int32_t frame_idx = 0; frame_idx < online_features.NumFramesReady();
       ++frame_idx) {
    kaldi::SubVector<float> feat(feats, frame_idx);
    online_features.GetFrame(frame_idx, &feat);
  }
  return feats;
}

}  // namespace tiro_speech
