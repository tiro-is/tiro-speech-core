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
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

#include <catch2/catch.hpp>

#include "src/audio/ffmpeg-wrapper.h"

using namespace tiro_speech::av;

namespace {

auto codec_to_info(Codec codec) -> std::pair<const char*, const char*> {
  switch (codec) {
    case Codec::kFlac:
      return {"FLAC", "test/example.flac"};
    case Codec::kMp3:
      return {"MP3", "test/example.mp3"};
    default:
      return {"Unknown codec", ""};
  }
}

const std::array<Codec, 2> codecs{{Codec::kFlac, Codec::kMp3}};

}  // end namespace

TEST_CASE("Creating an empty Packet", "[ffmpeg]") {
  REQUIRE_NOTHROW(Packet{});
  Packet packet;
  REQUIRE(packet->data == packet.get()->data);
}

TEST_CASE("Creating an empty Frame", "[ffmpeg]") {
  REQUIRE_NOTHROW(Frame{});
  Frame frame;
  REQUIRE(frame.get() != nullptr);
  REQUIRE(frame->data == frame.get()->data);
}

TEST_CASE("Creating a CodeContext", "[ffmpeg]") {
  SECTION("Creating a CodeContext with a supported codec (MP3) doesn't throw") {
    REQUIRE_NOTHROW(CodecContext{Codec::kMp3});
  }

  SECTION("Constructing a CodeContext with FLAC doesn't throw") {
    REQUIRE_NOTHROW(CodecContext{Codec::kFlac});
  }

  SECTION("A CodeContext has access to a valid avcodec_context for FLAC") {
    CodecContext codec_ctx{Codec::kFlac};
    REQUIRE(codec_ctx.get() != nullptr);

    CodecContext mp3_ctx{Codec::kMp3};
    REQUIRE(mp3_ctx.get() != nullptr);
  }

  SECTION(
      "Creating a CodeContext with an usupported/unknown codec throws "
      "AvError") {
    REQUIRE_THROWS_AS(CodecContext{Codec::kUnknown}, AvError);
  }
}

TEST_CASE(
    "InputBuffer refills if we deplete it and is empty when stream is depleted",
    "[ffmpeg]") {
  std::string some_x(64, 'X');
  std::string some_y(64, 'Y');
  std::istringstream input_stream{some_x + some_y};
  InputBuffer<64, 4, 2> input_buffer{input_stream};

  std::string first_data(reinterpret_cast<const char*>(input_buffer.Data()),
                         input_buffer.DataSize());

  REQUIRE_THAT(first_data, Catch::Matchers::Equals(some_x));
  REQUIRE(!input_buffer.Empty());
  REQUIRE(input_buffer.DataSize() == 64);
  input_buffer.Forward(input_buffer.DataSize());

  std::string second_data(reinterpret_cast<const char*>(input_buffer.Data()),
                          input_buffer.DataSize());
  REQUIRE_THAT(second_data, Catch::Matchers::Equals(some_y));
  input_buffer.Forward(input_buffer.DataSize());

  // We have depleted the input_stream, so this should return true
  REQUIRE(input_buffer.Empty());
}

TEST_CASE("InputBuffer returns the correct amount of bytes", "[ffmpeg]") {
  std::ifstream is{codec_to_info(Codec::kFlac).second,
                   std::ios::binary | std::ios::ate};
  const std::size_t n_bytes = is.tellg();
  is.seekg(0);
  std::size_t n_bytes_from_buf = 0;
  InputBuffer<> inbuf{is};
  while (!inbuf.Empty()) {
    int n_read = std::min<int>(20000, inbuf.DataSize());
    n_bytes_from_buf += n_read;
    inbuf.Forward(n_read);
  }
  REQUIRE(n_bytes_from_buf == n_bytes);
}

TEST_CASE("Parser can parse files encoded with supported codecs", "[ffmpeg]") {
  for (auto codec : codecs) {
    SECTION(std::string{"Parser can parse files encoded with "} +
            codec_to_info(codec).first) {
      std::ifstream input_stream{codec_to_info(codec).second};
      InputBuffer<> inbuf{input_stream};
      REQUIRE(!inbuf.Empty());
      Parser parser{codec};
      Packet packet;

      SECTION("Constructing Parser doesn't throw with a supported codec") {
        REQUIRE_NOTHROW(Parser{codec});
      }
      SECTION("Parser can parse an input buffer holding a stream") {
        REQUIRE_NOTHROW(([&] { parser.Parse(packet, inbuf); }()));
        // NOTE(rkjaran): Leaving these as comments... Parse may or may not
        //                supply packet with data. It'll only fill packet with
        //                data if it can flush.
        // REQUIRE(packet->size > 0);
        // REQUIRE(packet->data != nullptr);
      }
      SECTION(
          "Parser eventually depletes the buffer for a finite input stream") {
        bool all_empty = true;
        while (!inbuf.Empty()) {
          parser.Parse(packet, inbuf);
          if (packet->size > 0 && packet->data != nullptr) all_empty = false;
        }
        REQUIRE_FALSE(all_empty);
        REQUIRE(inbuf.Empty());
      }
    }
  }
}

TEST_CASE("Decoder can decode valid streams", "[ffmpeg]") {
  for (auto codec : codecs) {
    SECTION(std::string{"Decoder can decode stream encoded with "} +
            codec_to_info(codec).first) {
      auto codec_file_path = codec_to_info(codec).second;

      std::ifstream input_stream{codec_file_path};
      std::ostringstream output_stream;
      Decoder decoder{codec, input_stream, output_stream};
      REQUIRE_NOTHROW(decoder.PartialDecode());
      while (decoder.PartialDecode())
        ;
      REQUIRE(output_stream.str().size() > 0);
      REQUIRE_NOTHROW(decoder.PartialDecode());
      REQUIRE_NOTHROW(decoder.Flush());
    }
  }
}

TEST_CASE("Parser can parse and decode MPEG-1 Layer 3", "[ffmpeg][mpeg1]") {
  const std::string filename = "test/example_44100Hz.mp3";
  std::ifstream is{filename};

  SECTION("Parser eventually depletes the buffer for a finite input stream") {
    InputBuffer<> inbuf{is};
    REQUIRE_FALSE(inbuf.Empty());
    Parser parser{Codec::kMp3};
    Packet packet;

    bool all_empty = true;
    while (!inbuf.Empty()) {
      parser.Parse(packet, inbuf);
      if (packet->size > 0 && packet->data != nullptr) all_empty = false;
    }
    REQUIRE_FALSE(all_empty);
    REQUIRE(inbuf.Empty());
  }

  SECTION("Decoder can decode MPEG-1 Layer 3") {
    std::ostringstream output_stream{};
    constexpr int expected_size = 116194;
    Decoder decoder{Codec::kMp3, is, output_stream, 16000};
    REQUIRE_NOTHROW([&decoder]() {
      while (decoder.PartialDecode()) continue;
    }());
    REQUIRE_NOTHROW(decoder.Flush());
    REQUIRE(output_stream.str().size() == expected_size);
  }
}

TEST_CASE("IoContextDecoder can decode files", "[ffmpeg][mpeg1]") {
  std::array<std::pair<std::string, UrlIoContext>, 1> contexes{
      std::make_pair("UrlIoContext file MP3 44100Hz",
                     UrlIoContext{"file:test/example_44100Hz.mp3"}),
  };

  for (auto& ctx_pair : contexes) {
    SECTION("IoContextDecoder can decode " + ctx_pair.first) {
      std::ostringstream output_stream;
      IoContextDecoder<UrlIoContext> decoder{
          Codec::kMp3, std::move(ctx_pair.second), output_stream, 16000};

      REQUIRE_NOTHROW([&] {
        while (decoder.PartialDecode())
          ;
      }());
      REQUIRE_NOTHROW(decoder.Flush());
      REQUIRE_FALSE(output_stream.str().empty());
    }
  }
}
