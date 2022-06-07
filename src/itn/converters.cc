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
#include "src/itn/converters.h"

#include <cassert>

#include "src/itn/timing-weight.h"
#include "src/recognizer.h"

namespace tiro_speech::itn {

fst::VectorFst<fst::TimingArc> Convert(
    const std::vector<AlignedWord>& word_alis, const fst::SymbolTable& syms) {
  fst::VectorFst<fst::TimingArc> fst{};

  auto cur_state = fst.AddState();
  fst.SetStart(cur_state);

  auto space_id = syms.Find("<space>");
  assert(space_id != fst::kNoLabel);

  for (std::size_t idx = 0; idx < word_alis.size(); ++idx) {
    const auto& ali = word_alis[idx];
    auto next_state = fst.AddState();
    auto word_id = syms.Find(ali.word_symbol);
    fst::TimingWeight w(ali.start_time, ali.start_time + ali.duration);
    fst.AddArc(cur_state, fst::TimingArc(word_id, word_id, w, next_state));
    cur_state = next_state;

    next_state = fst.AddState();
    fst.AddArc(cur_state, fst::TimingArc(space_id, space_id,
                                         fst::TimingWeight::One(), next_state));
    cur_state = next_state;
  }
  fst.SetFinal(cur_state, fst::TimingWeight::One());
  return fst;
}

fst::VectorFst<fst::TimingArc> ConvertToByteFst(
    const std::vector<AlignedWord>& word_alis) {
  using Label = unsigned char;
  using Arc = fst::TimingArc;
  using Fst = fst::VectorFst<Arc>;
  constexpr unsigned char separator = ' ';

  Fst fst{};
  auto cur_state = fst.AddState();
  fst.SetStart(cur_state);

  for (const auto& ali : word_alis) {
    std::vector<Label> labels;
    fst::ByteStringToLabels(ali.word_symbol, &labels);

    fst.AddStates(labels.size() + 1);
    for (auto label : labels) {
      fst.AddArc(cur_state,
                 Arc{label, label, Arc::Weight::One(), cur_state + 1});
      ++cur_state;
    }

    // The weight, i.e. timing information, is attached to the space separator
    fst::TimingWeight w(ali.start_time, ali.start_time + ali.duration);
    fst.AddArc(cur_state, Arc{separator, separator, w, cur_state + 1});
    ++cur_state;
  }

  fst.SetFinal(cur_state, Arc::Weight::One());
  fst.SetProperties(fst::kCompiledStringProperties,
                    fst::kCompiledStringProperties);

  fst::TopSort(&fst);
  return fst;
}

fst::VectorFst<fst::StdArc> ConvertToTropical(
    const std::vector<AlignedWord>& word_alis, const fst::SymbolTable& syms) {
  fst::VectorFst<fst::StdArc> fst{};

  auto cur_state = fst.AddState();
  fst.SetStart(cur_state);

  auto space_id = syms.Find("<space>");
  assert(space_id != fst::kNoLabel);

  for (std::size_t idx = 0; idx < word_alis.size(); ++idx) {
    const auto& ali = word_alis[idx];
    auto next_state = fst.AddState();
    auto word_id = syms.Find(ali.word_symbol);
    fst.AddArc(cur_state, fst::StdArc(word_id, word_id,
                                      fst::StdArc::Weight::One(), next_state));
    cur_state = next_state;

    // Note that this adds a <space> as the last symbols, which is required by
    // our rewrite grammars
    next_state = fst.AddState();
    fst.AddArc(cur_state, fst::StdArc(space_id, space_id,
                                      fst::StdArc::Weight::One(), next_state));
    cur_state = next_state;
  }
  fst.SetFinal(cur_state, fst::StdArc::Weight::One());

  fst::TopSort(&fst);
  return fst;
}

fst::VectorFst<fst::StdArc> ConvertToTropicalByteFst(
    const std::vector<AlignedWord>& word_alis) {
  using Label = unsigned char;
  using Arc = fst::StdArc;
  using Fst = fst::VectorFst<Arc>;
  constexpr unsigned char separator = ' ';

  Fst fst{};

  auto cur_state = fst.AddState();
  fst.SetStart(cur_state);

  for (const auto& ali : word_alis) {
    std::vector<Label> labels;
    fst::ByteStringToLabels(ali.word_symbol, &labels);

    fst.AddStates(labels.size() + 1);
    for (auto label : labels) {
      fst.AddArc(cur_state, Arc{label, label, cur_state + 1});
      ++cur_state;
    }

    fst.AddArc(cur_state, Arc{separator, separator, cur_state + 1});
    ++cur_state;
  }

  fst.SetFinal(cur_state, Arc::Weight::One());
  fst.SetProperties(fst::kCompiledStringProperties,
                    fst::kCompiledStringProperties);

  fst::TopSort(&fst);
  return fst;
}

std::vector<AlignedWord> Convert(const fst::VectorFst<fst::TimingArc>& fst) {
  std::vector<AlignedWord> word_alis{};

  AlignedWord current_ali{};
  auto current_weight = fst::TimingWeight::One();

  for (fst::StateIterator siter(fst); !siter.Done(); siter.Next()) {
    auto state_id = siter.Value();
    for (fst::ArcIterator aiter(fst, state_id); !aiter.Done(); aiter.Next()) {
      const auto& arc = aiter.Value();

      if (arc.weight != fst::TimingWeight::One()) {
        if (current_weight == fst::TimingWeight::One()) {
          current_weight = arc.weight;
        } else {
          current_weight = fst::Times(current_weight, arc.weight);
        }
      }

      // A space begins a new ali
      if (arc.olabel == ' ') {
        current_ali.start_time =
            static_cast<std::int64_t>(current_weight.Value1().Value());
        current_ali.duration = static_cast<std::int64_t>(
            current_weight.Value2().Value() - current_weight.Value1().Value());
        word_alis.push_back(current_ali);
        current_ali = AlignedWord{};
        current_weight = fst::TimingWeight::One();
      } else if (arc.olabel != 0) {
        current_ali.word_symbol.append(1, arc.olabel);
      }
    }
  }

  return word_alis;
}

}  // namespace tiro_speech::itn
