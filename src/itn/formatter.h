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
#ifndef TIRO_SPEECH_SRC_ITN_FORMATTER_H_
#define TIRO_SPEECH_SRC_ITN_FORMATTER_H_

#include <fmt/format.h>
#include <fst/fstlib.h>

#include <memory>
#include <vector>

#include "src/itn/converters.h"
#include "src/logging.h"
#include "src/options.h"
#include "src/recognizer.h"

namespace tiro_speech::itn {

struct LookAheadFormatterConfig {
  std::string relabel_pairs_filename = "";
  std::string fst_filename = "";

  void Register(OptionsItf* opts) {
    opts->Register("relabel-pairs", &relabel_pairs_filename,
                   "Filename (possibly extended) of the relabel.pairs");
    opts->Register("fst", &fst_filename,
                   "Filename (possibly extended) of lookahead rewriter FST. "
                   "It must map from model.word_symbols to a string of bytes. "
                   "model.word_symbols MUST contain a <space> symbol.");
  }
};

/**
 * Rewriting/formatting of FSAs using a look ahead FST.
 */
class LookAheadFormatter {
 public:
  using Arc = fst::StdArc;
  using LabelPairs = std::vector<std::pair<Arc::Label, Arc::Label>>;
  using InFst = fst::VectorFst<Arc>;
  using OutFst = fst::VectorFst<Arc>;

  explicit LookAheadFormatter(const LookAheadFormatterConfig& opts);

  /**
   * Format a vector of aligned words
   *
   * The symbol table \p isyms MUST be compatible with the rewrite FST and all
   * elements of \p words MUST exist in \p isyms. The word symbols returned may
   * or may not exist \p isyms but will be formatted according to the rules of
   * the formatter. The time synchronization is preserved.
   *
   */
  std::vector<AlignedWord> FormatWords(
      const fst::SymbolTable& isyms,
      const std::vector<AlignedWord>& words) const;

  // TODO(rkjaran): std::vector<AlignedWord> FormatLattice(isyms,
  // kaldi::Lattice/CompactLattice);

 private:
  LabelPairs relabel_pairs_;
  std::unique_ptr<fst::StdILabelLookAheadFst> rewrite_fst_;
};

}  // namespace tiro_speech::itn

#endif  // TIRO_SPEECH_SRC_ITN_FORMATTER_H_
