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
#include "src/audio/audio-source.h"

#include <feat/wave-reader.h>
#include <matrix/kaldi-vector.h>
#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <memory>
#include <numeric>
#include <string_view>

#include "src/audio/audio.h"
#include "src/base.h"
#include "src/logging.h"
#include "src/utils.h"

namespace tiro_speech {

ContentAudioSource::ContentAudioSource(const AudioSourceInfo& info,
                                       const Bytes& content,
                                       int target_sample_rate)
    : target_sample_rate_{target_sample_rate} {
  try {
    switch (info.encoding) {
      // Supported encodings
      case AudioEncoding::LINEAR16: {
        Linear16BytesToWaveVector(content, &data_);
        // TODO(rkjaran): Have the above fn do the resampling, similar to Mp3...
        if (info.sample_rate_hertz != target_sample_rate) {
          Vector new_data;
          ResampleWaveForm(info.sample_rate_hertz, data_, target_sample_rate,
                           &new_data);
          data_.Swap(&new_data);
        }
        break;
      }
      case AudioEncoding::MP3:
        [[fallthrough]];
      case AudioEncoding::FLAC: {
        CodedBytesToWaveVector(info.encoding, content, &data_,
                               target_sample_rate);
        break;
      }
      default:
        throw AudioSourceError{"Unsupported encoding"};
    }
  } catch (const av::AvLibraryError& ex) {
    throw AudioSourceError{"Could not process audio content"};
  }
}

void ContentAudioSource::Open() {
  // Do nothing...
  return;
}

bool ContentAudioSource::HasMoreChunks() const {
  return n_seen_elements_ < data_.Dim();
}

const AudioSourceItf::SubVector ContentAudioSource::NextChunk() {
  int range_idx = n_seen_elements_;
  n_seen_elements_ += chunk_size_in_elem;
  int length = std::min<int>(chunk_size_in_elem, data_.Dim() - range_idx);
  // XXX: origin in Range is zero-based, contrary to what the docs say.
  return data_.Range(range_idx, length);
}

FileAudioSource::FileAudioSource(const AudioSourceInfo& info,
                                 const std::string& path,
                                 int target_sample_rate)
    : file_path_{path},
      target_sample_rate_{target_sample_rate},
      sample_rate_{info.sample_rate_hertz},
      encoding_{info.encoding} {
  switch (info.encoding) {
    case AudioEncoding::LINEAR16:
      [[fallthrough]];
    case AudioEncoding::MP3:
      break;
    default:
      throw AudioSourceError{"Unsupported encoding for FileAudioSource"};
  }
}

void FileAudioSource::Open() {
  // XXX(rkjaran): First implementation... load everything into memory right
  //               away. This makes the implementation pretty much the same as
  //               ContentAudioSource

  if (opened_) {
    return;
  }

  std::ifstream is{file_path_};
  is.exceptions(std::ios::badbit | std::ios::failbit);

  switch (encoding_) {
    case AudioEncoding::LINEAR16: {
      bool has_riff = HasRiffHeader(is);
      if (!has_riff) {
        is.seekg(0);
      }

      // Get data size
      auto data_pos = is.tellg();
      is.seekg(0, std::ios::end);
      auto size = is.tellg() - data_pos;
      is.seekg(data_pos);

      // Parse data
      std::string content(size, '\0');
      is.read(&content[0], size);
      Linear16BytesToWaveVector(content, &data_);

      // Then resample if necessary
      if (sample_rate_ != target_sample_rate_) {
        Vector new_data;
        ResampleWaveForm(sample_rate_, data_, target_sample_rate_, &new_data);
        data_.Swap(&new_data);
      }
      break;
    }
    case AudioEncoding::FLAC:
      [[fallthrough]];
    case AudioEncoding::MP3: {
      is.seekg(std::ios::end);
      auto size = is.tellg();
      std::string content(size, '\0');
      is.read(&content[0], size);
      CodedBytesToWaveVector(encoding_, content, &data_, target_sample_rate_);
      break;
    }
    default:
      throw std::logic_error{"Should have been rejected by ctor"};
  }

  opened_ = true;
}

const AudioSourceItf::Vector& FileAudioSource::Full() { return data_; }

bool FileAudioSource::HasMoreChunks() const {
  return n_seen_elements_ < data_.Dim();
}

const AudioSourceItf::SubVector FileAudioSource::NextChunk() {
  int range_idx = n_seen_elements_;
  int length = std::min<int>(chunk_size_in_elements_, data_.Dim() - range_idx);
  n_seen_elements_ += length;
  return data_.Range(range_idx, length);
}

int FileAudioSource::ChunksSeen() const {
  return n_seen_elements_ / chunk_size_in_elements_;
}

int FileAudioSource::TotalChunks() const {
  return data_.Dim() / chunk_size_in_elements_;
}

UriAudioSource::UriAudioSource(const AudioSourceInfo& info, std::string uri,
                               int target_sample_rate)
    : uri_{std::move(uri)},
      source_sample_rate_{info.sample_rate_hertz},
      encoding_{info.encoding},
      target_sample_rate_{target_sample_rate} {}

void UriAudioSource::Open() {
  if (opened_) {
    return;
  }
  auto encoding_to_codec = [](AudioEncoding enc) {
    switch (enc) {
      case AudioEncoding::FLAC:
        [[fallthrough]];
      case AudioEncoding::MP3:
        return av::Codec::kMp3;
      default:
        throw AudioSourceError("UriAudioSource doesn't support this encoding.");
    }
  };
  try {
    decoder_ = std::make_unique<av::IoContextDecoder<av::UrlIoContext>>(
        encoding_to_codec(encoding_), av::UrlIoContext{uri_}, ss_,
        target_sample_rate_);
    if (decoder_ == nullptr) throw AudioSourceError{"Invalid audio source."};
    while (decoder_->PartialDecode(ss_))
      ;
    decoder_->Flush(ss_);

    opened_ = true;
  } catch (const tiro_speech::av::AvError& e) {
    TIRO_SPEECH_WARN("Rethrowing AvError as an AudioSourceError");
    throw AudioSourceError{e.what()};
  }
}

const Vector& UriAudioSource::Full() {
  if (!filled_) {
    while (HasMoreChunks()) {
      GetMoreData();
    }

    std::size_t full_dim = std::accumulate(
        subvectors_.begin(), subvectors_.end(), 0,
        [](std::size_t acc, const Vector& v) { return acc + v.Dim(); });

    // Wasteful, since it caches it twice in memory
    data_.Resize(full_dim, kaldi::kUndefined);
    for (std::size_t i = 0, start = 0; i < subvectors_.size(); ++i) {
      const Vector& v = subvectors_[i];
      data_.Range(start, v.Dim()).CopyFromVec(v);
      start += v.Dim();
    }
  }

  filled_ = true;
  return data_;
}

bool UriAudioSource::HasMoreChunks() const {
  if (filled_) {
    return ChunksSeen() < TotalChunks();
  }
  return !ss_.eof();
}

const AudioSourceItf::SubVector UriAudioSource::NextChunk() {
  if (filled_ && n_seen_elements_ < data_.Dim()) {  // prefetched data
    int range_idx = n_seen_elements_;
    int length = std::min<int>(chunk_size_in_elem_, data_.Dim() - range_idx);
    n_seen_elements_ += length;
    return data_.Range(range_idx, length);
  }

  SubVector added_data = GetMoreData();
  n_seen_elements_ += added_data.Dim();
  return added_data;
}

int UriAudioSource::ChunksSeen() const {
  return n_seen_elements_ / chunk_size_in_elem_;
}

int UriAudioSource::TotalChunks() const {
  using std::chrono::duration_cast;
  using std::chrono::seconds;
  // convert ms->chunks
  const int n_samples = duration_cast<seconds>(decoder_->Duration()).count() *
                        target_sample_rate_;
  return n_samples / chunk_size_in_elem_;
}

AudioSourceItf::SubVector UriAudioSource::GetMoreData() {
  std::string content(2 * chunk_size_in_elem_, '\0');
  ss_.read(&content[0], content.size());

  subvectors_.emplace_back(chunk_size_in_elem_);
  SubVector added_data = subvectors_.back().Range(0, chunk_size_in_elem_);
  Linear16BytesToWaveVector(content, &added_data);
  return added_data;
}

StreamingUriAudioSource::StreamingUriAudioSource(const AudioSourceInfo& info,
                                                 std::string uri,
                                                 int target_sample_rate,
                                                 int chunk_size_in_samples)
    : chunk_size_in_elem_{chunk_size_in_samples},
      max_buffer_size_{bytes_per_elem_ * chunk_size_in_elem_},
      uri_{std::move(uri)},
      source_sample_rate_{info.sample_rate_hertz},
      encoding_{info.encoding},
      target_sample_rate_{target_sample_rate} {}

void StreamingUriAudioSource::Open() {
  if (opened_) {
    return;
  }
  auto encoding_to_codec = [&](AudioEncoding enc) {
    switch (enc) {
      case AudioEncoding::FLAC:
        [[fallthrough]];
      case AudioEncoding::MP3:
        return av::Codec::kMp3;
      case AudioEncoding::GUESS:
        TIRO_SPEECH_DEBUG(
            "StreamingUriAudioSource guessing codec for uri: '{}'", uri_);
        return av::Codec::kUnknown;
      default:
        throw AudioSourceError(
            "StreamingUriAudioSource doesn't support this encoding.");
    }
  };
  try {
    decoder_ = std::make_unique<av::IoContextDecoder<av::UrlIoContext>>(
        encoding_to_codec(encoding_), av::UrlIoContext{uri_}, ss_,
        target_sample_rate_);
    if (decoder_ == nullptr) throw AudioSourceError{"Invalid audio source."};
    opened_ = true;
  } catch (const tiro_speech::av::AvError& e) {
    TIRO_SPEECH_WARN("Rethrowing AvError as an AudioSourceError");
    throw AudioSourceError{e.what()};
  }
}

const Vector& StreamingUriAudioSource::Full() {
  throw AudioSourceError{"Not implemented for streaming audio source"};
}

bool StreamingUriAudioSource::HasMoreChunks() const { return !no_more_chunks_; }

const AudioSourceItf::SubVector StreamingUriAudioSource::NextChunk() {
  // TODO(rkjaran): clean this up...
  // TODO(rkjaran): decrease dynamic allocations
  for (; !no_more_to_decode_;) {
    if (!decoder_->PartialDecode(ss_)) {
      TIRO_SPEECH_DEBUG("Decoder reached EOF, flushing.");
      decoder_->Flush(ss_);
      no_more_to_decode_ = true;
      break;
    }
    if (ss_.rdbuf()->in_avail() >= max_buffer_size_) {
      break;
    }
  }

  const int n_bytes_ready = ss_.rdbuf()->in_avail();
  const int n_samples_ready = n_bytes_ready / 2;
  const int n_bytes_to_read = std::min(2 * n_samples_ready, max_buffer_size_);
  assert(n_bytes_to_read % 2 == 0);

  std::string content(n_bytes_to_read, '\0');
  const auto actual_size = ss_.readsome(&content[0], n_bytes_to_read);

  if (actual_size == 0) {
    if (n_bytes_ready == 1 && ss_.tellg() != -1) {
      // XXX: Unclog the drain
      ss_.get();
      ss_.unget();
    } else if (no_more_to_decode_) {
      TIRO_SPEECH_DEBUG("No more chunks.");
      no_more_chunks_ = true;
    }
    return data_.Range(0, 0);
  }
  content.resize(actual_size);
  const int actual_chunk_size_in_elem = actual_size / bytes_per_elem_;
  data_.Resize(actual_chunk_size_in_elem, kaldi::kUndefined);
  Linear16BytesToWaveVector(content, &data_);
  n_seen_elements_ += actual_chunk_size_in_elem;

  return data_.Range(0, actual_chunk_size_in_elem);
}

int StreamingUriAudioSource::ChunksSeen() const {
  return n_seen_elements_ / chunk_size_in_elem_;
}

int StreamingUriAudioSource::TotalChunks() const {
  using std::chrono::duration_cast;
  using std::chrono::seconds;
  // convert ms->chunks
  const int n_samples = duration_cast<seconds>(decoder_->Duration()).count() *
                        target_sample_rate_;
  return n_samples / chunk_size_in_elem_;
}

std::unique_ptr<AudioSourceItf> CreateAudioSourceFromUri(
    const AudioSourceInfo& info, const std::string& uri) {
  if (info.type != AudioSourceInfo::Type::kUri) {
    return nullptr;
  }

  return std::make_unique<StreamingUriAudioSource>(info, uri);
}

bool IsUriSupported(const std::string_view uri) {
  static std::vector<std::string> supported_uri_schemes;
  if (supported_uri_schemes.empty()) {
#ifdef TIRO_SPEECH_ENABLE_FILE_URI
    TIRO_SPEECH_WARN("Enabling file:// scheme for URIs. This is NOT SAFE.");
    supported_uri_schemes.emplace_back("file");
#endif  // TIRO_SPEECH_ENABLE_FILE_URI
#ifdef TIRO_SPEECH_ENABLE_HTTP_URI
    supported_uri_schemes.emplace_back("http");
    supported_uri_schemes.emplace_back("https");
#endif  // TIRO_SPEECH_ENABLE_HTTP_URI
  }
  auto nend_scheme = uri.find("://");
  if (nend_scheme == std::string_view::npos) {
    return false;
  }
  auto scheme = uri.substr(0, nend_scheme);
  return std::any_of(supported_uri_schemes.cbegin(),
                     supported_uri_schemes.cend(),
                     [=](auto _scheme) { return scheme == _scheme; });
}

}  // namespace tiro_speech
