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
#include <string>
#include "src/audio/audio-source.h"
#include "src/utils.h"

using namespace tiro_speech;

TEST_CASE("Constructing ContentAudioSource works for LINEAR16",
          "[server][audio-source]") {
  const std::string wav_name = "test/only_speech_16000hz.wav";
  AudioSourceInfo info;
  info.encoding = AudioEncoding::LINEAR16;
  info.sample_rate_hertz = 16000;
  ContentAudioSource::Bytes content = ReadWaveFile(wav_name);

  SECTION("For now ctor should throw on non LINEAR16 encoding") {
    info.encoding = AudioEncoding::ENCODING_UNSPECIFIED;
    REQUIRE_THROWS_AS((ContentAudioSource{info, content}), AudioSourceError);
  }

  SECTION("Ctor should not throw on LINEAR16 encoding") {
    REQUIRE_NOTHROW((ContentAudioSource{info, content}));
  }
}

TEST_CASE("ContentAudioSource can source chunks", "[server][audio-source]") {
  const std::string wav_name = "test/only_speech_16000hz.wav";
  AudioSourceInfo info;
  info.encoding = AudioEncoding::LINEAR16;
  info.sample_rate_hertz = 16000;
  ContentAudioSource::Bytes content = ReadWaveFile(wav_name);

  ContentAudioSource source{info, content};
  REQUIRE_NOTHROW(source.Open());
  REQUIRE(source.HasMoreChunks());
  SECTION("Getting chunks doesn't throw when ContentAudioSource has chunks") {
    REQUIRE_NOTHROW([&]() {
      while (source.HasMoreChunks()) {
        auto chunk = source.NextChunk();
      }
    }());
  }

  SECTION("Chunks returned from ContentAudioSource aren't empty vectors") {
    while (source.HasMoreChunks()) {
      auto chunk = source.NextChunk();
      REQUIRE(chunk.Dim() > 0);
    }
  }
}

TEST_CASE("ContentAudioSource can resample LINEAR16 data",
          "[server][audio-source]") {
  AudioSourceInfo base_info;
  base_info.encoding = AudioEncoding::LINEAR16;
  base_info.sample_rate_hertz = 16000;
  ContentAudioSource base_source{
      base_info, ReadWaveFile("test/only_speech_16000hz.wav"), 16000};
  base_source.Open();

  SECTION("It is possible to downsample from 32kHz to 16kHz") {
    const std::string wav_name = "test/only_speech_32000hz.wav";
    AudioSourceInfo info;
    info.encoding = AudioEncoding::LINEAR16;
    info.sample_rate_hertz = 32000;
    ContentAudioSource::Bytes content = ReadWaveFile(wav_name);
    REQUIRE(!content.empty());
    ContentAudioSource source{info, content, 16000};
    source.Open();
    REQUIRE(source.TotalChunks() > 0);
    REQUIRE(source.HasMoreChunks());
    REQUIRE(source.ChunksSeen() == 0);
    REQUIRE(source.TotalChunks() == base_source.TotalChunks());

    while (source.HasMoreChunks()) {
      auto chunk = source.NextChunk();
      REQUIRE(chunk.Dim() > 0);
    }
  }

  SECTION("It is possible to upsample from 8kHz to 16kHz") {
    const std::string wav_name = "test/only_speech_8000hz.wav";
    AudioSourceInfo info;
    info.encoding = AudioEncoding::LINEAR16;
    info.sample_rate_hertz = 8000;
    ContentAudioSource::Bytes content = ReadWaveFile(wav_name);
    ContentAudioSource source{info, content, 16000};
    source.Open();
    REQUIRE(source.TotalChunks() > 0);
    REQUIRE(source.HasMoreChunks());
    REQUIRE(source.ChunksSeen() == 0);
    REQUIRE(source.TotalChunks() == base_source.TotalChunks());
    while (source.HasMoreChunks()) {
      auto chunk = source.NextChunk();
      REQUIRE(chunk.Dim() > 0);
    }
  }
}
