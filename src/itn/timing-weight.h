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
#ifndef TIRO_SPEECH_SRC_ITN_TIMING_WEIGHT_H_
#define TIRO_SPEECH_SRC_ITN_TIMING_WEIGHT_H_

#include <fst/arc-map.h>
#include <fst/arc.h>
#include <fst/product-weight.h>
#include <fst/util.h>
#include <fst/weight.h>

#include "src/itn/maxmin-weight.h"

namespace fst {

/// Semiring defined by:
/// W_t = (s, e) : s, e ∈ R + ∪ {0, ∞}}
///   (s1, e1) + (s2, e2) = (max(s1, s2), min(e1, e2))
///   (s1, e1) x (s2, e2) = (min(s1, s2), max(e1, e2))
///   Zero() = (0, inf)
///   One() = (inf, 0)
/// Label: word
/// W_{raw:best} = RmEps(AnyPath(W ◦ W_{best}))
/// W̃_{best} = S̃ ◦ Map_t (W_{raw:best} )
using TimingWeight = ::fst::ProductWeight<MaxMinWeight, MinMaxWeight>;
using TimingArc = ::fst::ArcTpl<TimingWeight>;
using TimingFst = ::fst::VectorFst<TimingArc>;

// NOTE: Just convert everything to One()s
template <>
struct WeightConvert<TropicalWeight, TimingWeight> {
  TimingWeight operator()(const TropicalWeight& /* w */) const {
    return TimingWeight::One();
  }
};

template <>
struct WeightConvert<TimingWeight, TropicalWeight> {
  const TropicalWeight operator()(const TimingWeight& /* w */) const {
    return TropicalWeight::One();
  }
};

}  // end namespace fst

#endif  // TIRO_SPEECH_SRC_ITN_TIMING_WEIGHT_H_
