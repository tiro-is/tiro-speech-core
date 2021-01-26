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
#ifndef TIRO_SPEECH_SRC_VAD_H_
#define TIRO_SPEECH_SRC_VAD_H_

#include <matrix/kaldi-vector.h>

#include <string>
#include <vector>

#include "webrtc/common_audio/vad/include/webrtc_vad.h"

namespace tiro_speech {

using Vector = kaldi::Vector<float>;
using VectorBase = kaldi::VectorBase<float>;

/**
 * Thin wrapper around the WebRTC VAD API
 */
class VoiceActivityDetector {
 public:
  enum VoiceActivity { kError = -1, kActiveVoice = 1, kNonActiveVoice = 0 };

  VoiceActivityDetector();

  /**
   * See set_mode(int).
   *   - mode   [i]   : Aggressiveness mode (0, 1, 2, or 3).
   */
  explicit VoiceActivityDetector(int mode);

  bool Init();

  // Sets the VAD operating mode. A more aggressive (higher mode) VAD is more
  // restrictive in reporting speech. Put in other words the probability of
  // being speech when the VAD returns 1 is increased with increasing mode. As a
  // consequence also the missed detection rate goes up.
  //
  // - mode   [i]   : Aggressiveness mode (0, 1, 2, or 3).
  //
  // returns        : false on error
  bool set_mode(int mode);

  // Calculates a VAD decision for the |audio_frame|. For valid sampling rates
  // frame lengths, see the description of
  // WebRtcVad_ValidRatesAndFrameLengths().
  //
  // - fs           [i]   : Sampling frequency (Hz): 8000, 16000, or 32000
  // - audio_frame  [i]   : Audio frame buffer.
  // - frame_length [i]   : Length of audio frame buffer in number of samples.
  //
  VoiceActivity Process(int fs, const int16_t* audio_frame, int frame_length);

  // Calculates a VAD decision for the |audio_frame|. For valid sampling rates
  // frame lengths, see the description of
  // WebRtcVad_ValidRatesAndFrameLengths().
  //
  // - fs           [i]   : Sampling frequency (Hz): 8000, 16000, or 32000
  // - audio_frame  [i]   : Audio frame buffer.
  VoiceActivity Process(int fs, const std::vector<int16_t>& audio_frame);

  // Checks for valid combinations of |rate| and |frame_length|. We support 10,
  // 20 and 30 ms frames and the rates 8000, 16000 and 32000 Hz.
  //
  // - rate         [i] : Sampling frequency (Hz).
  // - frame_length [i] : Speech frame buffer length in number of samples.
  //
  // returns            : 0 - (valid combination), -1 - (invalid combination)
  bool ValidRateAndFrameLength(int rate, int frame_length) const;

  virtual ~VoiceActivityDetector();

 private:
  VadInst* vad_handle_;
};

class Vad {
 public:
  // Sample rate must be one of {8000, 16000, 32000, 48000} and frame_len_ms
  // must be 10, 30 or 50 milliseconds
  explicit Vad(int sample_rate, int frame_len_ms);

  int NumSamplesPerFrame() { return frame_len_samples_; }

  std::vector<bool> DetectSpeech(const int16_t* data, std::size_t data_sz);

  bool HasSpeech(const int16_t* data, std::size_t data_sz);

  bool HasSpeech(const std::vector<int16_t>& data);

  bool HasSpeech(const VectorBase& waveform);

 private:
  const int sample_rate_;
  const int frame_len_samples_;
  tiro_speech::VoiceActivityDetector vad_;
};

}  // end namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_VAD_H_
