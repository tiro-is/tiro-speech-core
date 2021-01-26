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
#include "src/audio/audio.h"

#include <feat/resample.h>

#include <fstream>
#include <sstream>
#include <string>

#include "src/audio/ffmpeg-wrapper.h"

namespace tiro_speech {

void Linear16BytesToWaveVector(const std::string& bytes, Vector* wavevector) {
  int num_samples = bytes.size() / 2;
  assert(wavevector != nullptr);
  wavevector->Resize(num_samples);
  Linear16BytesToWaveVector(bytes, static_cast<VectorBase*>(wavevector));
}

void Linear16BytesToWaveVector(const std::string& bytes,
                               VectorBase* wavevector) {
  const int num_samples = bytes.size() / 2;
  assert(wavevector != nullptr);
  assert(num_samples == wavevector->Dim());
  const uint16_t* data_ptr = reinterpret_cast<uint16 const*>(&bytes[0]);
  for (int i = 0; i < num_samples; i++) {
    int16_t val = *data_ptr++;
    (*wavevector)(i) = val;
  }
}

void CodedBytesToWaveVector(AudioEncoding encoding, const std::string& bytes,
                            Vector* wavevector, int target_sample_rate_hertz) {
  using namespace tiro_speech::av;

  const Codec codec = [encoding]() {
    switch (encoding) {
      case AudioEncoding::MP3:
        return Codec::kMp3;
      case AudioEncoding::FLAC:
        return Codec::kFlac;
      case AudioEncoding::LINEAR16:
        [[fallthrough]];
      case AudioEncoding::ENCODING_UNSPECIFIED:
        [[fallthrough]];
      default:
        // TODO(rkjaran): Define exceptions
        throw std::runtime_error{"Unsupported or unspecified AudioEncoding"};
    }
  }();
  std::istringstream input{bytes};
  std::ostringstream output{};
  tiro_speech::av::Decoder decoder{codec, input, output,
                                   target_sample_rate_hertz};
  while (decoder.PartialDecode()) {
  };
  decoder.Flush();
  Linear16BytesToWaveVector(output.str(), wavevector);
}

void Mp3BytesToWaveVector(const std::string& bytes, Vector* wavevector,
                          int target_sample_rate_hertz) {
  CodedBytesToWaveVector(AudioEncoding::MP3, bytes, wavevector,
                         target_sample_rate_hertz);
}

void ResampleWaveForm(float orig_freq, const Vector& wave, float new_freq,
                      Vector* new_wave) {
  float lowpass_cutoff = 0.99 * 0.5 * std::min(new_freq, orig_freq);
  int32_t lowpass_filter_width = 6;
  kaldi::LinearResample resampler(orig_freq, new_freq, lowpass_cutoff,
                                  lowpass_filter_width);
  resampler.Resample(wave, true, new_wave);
}

bool HasRiffHeader(std::istream& is) {
  is.exceptions(std::ios::failbit | std::ios::badbit);
  std::string riff(4, '\0');
  is.read(&riff[0], 4);
  if (riff != "RIFF") return false;
  is.seekg(44);
  return true;
}

}  // namespace tiro_speech
