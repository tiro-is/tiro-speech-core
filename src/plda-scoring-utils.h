// A large portion of this is from ivectorbin/ivector-plda-scoring-dense.cc in
// the Kaldi project
//
// Copyright 2016-2018  David Snyder
//           2017-2018  Matthew Maciejewski
//           2021       Tiro ehf.
//
// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.
#ifndef TIRO_SPEECH_SRC_PLDA_SCORING_UTILS_H_
#define TIRO_SPEECH_SRC_PLDA_SCORING_UTILS_H_

#include <base/kaldi-common.h>
#include <ivector/plda.h>
#include <matrix/matrix-lib.h>

#include <vector>

namespace kaldi {

bool EstPca(const Matrix<BaseFloat> &ivector_mat, BaseFloat target_energy,
            Matrix<BaseFloat> *mat);

/// Transforms i-vectors using the PLDA model.
void TransformIvectors(const Matrix<BaseFloat> &ivectors_in,
                       const PldaConfig &plda_config, const Plda &plda,
                       Matrix<BaseFloat> *ivectors_out);

/// Transform the i-vectors using the recording-dependent PCA matrix.
void ApplyPca(const Matrix<BaseFloat> &ivectors_in,
              const Matrix<BaseFloat> &pca_mat,
              Matrix<BaseFloat> *ivectors_out);

void IvectorSubtractGlobalMean(const Vector<BaseFloat> &mean,
                               std::vector<Vector<BaseFloat>> &ivectors);

bool TransformVec(const Matrix<BaseFloat> &transform,
                  const Vector<BaseFloat> &vec, Vector<BaseFloat> *vec_out);

void IvectorNormalizeLength(std::vector<Vector<BaseFloat>> &ivectors,
                            bool scaleup = true, bool normalize = true);

/// Computes PLDA scores from pairs of {x,i}-vectors.  These scores are in the
/// form of an affinity matrix.
Matrix<BaseFloat> ScorePlda(const PldaConfig &plda_config, Plda this_plda,
                            Matrix<BaseFloat> whitening_matrix,
                            Vector<BaseFloat> centering_vector,
                            const std::vector<Vector<BaseFloat>> &ivectors,
                            float target_energy = 0.9f);

}  // namespace kaldi

#endif  // TIRO_SPEECH_SRC_PLDA_SCORING_UTILS_H_
