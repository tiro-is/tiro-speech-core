#include "src/audio/ffmpeg-wrapper.h"

#include <string>
#include <system_error>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

namespace tiro_speech::av {

AvLibraryError::AvLibraryError(ErrnumT errnum)
    : AvError{StrError(errnum)}, errnum_{errnum} {}

AvLibraryError::ErrnumT AvLibraryError::Errnum() const { return errnum_; }

std::string AvLibraryError::StrError(AvLibraryError::ErrnumT errnum) const {
  std::string str_error;
  str_error.resize(AV_ERROR_MAX_STRING_SIZE);
  int ret = av_strerror(errnum, &str_error[0], AV_ERROR_MAX_STRING_SIZE);
  if (ret < 0) {  // AVERROR(errnum) not found
    throw std::runtime_error{
        "Exception AvError constructed with unknown AVERROR "};
  }
  return std::string{"AvLibraryError: code = "} + std::to_string(errnum) +
         " Msg = " + str_error;
}

SampleConverter::SampleConverter() : SampleConverter{kDefaultSampleRate} {}

SampleConverter::SampleConverter(int sample_rate_hertz) {
  out_frame_->channel_layout = AV_CH_LAYOUT_MONO;
  out_frame_->format = AV_SAMPLE_FMT_S16;
  out_frame_->sample_rate = sample_rate_hertz;
  out_frame_->nb_samples = 9254797;
}

void SampleConverter::Init(const Frame& in_frame) {
  swr_ctx_ = Call(&swr_alloc_set_opts, "Failed to alloc SwrContext",
                  static_cast<SwrContext*>(nullptr),
                  /* out */
                  static_cast<std::int64_t>(out_frame_->channel_layout),
                  static_cast<AVSampleFormat>(out_frame_->format),
                  out_frame_->sample_rate,
                  /* in */
                  static_cast<std::int64_t>(in_frame->channel_layout),
                  static_cast<AVSampleFormat>(in_frame->format),
                  in_frame->sample_rate, 0, static_cast<void*>(nullptr));
  CallRet(&swr_init, swr_ctx_);
}

bool SampleConverter::IsInitialized() const {
  return swr_ctx_ != nullptr && CallRet(&swr_is_initialized, swr_ctx_) > 0;
}

Frame& SampleConverter::Convert(const Frame& frame) {
  CallRet(&swr_convert_frame, swr_ctx_, out_frame_.get(),
          const_cast<const AVFrame*>(frame.get()));
  return out_frame_;
}

Frame& SampleConverter::Flush() {
  CallRet(&swr_convert_frame, swr_ctx_, out_frame_.get(),
          static_cast<const AVFrame*>(nullptr));
  return out_frame_;
}

bool Decoder::PartialDecode(std::ostream& os) {
  int ret = CallRetEof(&av_read_frame, fmt_ctx_.get(), packet_.get());
  if (packet_->size > 0) {
    total_packet_size_ += packet_->size;
    PartialDecodeImpl(packet_, os);
  }
  return ret != AVERROR_EOF;
}

bool Decoder::PartialDecode() { return PartialDecode(os_); }

void Decoder::Flush(std::ostream& os) {
  PartialDecodeImpl(packet_, os, /* flush */ true);
}

void Decoder::Flush() { Flush(os_); }

AVSampleFormat Decoder::OutputSampleFmt() const {
  return parser_.codec_ctx_->sample_fmt;
}

void Decoder::PartialDecodeImpl(Packet& packet, std::ostream& os, bool flush) {
  tiro_speech::av::PartialDecodeImpl(sample_converter_, parser_.codec_ctx_,
                                     frame_, packet, os, audio_stream_index_,
                                     flush);
}

namespace {
/// Write out_frame to stream in standard format
void WriteFrame(Frame& out_frame, std::ostream& os) {
  int sample_size = CallRet(&av_get_bytes_per_sample,
                            static_cast<AVSampleFormat>(out_frame->format));

  for (int i = 0; i < out_frame->nb_samples; ++i) {
    for (int ch = 0; ch < out_frame->channels; ++ch) {
      auto data =
          reinterpret_cast<char*>(out_frame->data[ch] + sample_size * i);
      os.write(data, sample_size);
    }
  }
}
}  // namespace

void PartialDecodeImpl(SampleConverter& sample_converter,
                       CodecContext& codec_ctx, Frame& frame, Packet& packet,
                       std::ostream& os, int audio_stream_index, bool flush) {
  if (!flush && packet->stream_index != audio_stream_index) return;

  int pkt_ret =
      CallRet(&avcodec_send_packet, codec_ctx.get(),
              const_cast<const AVPacket*>(flush ? nullptr : packet.get()));
  if (pkt_ret == AVERROR(EAGAIN) || pkt_ret == AVERROR_EOF) return;

  for (;;) {
    constexpr std::array<int, 2> normal_flow_errors{AVERROR_EOF,
                                                    AVERROR(EAGAIN)};
    int ret = CallRet2(cbegin(normal_flow_errors), cend(normal_flow_errors),
                       &avcodec_receive_frame, codec_ctx.get(), frame.get());
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
      break;
    }
    if (!sample_converter.IsInitialized()) sample_converter.Init(frame);
    Frame& out_frame = sample_converter.Convert(frame);
    WriteFrame(out_frame, os);
  }

  if (flush) {
    // When the decoder has been flushed we need to flush the sample converter
    // (which is usually empty unless we're downsampling).
    while (true) {
      Frame& out_frame = sample_converter.Flush();
      if (out_frame->nb_samples == 0) {
        break;
      }
      WriteFrame(out_frame, os);
    }
  }
}

}  // namespace tiro_speech::av
