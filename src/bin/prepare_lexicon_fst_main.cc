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
#include <fst/fst.h>

#include <cstdlib>
#include <string>

#include "src/itn/formatter.h"
#include "src/logging.h"
#include "src/options.h"

int main(int argc, char* argv[]) {
  try {
    using namespace tiro_speech;

    std::string log_level{"INFO"};
    ParseOptions po{
        "usage: prepare_lexicon_fst <symbol-table> <byte-rewrite-fst> "
        "<out-lexicon-fst>"};
    po.Register("log-level", &log_level,
                "Log level (one of DEBUG, INFO, WARNING, ERROR)");
    po.Read(argc, argv);

    if (po.NumArgs() != 3) {
      po.PrintUsage(true);
      return EXIT_FAILURE;
    }
    tiro_speech::logging::SetLogLevel(log_level);

    const std::string symbol_table_filename = po.GetArg(1);
    const std::string byte_rewrite_fst_filename = po.GetArg(2);

    const std::string lexicon_fst_filename = po.GetArg(3);

    TIRO_SPEECH_INFO("Loading rewrite FST");
    std::unique_ptr<fst::StdFst> byte_rewrite_fst{
        fst::StdFst::Read(byte_rewrite_fst_filename)};

    TIRO_SPEECH_INFO("Loading symbol table");
    std::unique_ptr<fst::SymbolTable> symbol_table{
        fst::SymbolTable::ReadText(symbol_table_filename)};

    if (symbol_table->Find("<space>") == fst::kNoSymbol) {
      TIRO_SPEECH_FATAL("Symbol table has to include '<space>'.");
    }

    TIRO_SPEECH_INFO("Creating lexicon FST from symbol table.");
    fst::VectorFst<fst::StdArc> lexicon_fst =
        itn::CreateLexiconFst(*symbol_table);

    if (!lexicon_fst.Write(lexicon_fst_filename)) {
      TIRO_SPEECH_FATAL("Could not write lexicon FST to file.");
    }

    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    TIRO_SPEECH_FATAL(e.what());
    return EXIT_FAILURE;
  }
}
