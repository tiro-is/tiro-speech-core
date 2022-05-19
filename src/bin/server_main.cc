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
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "src/kaldi-model.h"
#include "src/options.h"
#include "src/server.h"

namespace tiro_speech {

template <typename Config>
void PrintAllOptions(const char* message = "") {
  ParseOptions po_model{message};
  po_model.DisableOption("config");
  po_model.DisableOption("help");
  po_model.DisableOption("print-args");
  po_model.DisableOption("verbose");
  Config{}.Register(&po_model);
  po_model.PrintUsage();
}

}  // namespace tiro_speech

int main(int argc, char* argv[]) {
  try {
    using namespace tiro_speech;

    std::string log_level{"INFO"};
    bool print_help_for_kaldi_models = false;
    ParseOptions po{"usage: lr-run-server"};
    po.Register("log-level", &log_level,
                "Log level (one of DEBUG, INFO, WARNING, ERROR)");
    po.Register(
        "help-kaldi-model", &print_help_for_kaldi_models,
        "Print all available configuration options for the models "
        "loaded by --kaldi-models, i.e. options that can appear in main.conf");

    SpeechServerOptions server_options;
    server_options.Register(&po);

    po.Read(argc, argv);

    if (po.NumArgs() != 0) {
      po.PrintUsage(true);
      return EXIT_FAILURE;
    }

    po.PrintConfig(std::cerr);

    tiro_speech::logging::SetLogLevel(log_level);

    if (print_help_for_kaldi_models) {
      PrintAllOptions<KaldiModelConfig>("Kaldi model config:");
      return EXIT_SUCCESS;
    }

    SpeechServerInfo server_info{server_options};
    SpeechServer server{server_info};

    server.Start();
    server.Wait();

    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    TIRO_SPEECH_FATAL(e.what());
    return EXIT_FAILURE;
  }
}
