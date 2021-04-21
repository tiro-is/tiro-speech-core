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
#include <catch2/catch.hpp>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "src/itn/punctuation.h"

TEST_CASE("ElectraPunctuator can punctute", "[itn]") {
  using namespace tiro_speech;
  using namespace tiro_speech::itn;

  ElectraPunctuatorConfig opts{};
  opts.pytorch_jit_model_filename =
      std::filesystem::path{std::getenv("TEST_SRCDIR")} /
      "clarin_electra_punctuation_model_traced/file/traced_electra.pt";
  opts.word_piece_opts.vocab_filename =
      std::filesystem::path{std::getenv("TEST_SRCDIR")} /
      "clarin_electra_punctuation_vocab/file/vocab.txt";

  ElectraPunctuator punctuator{opts};

  const std::vector<std::string> words{"eftirlit",
                                       "í",
                                       "gangi",
                                       "ég",
                                       "segi",
                                       "það",
                                       "er",
                                       "lögregluliðin",
                                       "á",
                                       "tánum",
                                       "að",
                                       "sinna",
                                       "þessu",
                                       "eftirliti",
                                       "mjög",
                                       "vel",
                                       "við",
                                       "erum",
                                       "að",
                                       "fá",
                                       "líka",
                                       "margar",
                                       "ábendingar",
                                       "sem",
                                       "er",
                                       "bara",
                                       "mjög",
                                       "gott",
                                       "frá",
                                       "almenningi",
                                       "og",
                                       "líka",
                                       "rekstraraðilum",
                                       "og",
                                       "þeim",
                                       "er",
                                       "fylgt",
                                       "eftir",
                                       "ýmsir",
                                       "segja",
                                       "að",
                                       "það",
                                       "væri",
                                       "bara",
                                       "í",
                                       "ágætis",
                                       "málum",
                                       "hjá",
                                       "okkur",
                                       "hann",
                                       "bætir",
                                       "við",
                                       "ég",
                                       "inn",
                                       "á",
                                       "covid",
                                       "síðunni",
                                       "covid",
                                       "punktur",
                                       "bis"};
  const std::vector<std::string> expected_punctuated_words{"eftirlit",
                                                           "í",
                                                           "gangi.",
                                                           "ég",
                                                           "segi,",
                                                           "það",
                                                           "er",
                                                           "lögregluliðin",
                                                           "á",
                                                           "tánum",
                                                           "að",
                                                           "sinna",
                                                           "þessu",
                                                           "eftirliti",
                                                           "mjög",
                                                           "vel.",
                                                           "við",
                                                           "erum",
                                                           "að",
                                                           "fá",
                                                           "líka",
                                                           "margar",
                                                           "ábendingar",
                                                           "sem",
                                                           "er",
                                                           "bara",
                                                           "mjög",
                                                           "gott",
                                                           "frá",
                                                           "almenningi",
                                                           "og",
                                                           "líka",
                                                           "rekstraraðilum",
                                                           "og",
                                                           "þeim",
                                                           "er",
                                                           "fylgt",
                                                           "eftir.",
                                                           "ýmsir",
                                                           "segja",
                                                           "að",
                                                           "það",
                                                           "væri",
                                                           "bara",
                                                           "í",
                                                           "ágætis",
                                                           "málum",
                                                           "hjá",
                                                           "okkur.",
                                                           "hann",
                                                           "bætir",
                                                           "við,",
                                                           "ég",
                                                           "inn",
                                                           "á",
                                                           "covid",
                                                           "síðunni",
                                                           "covid",
                                                           "punktur",
                                                           "bis"};
  const std::vector<std::string> expected_punctuated_and_capitalized_words{
      "eftirlit",
      "í",
      "gangi.",
      "Ég",
      "segi,",
      "það",
      "er",
      "lögregluliðin",
      "á",
      "tánum",
      "að",
      "sinna",
      "þessu",
      "eftirliti",
      "mjög",
      "vel.",
      "Við",
      "erum",
      "að",
      "fá",
      "líka",
      "margar",
      "ábendingar",
      "sem",
      "er",
      "bara",
      "mjög",
      "gott",
      "frá",
      "almenningi",
      "og",
      "líka",
      "rekstraraðilum",
      "og",
      "þeim",
      "er",
      "fylgt",
      "eftir.",
      "Ýmsir",
      "segja",
      "að",
      "það",
      "væri",
      "bara",
      "í",
      "ágætis",
      "málum",
      "hjá",
      "okkur.",
      "Hann",
      "bætir",
      "við,",
      "ég",
      "inn",
      "á",
      "covid",
      "síðunni",
      "covid",
      "punktur",
      "bis"};

  SECTION("Punctate produces known output for known input") {
    auto punctuated_words = punctuator.Punctuate(words);
    CHECK(punctuated_words.size() == words.size());
    REQUIRE_THAT(punctuated_words,
                 Catch::Matchers::Equals(expected_punctuated_words));
  }

  SECTION("Punctate produces known capitalized output for known input") {
    auto punctuated_words = punctuator.Punctuate(words, /* capitalize */ true);
    CHECK(punctuated_words.size() == words.size());
    REQUIRE_THAT(
        punctuated_words,
        Catch::Matchers::Equals(expected_punctuated_and_capitalized_words));
  }

  SECTION("Punctuate produces only adds punctuate once per word") {
    // We know that in our test case sólskin is split up
    const std::vector<std::string> expected{
        "þú", "ert",    "mitt", "sólskin,", "mitt",     "eina",  "sólskin.",
        "þú", "gleður", "mig",  "þegar",    "heimilin", "gránar"};
    const std::vector<std::string> expected_capitalized{
        "þú", "ert",    "mitt", "sólskin,", "mitt",     "eina",  "sólskin.",
        "Þú", "gleður", "mig",  "þegar",    "heimilin", "gránar"};
    const std::vector<std::string> words{
        "þú", "ert",    "mitt", "sólskin", "mitt",     "eina",  "sólskin",
        "þú", "gleður", "mig",  "þegar",   "heimilin", "gránar"};
    auto punctuated = punctuator.Punctuate(words);
    REQUIRE_THAT(punctuated, Catch::Matchers::Equals(expected));
    auto punctuated_capitalized =
        punctuator.Punctuate(words, /* capitalized */ true);
    REQUIRE_THAT(punctuated_capitalized,
                 Catch::Matchers::Equals(expected_capitalized));
  }
}
