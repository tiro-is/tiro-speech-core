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
#include "src/vad.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <vector>

namespace tiro_speech {

VoiceActivityDetector::VoiceActivityDetector() {
  WebRtcVad_Create(&vad_handle_);
  Init();
  set_mode(0);
}

VoiceActivityDetector::VoiceActivityDetector(int mode) {
  WebRtcVad_Create(&vad_handle_);
  Init();
  set_mode(mode);
}

bool VoiceActivityDetector::Init() { return WebRtcVad_Init(vad_handle_); }

bool VoiceActivityDetector::set_mode(int mode) {
  return WebRtcVad_set_mode(vad_handle_, mode) == 0;
}

VoiceActivityDetector::VoiceActivity VoiceActivityDetector::Process(
    int fs, const int16_t* audio_frame, int frame_length) {
  switch (WebRtcVad_Process(vad_handle_, fs, audio_frame, frame_length)) {
    case 1:
      return kActiveVoice;
      break;
    case 0:
      return kNonActiveVoice;
      break;
    case -1:  // fall through
    default:
      return kError;
      break;
  }
}

VoiceActivityDetector::VoiceActivity VoiceActivityDetector::Process(
    int fs, const std::vector<int16_t>& audio_frame) {
  return Process(fs, audio_frame.data(), audio_frame.size());
}

bool VoiceActivityDetector::ValidRateAndFrameLength(int rate,
                                                    int frame_length) const {
  return WebRtcVad_ValidRateAndFrameLength(rate, frame_length) == 0;
}

VoiceActivityDetector::~VoiceActivityDetector() { WebRtcVad_Free(vad_handle_); }

Vad::Vad(int sample_rate, int frame_len_ms)
    : sample_rate_{sample_rate},
      frame_len_samples_{(frame_len_ms * sample_rate) / 1000},
      vad_{1} {
  if (!vad_.ValidRateAndFrameLength(sample_rate_, frame_len_samples_)) {
    throw std::runtime_error{"Invalid rate and frame length"};
  }
}

std::vector<bool> Vad::DetectSpeech(const int16_t* data, std::size_t data_sz) {
  std::size_t n_frames = data_sz / frame_len_samples_;
  std::vector<bool> vad_decisions(n_frames);
  auto* audio_samples = data;

  for (size_t i = 0; i < n_frames * frame_len_samples_;
       i += frame_len_samples_) {
    auto voice_activity =
        vad_.Process(sample_rate_, &audio_samples[i], frame_len_samples_);
    vad_decisions.push_back(voice_activity ==
                            tiro_speech::VoiceActivityDetector::kActiveVoice);
  }

  return vad_decisions;
}

bool Vad::HasSpeech(const int16_t* data, std::size_t data_sz) {
  if (static_cast<std::size_t>(frame_len_samples_) > data_sz) {
    // TODO(rkjaran): If received number of samples doesn't fit a frame we just
    //       classify it as speech. This should probably be fixed in the
    //       AudioSource...
    return true;
  }
  const std::vector<bool> vad_decisions = DetectSpeech(data, data_sz);
  std::size_t n_voiced =
      std::count(vad_decisions.cbegin(), vad_decisions.cend(), true);
  return n_voiced > 0;
}

bool Vad::HasSpeech(const std::vector<int16_t>& data) {
  return HasSpeech(&data[0], data.size());
}

bool Vad::HasSpeech(const VectorBase& waveform) {
  std::vector<int16_t> samples(waveform.Dim());
  for (int i = 0; i < waveform.Dim(); ++i) {
    samples[i] = static_cast<int16_t>(waveform(i));
  }
  return HasSpeech(samples);
}

}  // end namespace tiro_speech
