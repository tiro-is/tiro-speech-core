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
#include "src/plda-scoring-utils.h"

#include <matrix/matrix-lib.h>
#include <util/common-utils.h>
#include <util/stl-utils.h>

namespace kaldi {

bool EstPca(const Matrix<BaseFloat> &ivector_mat, BaseFloat target_energy,
            Matrix<BaseFloat> *mat) {
  // If the target_energy is 1.0, it's equivalent to not applying the
  // conversation-dependent PCA at all, so it's better to exit this
  // function before doing any computation.
  if (ApproxEqual(target_energy, 1.0, 0.001)) return false;

  int32 num_rows = ivector_mat.NumRows(), num_cols = ivector_mat.NumCols();
  Vector<BaseFloat> sum;
  SpMatrix<BaseFloat> sumsq;
  sum.Resize(num_cols);
  sumsq.Resize(num_cols);
  sum.AddRowSumMat(1.0, ivector_mat);
  sumsq.AddMat2(1.0, ivector_mat, kTrans, 1.0);
  sum.Scale(1.0 / num_rows);
  sumsq.Scale(1.0 / num_rows);
  sumsq.AddVec2(-1.0, sum);  // now sumsq is centered covariance.
  int32 full_dim = sum.Dim();

  Matrix<BaseFloat> P(full_dim, full_dim);
  Vector<BaseFloat> s(full_dim);

  try {
    if (num_rows > num_cols)
      sumsq.Eig(&s, &P);
    else
      Matrix<BaseFloat>(sumsq).Svd(&s, &P, NULL);
  } catch (...) {
    return false;
  }

  SortSvd(&s, &P);

  Matrix<BaseFloat> transform(P, kTrans);  // Transpose of P.  This is what
                                           // appears in the transform.

  // We want the PCA transform to retain target_energy amount of the total
  // energy.
  BaseFloat total_energy = s.Sum();
  BaseFloat energy = 0.0;
  int32 dim = 1;
  while (energy / total_energy <= target_energy) {
    energy += s(dim - 1);
    dim++;
  }
  Matrix<BaseFloat> transform_float(transform);
  mat->Resize(transform.NumCols(), transform.NumRows());
  mat->CopyFromMat(transform);
  mat->Resize(dim, transform_float.NumCols(), kCopyData);
  return true;
}

void TransformIvectors(const Matrix<BaseFloat> &ivectors_in,
                       const PldaConfig &plda_config, const Plda &plda,
                       Matrix<BaseFloat> *ivectors_out) {
  int32 dim = plda.Dim();
  ivectors_out->Resize(ivectors_in.NumRows(), dim);
  for (int32 i = 0; i < ivectors_in.NumRows(); i++) {
    Vector<BaseFloat> transformed_ivector(dim);
    plda.TransformIvector(plda_config, ivectors_in.Row(i), 1.0,
                          &transformed_ivector);
    ivectors_out->Row(i).CopyFromVec(transformed_ivector);
  }
}

void ApplyPca(const Matrix<BaseFloat> &ivectors_in,
              const Matrix<BaseFloat> &pca_mat,
              Matrix<BaseFloat> *ivectors_out) {
  int32 transform_cols = pca_mat.NumCols(), transform_rows = pca_mat.NumRows(),
        feat_dim = ivectors_in.NumCols();
  ivectors_out->Resize(ivectors_in.NumRows(), transform_rows);
  KALDI_ASSERT(transform_cols == feat_dim);
  ivectors_out->AddMatMat(1.0, ivectors_in, kNoTrans, pca_mat, kTrans, 0.0);
}

bool TransformVec(const Matrix<BaseFloat> &transform,
                  const Vector<BaseFloat> &vec, Vector<BaseFloat> *vec_out) {
  if (vec_out == nullptr) return false;

  int32 transform_rows = transform.NumRows(),
        transform_cols = transform.NumCols(), vec_dim = vec.Dim();

  vec_out->Resize(transform_rows);

  if (transform_cols == vec_dim) {
    vec_out->AddMatVec(1.0, transform, kNoTrans, vec, 0.0);
  } else {
    if (transform_cols != vec_dim + 1) {
      KALDI_ERR << "Dimension mismatch: input vector has dimension "
                << vec.Dim() << " and transform has " << transform_cols
                << " columns.";
    }
    vec_out->CopyColFromMat(transform, vec_dim);
    vec_out->AddMatVec(1.0, transform.Range(0, transform.NumRows(), 0, vec_dim),
                       kNoTrans, vec, 1.0);
  }

  return true;
}

void IvectorSubtractGlobalMean(const Vector<BaseFloat> &mean,
                               std::vector<Vector<BaseFloat>> &ivectors) {
  for (auto &ivector : ivectors) {
    ivector.AddVec(-1.0, mean);
  }
}

void IvectorNormalizeLength(std::vector<Vector<BaseFloat>> &ivectors,
                            bool scaleup, bool normalize) {
  for (auto &ivector : ivectors) {
    BaseFloat norm = ivector.Norm(2.0);
    BaseFloat ratio = norm / sqrt(ivector.Dim());  // how much larger it is
    // than it would be, in
    // expectation, if normally
    if (!scaleup) ratio = norm;

    if (ratio == 0.0) {
      KALDI_WARN << "Zero iVector";
    } else {
      if (normalize) ivector.Scale(1.0 / ratio);
    }
  }
}

Matrix<BaseFloat> ScorePlda(const PldaConfig &plda_config, Plda this_plda,
                            Matrix<BaseFloat> whitening_matrix,
                            Vector<BaseFloat> centering_vector,
                            const std::vector<Vector<BaseFloat>> &ivectors,
                            float target_energy) {
  std::vector<Vector<BaseFloat>> these_ivectors = ivectors;
  IvectorSubtractGlobalMean(centering_vector, these_ivectors);
  for (auto &ivector : these_ivectors) {
    Vector<float> transformed_ivector;
    TransformVec(whitening_matrix, ivector, &transformed_ivector);
    ivector.Swap(&transformed_ivector);
  }
  IvectorNormalizeLength(these_ivectors);

  Matrix<BaseFloat> ivector_mat(these_ivectors.size(), these_ivectors[0].Dim()),
      ivector_mat_pca, ivector_mat_plda, pca_transform,
      scores(these_ivectors.size(), these_ivectors.size());

  for (size_t i = 0; i < these_ivectors.size(); i++) {
    ivector_mat.Row(i).CopyFromVec(these_ivectors[i]);
  }

  if (EstPca(ivector_mat, target_energy, &pca_transform)) {
    // Apply the PCA transform to the raw i-vectors.
    ApplyPca(ivector_mat, pca_transform, &ivector_mat_pca);

    // Apply the PCA transform to the parameters of the PLDA model.
    this_plda.ApplyTransform(Matrix<double>(pca_transform));

    // Now transform the i-vectors using the reduced PLDA model.
    TransformIvectors(ivector_mat_pca, plda_config, this_plda,
                      &ivector_mat_plda);
  } else {
    // If EstPca returns false, we won't apply any PCA.
    TransformIvectors(ivector_mat, plda_config, this_plda, &ivector_mat_plda);
  }

  for (int32 i = 0; i < ivector_mat_plda.NumRows(); i++) {
    for (int32 j = 0; j < ivector_mat_plda.NumRows(); j++) {
      scores(i, j) = this_plda.LogLikelihoodRatio(
          Vector<double>(ivector_mat_plda.Row(i)), 1.0,
          Vector<double>(ivector_mat_plda.Row(j)));
    }
  }

  return scores;
}

}  // namespace kaldi
