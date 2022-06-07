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
#ifndef TIRO_SPEECH_SRC_ITN_CONVERTERS_H_
#define TIRO_SPEECH_SRC_ITN_CONVERTERS_H_

#include <fst/fst.h>

#include <string>
#include <vector>

#include "src/aligned-word.h"
#include "src/itn/timing-weight.h"

namespace tiro_speech::itn {

/**
 * Convert from a vector of aligned words to a Timing FST
 *
 * This Timing FST is defined here as a linear an acceptor that encodes the
 * word, start time and end time
 *
 * The symbol table \p syms has to include the symbol '<space>'
 *
 */
fst::VectorFst<fst::TimingArc> Convert(
    const std::vector<AlignedWord>& word_alis, const fst::SymbolTable& syms);

/**
 * Convert from a vector of aligned words to a Timing byte string FST
 *
 * This Timing FST is defined here as a linear acceptor that encodes each word
 * as a string of bytes ending with a separator character (' ').
 */
fst::VectorFst<fst::TimingArc> ConvertToByteFst(
    const std::vector<AlignedWord>& word_alis);

/**
 * Convert from a vector of aligned words to an unweighted StdFst
 *
 * The symbol table \p syms has to include the symbol '<space>'
 */
fst::VectorFst<fst::StdArc> ConvertToTropical(
    const std::vector<AlignedWord>& word_alis, const fst::SymbolTable& syms);

/**
 * Convert from a vector of aligned words into an unweighted StdFst of bytes
 */
fst::VectorFst<fst::StdArc> ConvertToTropicalByteFst(
    const std::vector<AlignedWord>& word_alis);

/**
 *  Convert from a linear TimingFst to a vector of word aligments
 *
 * This assumes that olabels in \p fst are of type 'byte' and that each arc
 * with a non-unity weight begins a new word.
 *
 * E.g. the linear FST:
 *
 *     0 -A (0,100)-> 1 -B-> 2 -C (105,200)-> 3 -D-> 4
 *
 * Contains two words: AB, starting at 0 ms and ending at 100 ms, and CD,
 * starting at 105 ms and ending at 200 ms.
 */
std::vector<AlignedWord> Convert(const fst::VectorFst<fst::TimingArc>& fst);

/**
 * Walk an acyclic FST whose olabels are bytes and convert the output labels to
 * a string.
 */
template <typename Fst>
std::string ByteFstToString(const Fst& ifst) {
  std::string output_string = "";
  for (fst::StateIterator siter(ifst); !siter.Done(); siter.Next()) {
    auto state_id = siter.Value();
    for (fst::ArcIterator aiter(ifst, state_id); !aiter.Done(); aiter.Next()) {
      const auto& arc = aiter.Value();
      if (arc.olabel != 0) {
        output_string.append(1, static_cast<char>(arc.olabel));
      }
    }
  }
  return output_string;
}

}  // namespace tiro_speech::itn

#endif  // TIRO_SPEECH_SRC_ITN_CONVERTERS_H_
