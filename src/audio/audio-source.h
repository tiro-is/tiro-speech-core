#ifndef TIRO_SPEECH_SRC_AUDIO_AUDIO_SOURCE_H_
#define TIRO_SPEECH_SRC_AUDIO_AUDIO_SOURCE_H_

#include <matrix/kaldi-vector.h>

#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

#include "src/audio/audio.h"
#include "src/audio/ffmpeg-wrapper.h"
#include "src/utils.h"

namespace tiro_speech {

struct AudioSourceError : public std::runtime_error {
  explicit AudioSourceError(const std::string& msg) : runtime_error{msg} {}
};

/**
 * Base struct for AudioSourceInfos
 */
struct AudioSourceInfo {
  enum class Type { kNoType = 0, kContent, kUri };  // XXX: this is very ugly...
  const Type type = Type::kNoType;
  AudioEncoding encoding = AudioEncoding::ENCODING_UNSPECIFIED;
  int sample_rate_hertz = 0;
};

// TODO(rkjaran): Split up AudioSourceItf into two classes,
//                AudioSourceReaderBase and AudioSourceWriterBase.

/** \class AudioSourceItf
 * \brief  Generic audio source - base class
 *
 * Inheriting classes are implementations for different schemes and types of
 * audio sources.
 *
 * The typical usage of this would be through the functions
 * CreateAudioSourceFromUri() and CreateAudioSource().
 */
class AudioSourceItf {
 public:
  using Vector = kaldi::Vector<kaldi::BaseFloat>;
  using SubVector = kaldi::SubVector<kaldi::BaseFloat>;
  using Bytes = std::string;

  /**
   * Open the audio source for reading, safe to call multiple times.
   */
  virtual void Open() = 0;

  /**
   * Returns full waveform vector, if possible. For a streaming source this will
   * seek until the end of stream.
   */
  virtual const Vector& Full() = 0;

  /**
   * Returns true if it is possible to call NextChunk()
   */
  [[nodiscard]] virtual bool HasMoreChunks() const = 0;

  /**
   * Return view of next available chunk
   */
  virtual const SubVector NextChunk() = 0;

  /**
   * Returns true for a streaming source.
   */
  [[nodiscard]] virtual bool IsStreamed() const { return false; }

  /**
   *  Should be equal to number of calls to NextChunk()
   */
  [[nodiscard]] virtual int ChunksSeen() const = 0;

  /**
   * Returns the total number of chunks for non streaming sources.
   */
  [[nodiscard]] virtual int TotalChunks() const = 0;

  [[nodiscard]] virtual std::chrono::milliseconds TimePassed() const = 0;

  virtual ~AudioSourceItf() = default;
};

// /** \class QueueAudioSourceItf
//  *
//  *
//  */
// class QueueAudioSourceItf
//     : public ThreadSafeQueue<AudioSourceItf::Bytes> {
//  public:
//   virtual ~QueueAudioSourceItf() {};
// };

/** \class ContentAudioSource
 *
 * This is used when we have the *content* of the audio source instead of a path
 * or URI. An instance of this class loads all content into memory at the moment
 * of construction and Open() does nothing.
 *
 * @TODO: Decrease the amount of copying being done.
 */
class ContentAudioSource : public AudioSourceItf {
 public:
  explicit ContentAudioSource(const AudioSourceInfo& info, const Bytes& content,
                              int target_sample_rate = 16000);

  void Open() override;
  const Vector& Full() override { return data_; };
  bool HasMoreChunks() const override;
  const SubVector NextChunk() override;
  int ChunksSeen() const override {
    return n_seen_elements_ / chunk_size_in_elem;
  };
  int TotalChunks() const override { return data_.Dim() / chunk_size_in_elem; };
  std::chrono::milliseconds TimePassed() const override {
    return std::chrono::milliseconds{1000 * n_seen_elements_ /
                                     target_sample_rate_};
  }

 private:
  const size_t chunk_size_in_elem = 400;
  std::int64_t n_seen_elements_ = 0;
  std::chrono::milliseconds total_time_;
  Vector data_;
  int target_sample_rate_;

  friend std::unique_ptr<AudioSourceItf> CreateAudioSource(
      const AudioSourceInfo& info,
      std::unique_ptr<AudioSourceItf::Bytes> content);
};

class FileAudioSource : public AudioSourceItf {
 public:
  /**
   * If \param info is not provided we try to get the relevent information from
   * the file header if supported.
   */
  FileAudioSource(const AudioSourceInfo& info, const std::string& path,
                  int target_sample_rate = 16000);

  virtual ~FileAudioSource() = default;

  void Open() override;
  const Vector& Full() override;
  bool HasMoreChunks() const override;
  const SubVector NextChunk() override;
  int ChunksSeen() const override;
  int TotalChunks() const override;

  std::chrono::milliseconds TimePassed() const override {
    return std::chrono::milliseconds{1000 * n_seen_elements_ /
                                     target_sample_rate_};
  }

 private:
  Vector data_;
  std::string file_path_;
  const int chunk_size_in_elements_ = 400;
  std::int64_t n_seen_elements_ = 0;
  const int target_sample_rate_;
  const int sample_rate_;
  bool opened_ = false;
  AudioEncoding encoding_;
};

class UriAudioSource : public AudioSourceItf {
 public:
  UriAudioSource(const AudioSourceInfo& info, std::string uri,
                 int target_sample_rate = 16000);
  ~UriAudioSource() = default;

  void Open() override;
  const Vector& Full() override;
  bool HasMoreChunks() const override;
  const SubVector NextChunk() override;
  int ChunksSeen() const override;
  int TotalChunks() const override;
  std::chrono::milliseconds TimePassed() const override {
    return std::chrono::milliseconds{1000 * n_seen_elements_ /
                                     target_sample_rate_};
  }

 private:
  const size_t chunk_size_in_elem_ = 400;
  std::int64_t n_seen_elements_ = 0;
  std::chrono::milliseconds total_time_{0};
  bool opened_ = false;
  bool filled_ = false;
  std::stringstream ss_;  // TODO(rkjaran): Use custom preallocated buffer
  const std::string uri_;
  const int source_sample_rate_;
  const AudioEncoding encoding_;
  Vector data_;
  std::vector<Vector> subvectors_;
  const int target_sample_rate_;
  std::unique_ptr<av::IoContextDecoder<av::UrlIoContext>> decoder_ = nullptr;

  SubVector GetMoreData();
};

class StreamingUriAudioSource : public AudioSourceItf {
 public:
  StreamingUriAudioSource(const AudioSourceInfo& info, std::string uri,
                          int target_sample_rate = 16000,
                          int chunk_size_in_samples = 2048);

  void Open() override;
  const Vector& Full() override;
  bool HasMoreChunks() const override;
  const SubVector NextChunk() override;
  int ChunksSeen() const override;
  int TotalChunks() const override;
  std::chrono::milliseconds TimePassed() const override {
    return std::chrono::milliseconds{1000 * n_seen_elements_ /
                                     target_sample_rate_};
  }
  bool IsStreamed() const override { return true; };

 private:
  constexpr static int bytes_per_elem_ = 2;
  const int chunk_size_in_elem_;
  const int max_buffer_size_;
  std::int64_t n_seen_elements_ = 0;
  std::chrono::milliseconds total_time_{0};
  bool opened_ = false;
  bool no_more_to_decode_ = false;
  bool no_more_chunks_ = false;
  std::stringstream ss_;  // TODO(rkjaran): Use custom preallocated buffer
  const std::string uri_;
  const int source_sample_rate_;
  const AudioEncoding encoding_;
  Vector data_;
  const int target_sample_rate_;
  std::unique_ptr<av::IoContextDecoder<av::UrlIoContext>> decoder_ = nullptr;
};

/**
 * \brief Creates an audio source from a URI, throwing AudioSourceError if the
 *        URI is malformed or URI scheme is not supported.
 *
 * Example usage:
 *
 * \code
 *  auto* audio_src = CreateAudioSourceFromUri(
 *      AudioSourceInfo{AudioSourceInfo::Type::kUri,
 *                      AudioEncoding::LINEAR16, 16000},
 *                      "http://example.com/example.pcm");
 *  // Open the connection to the HTTP connection
 *  audio_src->Open();
 *  // Read chunks of audio from the connection, with no checks for validity
 *  while (audio_src->HasMoreChunks()) {
 *    AudioSourceItf::SubVector chunk = audio_src->NextChunk();
 *    // do stuff with the chunk...
 *  }
 * \endcode
 *
 * This will create a HttpAudioSource for a LINEAR 16 PCM file through HTTP.
 */
std::unique_ptr<AudioSourceItf> CreateAudioSourceFromUri(
    const AudioSourceInfo& info, const std::string& uri);

/**
 * \brief Create audio source from a content byte buffer.
 *
 * Example usage, creates a ContentAudioSource for a LINEAR16 PCM byte buffer:
 *
 * \code
 *  std::string content = some bytes...;
 *  auto* audio_src = CreateAudioSource(
 *      AudioSourceInfo{AudioSourceInfo::Type::kContent,
 *                      AudioEncoding::LINEAR16, 16000},
 *                      content);
 *  // Open the audio stream (a no-op for ContentAudioSource)
 *  audio_src->Open();
 *  // Read chunks of audio from the connection, with no checks for validity
 *  while (audio_src->HasMoreChunks()) {
 *    AudioSourceItf::SubVector chunk = audio_src->NextChunk();
 *    // do stuff with the chunk...
 *  }
 * \endcode
 */
std::unique_ptr<AudioSourceItf> CreateAudioSource(
    const AudioSourceInfo& info, const ContentAudioSource::Bytes& content);

/**
 * Check whether URI (i.e. the scheme part) is supported by
 * \a CreateAudioSourceFromUri
 */
bool IsUriSupported(const std::string_view uri);

}  // namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_AUDIO_AUDIO_SOURCE_H_
