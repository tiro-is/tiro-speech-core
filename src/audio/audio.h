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
#ifndef TIRO_SPEECH_SRC_AUDIO_AUDIO_H_
#define TIRO_SPEECH_SRC_AUDIO_AUDIO_H_

#include <matrix/kaldi-vector.h>

#include <string>

#include "src/base.h"

namespace tiro_speech {

using Vector = kaldi::Vector<float>;
using VectorBase = kaldi::VectorBase<float>;

// NOTE: This is copied from the Protobuf definition for
//       lr.speech.v2beta1.RecognitionConfig.AudioEncoding
enum class AudioEncoding {
  ENCODING_UNSPECIFIED = 0,

  // Uncompressed 16-bit signed little-endian samples (Linear PCM).
  LINEAR16,

  // [`FLAC`](https://xiph.org/flac/documentation.html) (Free Lossless Audio
  // Codec) is the recommended encoding because it is
  // lossless--therefore recognition is not compromised--and
  // requires only about half the bandwidth of `LINEAR16`. `FLAC` stream
  // encoding supports 16-bit and 24-bit samples, however, not all fields in
  // `STREAMINFO` are supported.
  FLAC,

  // // 8-bit samples that compand 14-bit audio samples using G.711 PCMU/mu-law.
  // MULAW,

  // // Adaptive Multi-Rate Narrowband codec. `sample_rate_hertz` must be 8000.
  // AMR,

  // // Adaptive Multi-Rate Wideband codec. `sample_rate_hertz` must be 16000.
  // AMR_WB,

  // // Opus encoded audio frames in Ogg container
  // // ([OggOpus](https://wiki.xiph.org/OggOpus)).
  // // `sample_rate_hertz` must be 16000.
  // OGG_OPUS,

  // // Although the use of lossy encodings is not recommended, if a very low
  // // bitrate encoding is required, `OGG_OPUS` is highly preferred over
  // // Speex encoding. The [Speex](https://speex.org/)  encoding supported by
  // // Cloud Speech API has a header byte in each block, as in MIME type
  // // `audio/x-speex-with-header-byte`.
  // // It is a variant of the RTP Speex encoding defined in
  // // [RFC 5574](https://tools.ietf.org/html/rfc5574).
  // // The stream is a sequence of blocks, one block per RTP packet. Each block
  // // starts with a byte containing the length of the block, in bytes,
  // followed
  // // by one or more frames of Speex data, padded to an integral number of
  // // bytes (octets) as specified in RFC 5574. In other words, each RTP header
  // // is replaced with a single byte containing the block length. Only Speex
  // // wideband is supported. `sample_rate_hertz` must be 16000.
  // SPEEX_WITH_HEADER_BYTE,

  // MP3
  MP3 = 9,

  GUESS = 10
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

/**
 * Convert a single channel MP3 binary blob to a Kaldi vector.
 * \p wavevector must be owned by the caller (not-null).
 */
[[deprecated("Use CodedBytesToWaveVector")]] void Mp3BytesToWaveVector(
    const std::string& bytes, Vector* wavevector,
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
