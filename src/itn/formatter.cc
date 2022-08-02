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

#include <thrax/algo/cross.h>
#include <thrax/algo/stringcompile.h>
#include <thrax/algo/stringmap.h>
#include <unicode/unistr.h>

#include <algorithm>

#include "src/logging.h"

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

template <typename Arc>
void Optimize(fst::MutableFst<Arc>* ifst) {
  fst::RmEpsilon(ifst);
  fst::EncodeMapper<fst::StdArc> mapper{fst::kEncodeLabels |
                                        fst::kEncodeWeights};
  fst::Encode(ifst, &mapper);
  fst::VectorFst<fst::StdArc> fst_det;
  fst::Determinize(*ifst, &fst_det);
  fst::Minimize(&fst_det);
  fst::Decode(&fst_det, mapper);
  *ifst = std::move(fst_det);
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

Formatter::Formatter(const FormatterConfig& opts) {
  rewrite_fst_.reset(fst::StdFst::Read(opts.rewrite_fst_filename));
  if (rewrite_fst_ == nullptr) {
    throw std::runtime_error{
        fmt::format("Could not read rewrite FST from file '{}'",
                    opts.rewrite_fst_filename)};
  }
}

std::vector<AlignedWord> Formatter::FormatWords(
    const std::vector<AlignedWord>& words) const {
  auto words_byte_fst = ConvertToTropicalByteFst(words);

  fst::ArcSort(&words_byte_fst, fst::OLabelCompare<Arc>{});
  fst::VectorFst<Arc> formatted_candidates_fst{};
  fst::Compose(words_byte_fst, *rewrite_fst_, &formatted_candidates_fst);

  fst::VectorFst<Arc> formatted_words_fst;
  fst::ShortestPath(formatted_candidates_fst, &formatted_words_fst);

  fst::TimingFst formatted_time_fst{};
  fst::ArcMap(formatted_words_fst, &formatted_time_fst,
              fst::WeightConvertMapper<Arc, fst::TimingArc>{});

  fst::TimingFst timing_symbol_fst = ConvertToByteFst(words);
  fst::TimingFst timing_byte_fst{};
  fst::ArcSort(&timing_symbol_fst, fst::OLabelCompare<fst::TimingArc>{});
  fst::Compose(timing_symbol_fst, formatted_time_fst, &timing_byte_fst);

  return Convert(timing_byte_fst);
}

void Capitalize(std::string& str) {
  auto upiece = icu::UnicodeString::fromUTF8(str);
  upiece.replace(0, 1, upiece.tempSubString(0, 1).toUpper());
  str.clear();
  upiece.toUTF8String(str);
}

fst::VectorFst<fst::StdArc> CreateLexiconFst(
    const fst::SymbolTable& input_symbols,
    const std::vector<std::string>& filter) {
  // TODO(rkjaran): Make this configurable
  fst::VectorFst<fst::StdArc> lexicon_fst;

  std::vector<std::vector<std::string>> string_map;
  for (auto const& entry : input_symbols) {
    // if entry not in forbidded
    const auto symbol_str = entry.Symbol();
    if (std::any_of(
            filter.cbegin(), filter.cend(),
            [&](const auto& filter_sym) { return symbol_str == filter_sym; })) {
      continue;
    }
    string_map.push_back({symbol_str, symbol_str});
  }

  TIRO_SPEECH_INFO("Compiling string map");
  if (!fst::StringMapCompile(string_map, &lexicon_fst, fst::TokenType::SYMBOL,
                             fst::TokenType::BYTE, &input_symbols)) {
    TIRO_SPEECH_FATAL("Could not compile string map");
  }
  string_map.clear();

  TIRO_SPEECH_INFO("Compiling space string map");
  fst::VectorFst<fst::StdArc> space_fst;
  std::vector<std::vector<std::string>> space_map{{"<space>", " "}};
  if (!fst::StringMapCompile(space_map, &space_fst, fst::TokenType::SYMBOL,
                             fst::TokenType::BYTE, &input_symbols)) {
    TIRO_SPEECH_FATAL("Could not compile space string map");
  }
  TIRO_SPEECH_INFO("Finished compiling space string map");

  // This is an artifact we'll have to live with... ending every word with a
  // space, even the last one
  fst::Concat(&lexicon_fst, space_fst);

  TIRO_SPEECH_INFO("Getting the closure of the string map");
  fst::Closure(&lexicon_fst, fst::ClosureType::CLOSURE_PLUS);

  TIRO_SPEECH_INFO("Optimizing the lexicon");
  Optimize(&lexicon_fst);
  fst::ArcSort(&lexicon_fst, fst::ILabelCompare<fst::StdArc>());

  return lexicon_fst;
}

std::pair<fst::StdILabelLookAheadFst, LookAheadFormatter::LabelPairs>
CreateLookaheadFormatterFst(const fst::StdFst& lexicon_fst,
                            const fst::StdFst& rewrite_fst) {
  TIRO_SPEECH_DEBUG("Composing lexicon and rewrite FST");
  fst::VectorFst<fst::StdArc> rewrite_from_syms_fst;
  fst::Compose(lexicon_fst, rewrite_fst, &rewrite_from_syms_fst);

  TIRO_SPEECH_DEBUG("Converting to lookahead");
  fst::StdILabelLookAheadFst lookahead_fst(rewrite_from_syms_fst);

  LookAheadFormatter::LabelPairs relabel_pairs;
  fst::LabelLookAheadRelabeler<fst::StdArc>::RelabelPairs(lookahead_fst,
                                                          &relabel_pairs);

  return {lookahead_fst, relabel_pairs};
}

}  // namespace tiro_speech::itn
