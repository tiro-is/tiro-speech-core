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
#ifndef TIRO_SPEECH_SRC_AUDIO_AUDIO_H_
#define TIRO_SPEECH_SRC_AUDIO_AUDIO_H_

#include <matrix/kaldi-vector.h>

#include <string>

#include "src/base.h"

namespace tiro_speech {

using Vector = kaldi::Vector<float>;
using VectorBase = kaldi::VectorBase<float>;

// NOTE: This is copied from the Protobuf definition for
//       tiro.speech.v1alpha.RecognitionConfig.AudioEncoding
enum class AudioEncoding {
  ENCODING_UNSPECIFIED = 0,
  LINEAR16,
  FLAC,
  MP3,
  GUESS
};

/**
 * Convert a single channel LINEAR16 RAW PCM binary blob to a Kaldi vector.
 * \p wavevector must be owned by the caller (not-null).
 *
 * \note Only supports single channel PCM
 */
void Linear16BytesToWaveVector(const std::string& bytes, Vector* wavevector);

/**
 * This version expects \param wavevector to fit samples extracted from \param
 * bytes.
 */
void Linear16BytesToWaveVector(const std::string& bytes,
                               VectorBase* wavevector);

void CodedBytesToWaveVector(AudioEncoding encoding, const std::string& bytes,
                            Vector* wavevector,
                            int target_sample_rate_hertz = kDefaultSampleRate);

void ResampleWaveForm(float orig_freq, const Vector& wave, float new_freq,
                      Vector* new_wave);

/**
 * Returns true if RIFF (WAVE) header found in stream, position in stream will
 * be at the start of the data section (i.e. end of header).
 */
bool HasRiffHeader(std::istream& is);

}  // end namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_AUDIO_AUDIO_H_
