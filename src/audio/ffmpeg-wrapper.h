// TODO(rkjaran): Refactor
#ifndef TIRO_SPEECH_SRC_AUDIO_FFMPEG_WRAPPER_H_
#define TIRO_SPEECH_SRC_AUDIO_FFMPEG_WRAPPER_H_

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <istream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include "src/logging.h"
#include "src/utils.h"

namespace tiro_speech::av {

/** Base time units for libav.
 *
 * Useful for type-safe conversion to other chrono time units.
 */
using av_time_units =
    std::chrono::duration<std::uint64_t, std::ratio<1, AV_TIME_BASE>>;

struct AvError : public std::runtime_error {
  explicit AvError(const std::string& msg) : std::runtime_error{msg} {};
};

struct AvLibraryError : public AvError {
  using ErrnumT = int;
  explicit AvLibraryError(ErrnumT errnum);
  ErrnumT Errnum() const;

 private:
  ErrnumT errnum_;
  std::string StrError(ErrnumT errnum) const;
};

/** Call(...) - Wrappers for init or alloc functions returning a ptr.
 *
 * Throws AvError with message \p err_msg if (*fp)() returns a nullptr.
 */
template <class Thing>
auto Call(Thing* (*fp)(), const char* err_msg = "Unknown libavcodec error")
    -> Thing* {
  Thing* thing = nullptr;
  thing = (*fp)();
  if (thing == nullptr) {
    throw AvError{err_msg};
  }

  return thing;
}

template <class Thing, class Param>
auto Call(Thing* (*fp)(Param), Param param,
          const char* err_msg = "Unknown libavcodec error") -> Thing* {
  Thing* thing = nullptr;
  thing = (*fp)(param);
  if (thing == nullptr) {
    throw AvError{err_msg};
  }

  return thing;
}

template <class Thing, class... Params>
auto Call(Thing* (*fp)(Params...), const char* err_msg, Params... params)
    -> Thing* {
  Thing* thing = nullptr;
  thing = (*fp)(params...);
  if (thing == nullptr) {
    throw AvError{err_msg};
  }

  return thing;
}

/** A call that returns a positive integer or any of the negative integers in
 * the input iterator. Throws AvLibraryError on any other negative integers.
 */
template <class InputIt, class... Params>
int CallRet2(InputIt first, InputIt last, int (*fp)(Params...),
             Params... params) {
  // Handle expected non-exceptional errors
  int ret = (*fp)(params...);
  if (ret < 0 &&
      std::none_of(first, last, [ret](auto i) { return i == ret; })) {
    throw AvLibraryError{ret};
  }
  return ret;
}

/** A call that returns a zero or positive integer, throwing on any other value.
 */
template <class... Params>
[[maybe_unused]] int CallRet(int (*fp)(Params...), Params... params) {
  constexpr std::array<int, 0> normal_flow_errors{};
  return CallRet2(cbegin(normal_flow_errors), cend(normal_flow_errors), fp,
                  params...);
}

/** A call that can return a positive int or an AVERROR_EOF, throwing on any
 * other value.
 */
template <class... Params>
int CallRetEof(int (*fp)(Params...), Params... params) {
  constexpr std::array<int, 1> normal_flow_errors{AVERROR_EOF};
  return CallRet2(cbegin(normal_flow_errors), cend(normal_flow_errors), fp,
                  params...);
}

/**
 * Codecs supported by the wrapper, i.e. by CodecContext and therefore Decoder.
 */
enum class Codec {
  kUnknown = 0x0001,
  kMp3 = 0x0002,
  // XXX: Do not use because of a bug that cuts off last 2s.
  kFlac,
  kMulaw,
  kAlaw,
  kAmrNb,
  kAmrWb,
  kOpus,
  kMpeg4,
};

/**
 * Convert from local codec to internal codec ID
 */
inline enum ::AVCodecID CodecCast(::tiro_speech::av::Codec codec) {
  switch (codec) {
    case decltype(codec)::kMp3:
      return AV_CODEC_ID_MP3;
    case decltype(codec)::kFlac:
      return AV_CODEC_ID_FLAC;
    case decltype(codec)::kMulaw:
      return AV_CODEC_ID_PCM_MULAW;
    case decltype(codec)::kAlaw:
      return AV_CODEC_ID_PCM_ALAW;
    case decltype(codec)::kAmrNb:
      return AV_CODEC_ID_AMR_NB;
    case decltype(codec)::kAmrWb:
      return AV_CODEC_ID_AMR_WB;
    case decltype(codec)::kOpus:
      return AV_CODEC_ID_OPUS;
    case decltype(codec)::kMpeg4:
      return AV_CODEC_ID_MPEG4;
    case decltype(codec)::kUnknown:
      [[fallthrough]];
    default:
      throw AvError{"Unknown codec"};
  }
}

/**
 * Convert from internal codec ID to local codec
 */
inline ::tiro_speech::av::Codec CodecCast(enum ::AVCodecID av_codec_id) {
  switch (av_codec_id) {
    case AV_CODEC_ID_MP3:
      return Codec::kMp3;
    case AV_CODEC_ID_FLAC:
      return Codec::kFlac;
    case AV_CODEC_ID_PCM_MULAW:
      return Codec::kMulaw;
    case AV_CODEC_ID_PCM_ALAW:
      return Codec::kAlaw;
    case AV_CODEC_ID_AMR_NB:
      return Codec::kAmrNb;
    case AV_CODEC_ID_AMR_WB:
      return Codec::kAmrWb;
    case AV_CODEC_ID_OPUS:
      return Codec::kOpus;
    case AV_CODEC_ID_MPEG4:
      return Codec::kMpeg4;
    default:
      throw AvError{"Unknown/unsupported AVCodecID"};
  }
}

/**
 * Thin lifetime management for structs from libavcodec.
 *
 * Public interface:
 * \code
 *   class Wrapper {
 *     /// Returns a pointer to underlying struct
 *     Wrapper* get();
 *     /// Dereference operator defined for convencience, same as get()->
 *     Wrapper* operator->();
 *   };
 * \endcode
 */

/** \class Packet
 *
 * Lifetime managing wrapper around AVPacket
 */
class Packet final : no_copy_or_move {
 public:
  Packet() : packet_{Call(&av_packet_alloc, "AVPacket alloc failure.")} {}

  AVPacket* operator->() const noexcept { return packet_; }

  AVPacket* get() const noexcept { return packet_; }

  ~Packet() noexcept { av_packet_free(&packet_); }

 private:
  AVPacket* packet_;
};

/** \class Frame
 *
 *  Lifetime management wrapper around AVFrame
 */
class Frame final : no_copy_or_move {
 public:
  Frame() : frame_{Call(&av_frame_alloc, "AVFrame alloc failure.")} {}

  AVFrame* operator->() const noexcept { return frame_; }

  AVFrame* get() const noexcept { return frame_; }

  ~Frame() { av_frame_free(&frame_); }

 private:
  AVFrame* frame_;
};

/** \class CodecContext
 *  Lifetime management wrapper around AVCodecContext
 */
class CodecContext final {
 public:
  explicit CodecContext(AVCodec* codec_ptr,
                        const AVCodecParameters* parameters = nullptr)
      : codec_ptr_{codec_ptr}, ptr_{Call(&avcodec_alloc_context3, codec_ptr_)} {
    if (parameters != nullptr) {
      CallRet(&avcodec_parameters_to_context, ptr_, parameters);
      ptr_->channel_layout = av_get_default_channel_layout(ptr_->channels);
    }
    CallRet(&avcodec_open2, ptr_, codec_ptr_,
            static_cast<AVDictionary**>(nullptr));
  }

  explicit CodecContext(Codec codec)
      : codec_ptr_{Call(&avcodec_find_decoder, CodecCast(codec))},
        ptr_{Call(&avcodec_alloc_context3, codec_ptr_)} {
    // TODO(rkjaran): Check whether we actually get this sample_fmt, and convert
    //                otherwise.
    CallRet(&avcodec_open2, ptr_, codec_ptr_,
            static_cast<AVDictionary**>(nullptr));
  }

  CodecContext(const CodecContext& other) = delete;
  CodecContext& operator=(const CodecContext& other) = delete;

  CodecContext(CodecContext&& other) {
    codec_ptr_ = other.codec_ptr_;
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
  }

  AVCodecContext* operator->() const noexcept { return ptr_; }

  AVCodecContext* get() const noexcept { return ptr_; }

  ~CodecContext() noexcept {
    if (ptr_) avcodec_free_context(&ptr_);
  }

 private:
  const AVCodec* codec_ptr_;
  AVCodecContext* ptr_;

  friend class Parser;
};

/** \class InputBuffer
 *
 * Automatically refillable input buffer reading from an input stream
 *
 * Typical usage:
 *
 * \code
 *   InputBuffer inbuf{input_stream};
 *   Parser parser{Codec::kMp3};
 *   Packet packet;
 *   parser.Parse(packet, inbuf);
 * \endcode
 */
template <std::size_t kBufferSize = 9480,
          std::size_t kPaddingSize = AV_INPUT_BUFFER_PADDING_SIZE,
          std::size_t kRefillThreshold = 4096>
class InputBuffer final : no_copy_or_move {
 public:
  explicit InputBuffer(std::istream& input_stream)
      : input_stream_{input_stream} {
    input_stream_.exceptions(std::ios::badbit);
    if (!ReadMore()) {
      throw AvError{"Unexpected EOF"};
    }
  }

  /**
   * Returns a pointer to the *current data* in the buffer
   */
  const uint8_t* Data() const { return &buf_[cur_offset_]; }

  /**
   * Returns number of bytes currently available from Data()
   */
  std::size_t DataSize() const { return cur_data_size_; }

  /**
   * Returns true if buffer is empty and underlying stream is depleted.
   */
  bool Empty() { return input_stream_.eof() && DataSize() <= 0; }

  /** Go forward in stream by n_used bytes.
   *
   * \param[in] n_used  Number of bytes used from buffer, i.e. from Data().
   *
   * \warning Calling this after buffer has been depleted, (i.e. Empty() ==
   *          true) is undefined behaviour
   */
  void Forward(std::size_t n_used) {
    assert(n_used >= 0);
    assert(n_used <= kBufferSize);
    assert(n_used <= DataSize());
    assert(!Empty());

    total_used_bytes_ += n_used;

    cur_offset_ += n_used;
    cur_data_size_ -= n_used;

    if (DataSize() < kRefillThreshold) {
      if (DataSize() != 0) {
        std::copy(std::begin(buf_) + cur_offset_,
                  std::begin(buf_) + cur_offset_ + DataSize(),
                  std::begin(buf_));
      }
      ReadMore();
      cur_offset_ = 0;
    }
  }

 private:
  std::istream& input_stream_;
  std::array<uint8_t, kBufferSize + kPaddingSize> buf_;  //< Main buffer
  int cur_offset_ = 0;                                   //< Current pos in buf
  int cur_data_size_ = 0;
  int total_bytes_read_ = 0;
  int total_used_bytes_ = 0;

  /// Read up to kBufferSize bytes from input stream and store in buffer.
  bool ReadMore() {
    auto data = reinterpret_cast<char*>(&buf_[DataSize()]);
    input_stream_.read(data, kBufferSize - DataSize());
    int n_read = input_stream_.gcount();
    total_bytes_read_ += n_read;
    if (n_read == 0) {
      return false;
    }
    cur_data_size_ = DataSize() + n_read;
    return true;
  }
};

class Parser final : no_copy_or_move {
 public:
  explicit Parser(Codec codec)
      : codec_ctx_{codec},
        parser_ctx_ptr_{Call(&av_parser_init,
                             static_cast<int>(codec_ctx_.codec_ptr_->id),
                             "AVParser init failure.")} {}

  template <class InputBufferT>
  void Parse(Packet& packet, InputBufferT& inbuf) {
    assert(!inbuf.Empty());

    int n_used = CallRet(&av_parser_parse2, parser_ctx_ptr_, codec_ctx_.get(),
                         &packet->data, &packet->size, inbuf.Data(),
                         static_cast<int>(inbuf.DataSize()), AV_NOPTS_VALUE,
                         AV_NOPTS_VALUE, static_cast<std::int64_t>(0));
    inbuf.Forward(n_used);
  }

  void Flush(Packet& packet) {
    int n_used = CallRet(
        &av_parser_parse2, parser_ctx_ptr_, codec_ctx_.get(), &packet->data,
        &packet->size, static_cast<const std::uint8_t*>(nullptr), 0,
        AV_NOPTS_VALUE, AV_NOPTS_VALUE, static_cast<std::int64_t>(0));
    (void)n_used;
    assert(n_used == 0);
  }

  ~Parser() noexcept { av_parser_close(parser_ctx_ptr_); }

 private:
  CodecContext codec_ctx_;
  AVCodecParserContext* parser_ctx_ptr_;

  friend class Decoder;
};

/** \class SampleConverter - Thin wrapper around libswresample
 *
 * A SampleConverter handles conversion from various intermediate formats used
 * by libavcodec to the standard format used in this project.
 *
 * User has is required to call SampleConverter::Init(ctx) when the CodecContext
 * ctx has been used to successfully decode a \class Frame.
 */
class SampleConverter final : no_copy_or_move {
 public:
  using OutputSampleT = std::int16_t;
  using InputSampleT = char;
  using OutputStream = std::basic_ostream<OutputSampleT>;
  using InputStream = std::basic_istream<InputSampleT>;

  // TODO(rkjaran): Support different target formats and sample rates.
  SampleConverter();

  explicit SampleConverter(int sample_rate_hertz);

  /** Initialize the converter with input info from \param codec_ctx
   *
   * \param[in] codec_ctx CodecContext that's been used to successfully decode a
   *                      \class Frame, i.e. has channel_layout, sample_fmt,
   * etc.
   */
  void Init(const Frame& in_frame);

  /** Check whether converter is in a legal state.
   */
  bool IsInitialized() const;

  /** Convert \param frame in one format and/or sample rate to the standard one.
   *
   * \param[in]  frame  Frame to be converted
   *
   * \return A reference to the converted frame
   *
   * \warning The returned reference is updated on the next call to Convert() or
   *          Flush().
   */
  Frame& Convert(const Frame& frame);

  /** Signal end of conversion and flush conversion buffer
   *
   * \return  A reference to the last remaining converted frame, if any.
   */
  Frame& Flush();

  int OutputSampleRate() const { return kDefaultSampleRate; };

  AVSampleFormat OutputSampleFmt() const {
    return static_cast<AVSampleFormat>(out_frame_->format);
  };

  SwrContext* get() const noexcept { return swr_ctx_; }

  Frame& get_out_frame() { return out_frame_; }

  ~SampleConverter() noexcept {
    if (swr_ctx_ != nullptr) {
      swr_free(&swr_ctx_);
    }
  }

 private:
  Frame out_frame_;
  SwrContext* swr_ctx_ = nullptr;
  AVSampleFormat out_sample_fmt_ = AV_SAMPLE_FMT_S16;
};

template <typename InputBufferT = InputBuffer<>>
class IoContext final {
 public:
  IoContext(InputBufferT& inbuf)
      : ptr_{Call<AVIOContext, unsigned char*, int, int, void*,
                  int (*)(void* opaque, std::uint8_t*, int),
                  int (*)(void*, std::uint8_t*, int),
                  std::int64_t (*)(void*, std::int64_t, int)>(
            &avio_alloc_context, "Failed to allocate avio context.", buf_,
            buf_sz_, 0, static_cast<void*>(&inbuf),
            [](void* opaque, uint8_t* buf, int buf_size) {
              auto* inbuf = reinterpret_cast<InputBufferT*>(opaque);
              if (inbuf->Empty()) {
                return AVERROR_EOF;
              }
              auto n_read = std::min<int>(inbuf->DataSize(), buf_size);
              std::copy(inbuf->Data(), inbuf->Data() + n_read, buf);
              inbuf->Forward(n_read);

              return n_read;
            },
            nullptr, nullptr)} {}

  AVIOContext* get() const noexcept { return ptr_; }

  ~IoContext() noexcept {
    if (ptr_) {
      av_freep(&ptr_->buffer);
      av_freep(&ptr_);
    }
  }

 private:
  constexpr static std::size_t buf_sz_ = 4096;
  std::uint8_t* buf_{static_cast<std::uint8_t*>(Call(&av_malloc, buf_sz_))};
  AVIOContext* ptr_;
};

class UrlIoContext final {
 public:
  explicit UrlIoContext(const std::string& url) {
    AVDictionary* options = nullptr;
    av_dict_set_int(&options, "multiple_requests", 1, 0);
    av_dict_set_int(&options, "reconnect", 1, 0);
    av_dict_set_int(&options, "icy", 0, 0);
    CallRet(&avio_open2, &ptr_, url.c_str(), AVIO_FLAG_READ,
            static_cast<const AVIOInterruptCB*>(nullptr), &options);
    TIRO_SPEECH_DEBUG("Did not understand {} opts", av_dict_count(options));
    if (options != nullptr) av_dict_free(&options);
    // CallRet(&avio_open, &ptr_, url.c_str(), AVIO_FLAG_READ);
    if (ptr_ == nullptr) throw AvError{"Could not read URL resource."};
  }

  UrlIoContext(UrlIoContext&& other) {
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
  }

  UrlIoContext& operator=(UrlIoContext&& other) {
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  AVIOContext* get() const noexcept { return ptr_; }

  ~UrlIoContext() noexcept {
    if (ptr_ != nullptr) {
      CallRet(&avio_closep, &ptr_);
    }
  }

 private:
  AVIOContext* ptr_ = nullptr;
};

class FormatContext final {
 public:
  FormatContext() : ptr_{Call(&avformat_alloc_context)} {}

  FormatContext(FormatContext&& other) {
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
  }

  FormatContext& operator=(FormatContext&& other) {
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  AVFormatContext* get() noexcept { return ptr_; }

  AVFormatContext* operator->() const { return ptr_; }

  AVFormatContext* operator->() { return ptr_; }

  AVFormatContext** operator&() { return &ptr_; }

  ~FormatContext() noexcept {
    if (ptr_ != nullptr) {
      avformat_close_input(&ptr_);
    }
  }

 private:
  AVFormatContext* ptr_;
};

void PartialDecodeImpl(SampleConverter& sample_converter,
                       CodecContext& codec_ctx, Frame& frame, Packet& packet,
                       std::ostream& os, int audio_stream_index, bool flush);

template <class IoContextT>
class IoContextDecoder final {
  static_assert(std::is_move_constructible<IoContextT>::value,
                "IoContextT has to be move constructible");

 public:
  IoContextDecoder(Codec codec, IoContextT&& io_ctx, std::ostream& os,
                   int target_sample_rate_hertz = kDefaultSampleRate);

  /** Decode bytes and possibly write them to \a os
   *
   * \param[out] os   Decoded stream of bytes.
   *
   * \return false if input stream has been depleted, true otherwise.
   */
  bool PartialDecode(std::ostream& os);

  /** Flush decoder
   *
   * A call to Flush() will signal the decoder to finish decoding. Usually
   * called after PartialDecode() returns false.
   *
   * \param[out] os  Decoded stream of bytes
   */
  void Flush(std::ostream& os);

  /** Decode bytes and possibly write them to stream given to constructor
   *
   * \param[out] os   Decoded stream of bytes.
   *
   * \return false if input stream has been depleted, true otherwise.
   */
  bool PartialDecode();

  /** Flush decoder
   *
   * A call to Flush() will signal the decoder to finish decoding and writes
   * remaining bytes to stream. Usually called after PartialDecode() returns
   * false.
   */
  void Flush();

  AVSampleFormat OutputSampleFmt() const;

  std::chrono::milliseconds Duration() const;

 private:
  IoContextT io_ctx_;
  Packet packet_;
  Frame frame_;
  Frame out_frame_;
  FormatContext fmt_ctx_;
  CodecContext codec_ctx_;
  std::ostream& os_;
  const int target_sample_rate_;
  SampleConverter sample_converter_;
  int total_packet_size_ = 0;
  int audio_stream_index_;

  /** Decode bytes and write them to \a os
   *
   * This method perfoms decoding without parsing a new Packet from inbuf_. Used
   * in PartialDecode() (which does parsing) and Flush().
   *
   * \param[out] os   Decoded stream of bytes.
   */
  void PartialDecodeImpl(Packet& packet, std::ostream& os, bool flush = false);
};

template <class IoContextT>
IoContextDecoder<IoContextT>::IoContextDecoder(Codec codec, IoContextT&& io_ctx,
                                               std::ostream& os,
                                               int target_sample_rate_hertz)
    : io_ctx_{std::move(io_ctx)},
      fmt_ctx_{},
      codec_ctx_{std::move(([&]() -> CodecContext {
        fmt_ctx_->flags |= AVFMT_FLAG_GENPTS;
        fmt_ctx_->pb = io_ctx_.get();
        CallRet<AVFormatContext**, const char*, AVInputFormat*, AVDictionary**>(
            &avformat_open_input, &fmt_ctx_, nullptr, nullptr, nullptr);
        CallRet(&avformat_find_stream_info, fmt_ctx_.get(),
                static_cast<AVDictionary**>(nullptr));
        av_dump_format(fmt_ctx_.get(), 0, "IoContext", 0);
        if (codec != Codec::kUnknown) {
          audio_stream_index_ =
              CallRet(&av_find_best_stream, fmt_ctx_.get(), AVMEDIA_TYPE_AUDIO,
                      -1, -1, static_cast<AVCodec**>(nullptr), 0);
          return CodecContext{codec};
        }

        AVCodec* av_codec;
        audio_stream_index_ = CallRet(&av_find_best_stream, fmt_ctx_.get(),
                                      AVMEDIA_TYPE_AUDIO, -1, -1, &av_codec, 0);
        return CodecContext{av_codec,
                            fmt_ctx_->streams[audio_stream_index_]->codecpar};
      })())},
      os_{os},
      target_sample_rate_{target_sample_rate_hertz},
      sample_converter_{target_sample_rate_} {}

template <class IoContextT>
bool IoContextDecoder<IoContextT>::PartialDecode(std::ostream& os) {
  constexpr std::array<int, 2> normal_flow_errors{AVERROR_EOF, AVERROR(EAGAIN)};
  int ret = CallRet2(cbegin(normal_flow_errors), cend(normal_flow_errors),
                     &av_read_frame, fmt_ctx_.get(), packet_.get());

  if (packet_->size > 0 && packet_->data != nullptr) {
    total_packet_size_ += packet_->size;
    PartialDecodeImpl(packet_, os);
  }

  if (io_ctx_.get()->error != 0) {
    std::string str_error;
    str_error.resize(AV_ERROR_MAX_STRING_SIZE);
    av_strerror(io_ctx_.get()->error, &str_error[0], AV_ERROR_MAX_STRING_SIZE);
    TIRO_SPEECH_DEBUG("PartialDecode had an IoContext error: {}", str_error);
  }

  av_packet_unref(packet_.get());

  if (ret == AVERROR(EAGAIN)) return true;

  return ret == 0;
}

template <class IoContextT>
void IoContextDecoder<IoContextT>::Flush(std::ostream& os) {
  PartialDecodeImpl(packet_, os, /* flush */ true);
}

template <class IoContextT>
bool IoContextDecoder<IoContextT>::PartialDecode() {
  return PartialDecode(os_);
}

template <class IoContextT>
void IoContextDecoder<IoContextT>::Flush() {
  Flush(os_);
}

template <class IoContextT>
AVSampleFormat IoContextDecoder<IoContextT>::OutputSampleFmt() const {
  return codec_ctx_->sample_fmt;
}

template <class IoContextT>
void IoContextDecoder<IoContextT>::PartialDecodeImpl(Packet& packet,
                                                     std::ostream& os,
                                                     bool flush) {
  tiro_speech::av::PartialDecodeImpl(sample_converter_, codec_ctx_, frame_,
                                     packet, os, audio_stream_index_, flush);
}

template <class IoContextT>
std::chrono::milliseconds IoContextDecoder<IoContextT>::Duration() const {
  av_time_units dur{fmt_ctx_->duration};
  return std::chrono::duration_cast<std::chrono::milliseconds>(dur);
}

/** Decode frames of audio with a certain codec
 *
 * Example usage (decode mp3 on stdin and print out ot stdout):
 * \code
 *   Decoder decoder{Codec::kMp3, std::cin, std::cout};
 *   while (decoder.PartialDecode());
 *   decoder.Flush();
 * \endcode
 *
 * \TODO(rkjaran): decode straight to kaldi::Matrix
 */
class Decoder final : no_copy_or_move {
 public:
  Decoder(Codec codec, std::istream& is, std::ostream& os)
      : Decoder{codec, is, os, kDefaultSampleRate} {}

  Decoder(Codec codec, std::istream& is, std::ostream& os,
          int target_sample_rate_hertz)
      : packet_{},
        frame_{},
        inbuf_{is},
        parser_{codec},
        os_{os},
        sample_converter_{target_sample_rate_hertz},
        avio_ctx_{inbuf_} {
    auto* fmt_ctx = fmt_ctx_.get();
    fmt_ctx->pb = avio_ctx_.get();
    CallRet<AVFormatContext**, const char*, AVInputFormat*, AVDictionary**>(
        &avformat_open_input, &fmt_ctx, nullptr, nullptr, nullptr);
    CallRet(&avformat_find_stream_info, fmt_ctx,
            static_cast<AVDictionary**>(nullptr));
    audio_stream_index_ =
        CallRet(&av_find_best_stream, fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1,
                static_cast<AVCodec**>(nullptr), 0);
#if !defined(NDEBUG)
    av_dump_format(fmt_ctx, 0, "Input Buffer", 0);
#endif
  }

  /** Decode bytes and possibly write them to \a os
   *
   * \param[out] os   Decoded stream of bytes.
   *
   * \return false if input stream has been depleted, true otherwise.
   */
  bool PartialDecode(std::ostream& os);

  /** Decode bytes and possibly write them to stream given to constructor
   *
   * \param[out] os   Decoded stream of bytes.
   *
   * \return false if input stream has been depleted, true otherwise.
   */
  bool PartialDecode();

  /** Flush decoder
   *
   * A call to Flush() will signal the decoder to finish decoding. Usually
   * called after PartialDecode() returns false.
   *
   * \param[out] os  Decoded stream of bytes
   */
  void Flush(std::ostream& os);

  /** Flush decoder
   *
   * A call to Flush() will signal the decoder to finish decoding and writes
   * remaining bytes to stream. Usually called after PartialDecode() returns
   * false.
   */
  void Flush();

  AVSampleFormat OutputSampleFmt() const;

 private:
  Packet packet_;
  Frame frame_;
  Frame out_frame_;
  InputBuffer<> inbuf_;
  Parser parser_;
  std::ostream& os_;
  SampleConverter sample_converter_;
  int total_packet_size_ = 0;
  int audio_stream_index_ = 0;

  std::unique_ptr<AVFormatContext, void (*)(AVFormatContext*)> fmt_ctx_{
      Call(&avformat_alloc_context),
      [](AVFormatContext* /* ptr */) { /* avformat_close_input(&ptr); */ }};

  IoContext<decltype(inbuf_)> avio_ctx_;

  /** Decode bytes and write them to \a os
   *
   * This method perfoms decoding without parsing a new Packet from inbuf_. Used
   * in PartialDecode() (which does parsing) and Flush().
   *
   * \param[out] os   Decoded stream of bytes.
   */
  void PartialDecodeImpl(Packet& packet, std::ostream& os, bool flush = false);
};

}  // namespace tiro_speech::av

#endif  // TIRO_SPEECH_SRC_AUDIO_FFMPEG_WRAPPER_H_
