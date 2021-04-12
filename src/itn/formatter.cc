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
#include "src/itn/formatter.h"

namespace tiro_speech::itn {

namespace {

/**
 * Use FST to format
 *
 * The input FST \p ifst is an acceptor of word symbols. The output FST \p
 * ofst will have those same word symbols on the input and the corresponding
 * rewritten/formatted bytestring on the output.
 */
void Format(LookAheadFormatter::InFst ifst,
            const LookAheadFormatter::LabelPairs& relabel_pairs,
            const fst::StdILabelLookAheadFst& rewrite_fst,
            LookAheadFormatter::OutFst* ofst) {
  // ifst is passed by value (and thus copied) because we have to mutate it
  // any way.
  fst::Relabel(&ifst, {}, relabel_pairs);
  fst::ArcSort(&ifst, fst::OLabelCompare<LookAheadFormatter::Arc>{});
  fst::Compose(ifst, rewrite_fst, ofst);
}

}  // namespace

LookAheadFormatter::LookAheadFormatter(const LookAheadFormatterConfig& opts) {
  if (!fst::ReadLabelPairs(opts.relabel_pairs_filename, &relabel_pairs_)) {
    throw std::runtime_error{
        fmt::format("Could not read relabel pairs from file '{}'",
                    opts.relabel_pairs_filename)};
  }
  rewrite_fst_.reset(fst::StdILabelLookAheadFst::Read(opts.fst_filename));
  if (rewrite_fst_ == nullptr) {
    throw std::runtime_error{fmt::format(
        "Could not read look ahead FST from file '{}'", opts.fst_filename)};
  }
}

std::vector<AlignedWord> LookAheadFormatter::FormatWords(
    const fst::SymbolTable& isyms,
    const std::vector<AlignedWord>& words) const {
  fst::VectorFst<fst::StdArc> words_fst = ConvertToTropical(words, isyms);
  fst::VectorFst<fst::StdArc> formatted_words_fst{};
  Format(words_fst, relabel_pairs_, *rewrite_fst_, &formatted_words_fst);

  fst::TimingFst formatted_time_fst{};
  fst::ArcMap(formatted_words_fst, &formatted_time_fst,
              fst::WeightConvertMapper<fst::StdArc, fst::TimingArc>{});

  fst::TimingFst timing_symbol_fst = Convert(words, isyms);
  fst::TimingFst timing_byte_fst{};
  fst::Compose(timing_symbol_fst, formatted_time_fst, &timing_byte_fst);

  return Convert(timing_byte_fst);
}

}  // namespace tiro_speech::itn
