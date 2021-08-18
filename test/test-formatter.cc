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
#include <fst/compose.h>
#include <fst/fst-decl.h>
#include <fst/fst.h>
#include <fst/script/compile.h>
#include <fst/script/print-impl.h>
#include <fst/string.h>
#include <fst/symbol-table.h>
#include <fst/vector-fst.h>
#include <fst/weight.h>

#include <catch2/catch.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
using namespace std::chrono_literals;
#include <filesystem>

#include "src/itn/converters.h"
#include "src/itn/formatter.h"
#include "src/itn/timing-weight.h"
#include "src/recognizer.h"

TEST_CASE("A TimingFst can be created and printed", "[fst][itn]") {
  fst::VectorFst<fst::TimingArc> fst{};

  fst::SymbolTable symtab{};
  auto sym1 = symtab.AddSymbol("ten");
  auto sym2 = symtab.AddSymbol("six");
  auto sym3 = symtab.AddSymbol("<sp>");

  auto state0 = fst.AddState();
  auto state1 = fst.AddState();
  auto state2 = fst.AddState();
  auto state3 = fst.AddState();

  fst.SetStart(state0);

  fst::TimingArc arc1{};
  arc1.nextstate = state1;
  arc1.ilabel = sym1;
  arc1.olabel = sym1;
  arc1.weight = {1, 4};
  fst.AddArc(state0, arc1);

  fst::TimingArc arc2{};
  arc2.nextstate = state2;
  arc2.ilabel = sym3;
  arc2.olabel = sym3;
  arc2.weight = fst::TimingWeight::One();
  fst.AddArc(state1, arc2);

  fst::TimingArc arc3{};
  arc3.nextstate = state3;
  arc3.ilabel = sym2;
  arc3.olabel = sym2;
  arc3.weight = {3, 4};
  fst.AddArc(state2, arc3);

  fst.SetFinal(state3, fst::TimingWeight::One());

  fst::FstPrinter<fst::TimingArc> printer{fst,  &symtab, &symtab, nullptr,
                                          true, false,   "\t"};

  const std::string text_fst = R"(0	1	ten	1,4
1	2	<sp>
2	3	six	3,4
3
)";
  std::stringstream ss{};
  printer.Print(ss, "");
  REQUIRE(ss.str() == text_fst);
}

TEST_CASE("Formatting using a Timing FST works", "[fst][itn]") {
  using namespace tiro_speech;
  using namespace tiro_speech::itn;
  using Arc = fst::StdArc;
  const std::filesystem::path model_path{"test/test_model"};

  std::unique_ptr<fst::SymbolTable> isyms{fst::SymbolTable::ReadText(
      model_path / "graph" / "words.txt", fst::SymbolTableTextOptions{})};

  REQUIRE(isyms->Find("<space>") != fst::kNoLabel);

  const std::string expected_byte_output{",/er 01.12.11:\n "};
  const std::vector<AlignedWord> expected_alis_output{
      {0ms, 112ms, ",/er"}, {120ms, 690ms, "01.12.11:\n"}};
  std::vector<AlignedWord> word_alis{
      {0ms, 50ms, "<k>"},       {60ms, 5ms, "<sk>"},   {102ms, 10ms, "er"},
      {120ms, 10ms, "0"},     {240ms, 120ms, "1"}, {410ms, 50ms, "12"},
      {520ms, 150ms, "2011"}, {700ms, 50ms, "<pp>"}, {751ms, 35ms, "<nl>"},
      {800ms, 10ms, "<hy>"}};

  fst::VectorFst<fst::TimingArc> S = Convert(word_alis, *isyms);

  fst::VectorFst<Arc> phrase_fst = ConvertToTropical(word_alis, *isyms);
  REQUIRE(phrase_fst.NumStates() ==
          static_cast<Arc::StateId>(word_alis.size() * 2 + 1));
  REQUIRE(fst::Verify(phrase_fst));

  std::vector<std::pair<Arc::Label, Arc::Label>> relabel_pairs{};
  fst::ReadLabelPairs(model_path / "relabel.pairs", &relabel_pairs);
  fst::Relabel(&phrase_fst, std::vector<std::pair<int, int>>{}, relabel_pairs);
  REQUIRE(phrase_fst.NumStates() > 0);
  fst::ArcSort(&phrase_fst, fst::OLabelCompare<Arc>{});

  std::unique_ptr<fst::StdILabelLookAheadFst> rewrite_fst{
      fst::StdILabelLookAheadFst::Read(model_path /
                                       "rewrite_from_syms_lookahead.fst")};

  REQUIRE(rewrite_fst->NumStates() > 0);
  REQUIRE(fst::Verify(*rewrite_fst));

  fst::VectorFst<Arc> out_fst{};
  fst::Compose(phrase_fst, *rewrite_fst, &out_fst);
  REQUIRE(fst::Verify(out_fst));
  REQUIRE(out_fst.NumStates() > 0);

  // Map to timing
  fst::VectorFst<fst::TimingArc> shortest_timing_fst{};
  fst::ArcMap(out_fst, &shortest_timing_fst,
              fst::WeightConvertMapper<fst::StdArc, fst::TimingArc>{});
  REQUIRE(shortest_timing_fst.NumStates() > 0);
  REQUIRE(fst::Verify(shortest_timing_fst));

  fst::VectorFst<fst::TimingArc> shortest_with_time{};
  fst::Compose(S, shortest_timing_fst, &shortest_with_time);
  REQUIRE(fst::Verify(shortest_with_time));
  REQUIRE(shortest_with_time.NumStates() > 0);

  auto formatted_word_alis = Convert(shortest_with_time);
  REQUIRE(formatted_word_alis.size() == expected_alis_output.size());

  for (std::size_t idx = 0; idx < formatted_word_alis.size(); ++idx) {
    CAPTURE(formatted_word_alis[idx]);
    REQUIRE(formatted_word_alis[idx].word_symbol ==
            expected_alis_output[idx].word_symbol);
    REQUIRE(formatted_word_alis[idx].duration ==
            expected_alis_output[idx].duration);
    REQUIRE(formatted_word_alis[idx].start_time ==
            expected_alis_output[idx].start_time);
  }
}
