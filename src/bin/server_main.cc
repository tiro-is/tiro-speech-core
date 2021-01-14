// Copyright 2020 Tiro ehf.
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

#include "src/options.h"
#include "src/server.h"

int main(int argc, char* argv[]) {
  try {
    using namespace tiro_speech;

    std::string log_level{"INFO"};
    ParseOptions po{"usage: lr-run-server"};
    po.Register("log-level", &log_level,
                "Log level (one of DEBUG, INFO, WARNING, ERROR)");

    SpeechServerOptions server_options;
    server_options.Register(&po);

    po.Read(argc, argv);

    if (po.NumArgs() != 0) {
      po.PrintUsage(true);
      return EXIT_FAILURE;
    }

    po.PrintConfig(std::cerr);

    tiro_speech::logging::SetLogLevel(log_level);

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
