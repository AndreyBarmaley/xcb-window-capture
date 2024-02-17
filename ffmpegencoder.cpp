/***************************************************************************
 *   Copyright © 2022 by Andrey Afletdinov <public.irkutsk@gmail.com>      *
 *                                                                         *
 *   Part of the XcbWindowCapture                                          *
 *   https://github.com/AndreyBarmaley/xcb-window-capture                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <QDebug>

#include <iostream>
#include <exception>
#include <algorithm>

#include "ffmpegencoder.h"

namespace FFMPEG
{
    const char* H264Preset::name(const H264Preset::type & h264Preset)
    {
        switch(h264Preset)
        {
            case H264Preset::UltraFast:  return "ultrafast";
            case H264Preset::SuperFast:  return "superfast";
            case H264Preset::VeryFast:   return "veryfast";
            case H264Preset::Faster:     return "faster";
            case H264Preset::Fast:       return "fast";
            case H264Preset::Medium:     return "medium";
            case H264Preset::Slow:       return "slow";
            case H264Preset::Slower:     return "slower";
            case H264Preset::VerySlow:   return "veryslow";
            default: break;
        }

        return nullptr;
    }

    AVSampleFormat avFormatFromPulse(int pulse)
    {
        switch(pulse)
        {
            case PA_SAMPLE_FLOAT32LE: return AV_SAMPLE_FMT_FLTP;
            case PA_SAMPLE_S16LE: return AV_SAMPLE_FMT_S16;
            default: break;
        }

        return AV_SAMPLE_FMT_NONE;
    }

    QString errorString(int errnum)
    {
        char errbuf[1024]{0};
        return 0 > av_strerror(errnum, errbuf, sizeof(errbuf) - 1) ? "error not found" : errbuf;
    }

    std::string av_ts2string(int64_t ts)
    {
        char str[AV_TS_MAX_STRING_SIZE]{0};
        return av_ts_make_string(str, ts);
    }

    std::string av_ts2timestring(int64_t ts, AVRational* tb)
    {
        char str[AV_TS_MAX_STRING_SIZE]{0};
        return av_ts_make_time_string(str, ts, tb);
    }

    void AudioFrame::init(const AVCodecContext* avcctx)
    {
        auto frame = av_frame_alloc();
        if(! frame)
            throw std::runtime_error("av_frame_alloc failed");

        frame->format = avcctx->sample_fmt;
        frame->channel_layout = avcctx->channel_layout;
        frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);
        frame->sample_rate = avcctx->sample_rate;
        frame->nb_samples = avcctx->frame_size;

        reset(frame);

        int ret = av_frame_get_buffer(frame, 0);
        if(0 > ret)
            throw FFMPEG::runtimeException("av_frame_get_buffer", ret);
    }

    void AudioFrame::init(const AVSampleFormat & format, int layout, int rate, int samples)
    {
        auto frame = av_frame_alloc();
        if(! frame)
            throw std::runtime_error("av_frame_alloc failed");

        frame->format = format;
        frame->channel_layout = layout;
        frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);
        frame->sample_rate = rate;
        frame->nb_samples = samples;

        reset(frame);

        if(samples)
        {
            int ret = av_frame_get_buffer(frame, 0);
            if(0 > ret)
                throw FFMPEG::runtimeException("av_frame_get_buffer", ret);
        }
    }

    int AudioFrame::fill(const uint8_t* buf, size_t len, bool align)
    {
        AVFrame* frame = get();

        if(0 == frame->nb_samples)
            frame->nb_samples = len / (av_get_bytes_per_sample((AVSampleFormat) frame->format) * frame->channels);

        return avcodec_fill_audio_frame(frame, frame->channels, (AVSampleFormat) frame->format, buf, len, align);
    }

    void VideoFrame::init(const AVPixelFormat & format, int width, int height)
    {
        auto frame = av_frame_alloc();
        if(! frame)
            throw std::runtime_error("av_frame_alloc failed");

        frame->width = width;
        frame->height = height;
        frame->format = format;

        reset(frame);

        int ret = av_frame_get_buffer(frame, 32);
        if(0 > ret)
            throw FFMPEG::runtimeException("av_frame_get_buffer", ret);
    }

    void EncoderBase::writeFrame(const AVFrame* framePtr)
    {
        int ret = avcodec_send_frame(avcctx.get(), framePtr);
        if(0 > ret)
            throw FFMPEG::runtimeException("avcodec_send_frame", ret);

        while(true)
        {
            std::unique_ptr<AVPacket, AVPacketDeleter> pkt(av_packet_alloc());

            int ret = avcodec_receive_packet(avcctx.get(), pkt.get());
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;

            if(0 > ret)
                throw FFMPEG::runtimeException("avcodec_receive_packet", ret);

            av_packet_rescale_ts(pkt.get(), avcctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;

            // log packet 
#ifdef BUILD_DEBUG
            if(0)
            {
                AVRational* time_base = & avfctx->streams[pkt->stream_index]->time_base;
 
                std::cout << __FUNCTION__ << "pts:" << av_ts2string(pkt->pts) << " pts_time:" << av_ts2timestring(pkt->pts, time_base) <<
                    " dts:" << av_ts2string(pkt->dts) << " dts_time:" << av_ts2timestring(pkt->dts, time_base) <<
                    " duration:" << av_ts2string(pkt->duration) << " duration_time:" << av_ts2timestring(pkt->duration, time_base) <<
                    " stream_index:" << pkt->stream_index << std::endl;
            }
#endif

            ret = av_write_frame(avfctx, pkt.get());
            if(0 > ret)
                throw FFMPEG::runtimeException("av_interleaved_write_frame", ret);
        }
    }

    /* VideoEncoder */
    void VideoEncoder::init(AVFormatContext* ptr, const H264Preset::type & h264Preset, int bitrate)
    {
        avfctx = ptr;

        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if(! codec)
            throw std::runtime_error("avcodec_find_encoder failed");

        stream = avformat_new_stream(avfctx, codec);
        if(! stream)
            throw std::runtime_error("avformat_new_stream failed");

        avcctx.reset(avcodec_alloc_context3(codec));
        if(! avcctx)
            throw std::runtime_error("avcodec_alloc_context3 failed");

        stream->id = avfctx->nb_streams - 1;
        stream->time_base = (AVRational){1, fps};
        stream->avg_frame_rate = (AVRational){fps, 1};

        auto codecpar = stream->codecpar;
        codecpar->codec_id = AV_CODEC_ID_H264;
        codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        codecpar->format = AV_PIX_FMT_YUV420P;
        codecpar->bit_rate = bitrate * 1024;

        int ret = avcodec_parameters_to_context(avcctx.get(), codecpar);
        if(0 > ret)
            throw FFMPEG::runtimeException("avcodec_parameters_to_context", ret);

        avcctx->time_base = (AVRational){1, fps};
        avcctx->framerate = (AVRational){fps, 1};
        avcctx->gop_size = 12;

        const char* preset = H264Preset::name(h264Preset);
        if(preset)
            av_opt_set(avcctx.get(), "preset", preset, 0);
    }

    void VideoEncoder::start(int width, int height)
    {
        // align width, height
        if(height % 2) height -= 1;
        if(width % 8) width -= (width % 8);

        avcctx->pix_fmt = AV_PIX_FMT_YUV420P;
        avcctx->width = width;
        avcctx->height = height;

        int ret = avcodec_parameters_from_context(stream->codecpar, avcctx.get());
        if(0 > ret)
            throw FFMPEG::runtimeException("avcodec_parameters_from_context", ret);

        ret = avcodec_open2(avcctx.get(), codec, nullptr);
        if(0 > ret)
            throw FFMPEG::runtimeException("avcodec_open2", ret);

        frame.init(AV_PIX_FMT_YUV420P, avcctx->width, avcctx->height);

#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        AVPixelFormat avPixelFormat = AV_PIX_FMT_BGR0;
#else
        AVPixelFormat avPixelFormat = AV_PIX_FMT_0RGB;
#endif
        swsctx.reset(sws_getContext(avcctx->width, avcctx->height, avPixelFormat,
                        frame->width, frame->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr));

        if(! swsctx)
            throw std::runtime_error("sws_getContext failed");

        pts = 0;
    }

    void VideoEncoder::encodeFrame(const uint8_t* pixels, int pitch, int height)
    {
        const uint8_t* data[1] = { pixels };
        int lines[1] = { pitch };

        // align
        if(height % 2) height -= 1;
        
        sws_scale(swsctx.get(), data, lines, 0, height, frame->data, frame->linesize);
        frame->pts = pts++;

        writeFrame(frame.get());
    }

    /* AudioEncoder */
    void AudioEncoder::init(AVFormatContext* ptr, const AudioPlugin & plugin, int bitrate)
    {
        avfctx = ptr;

        codec = avcodec_find_encoder(avfctx->oformat->audio_codec);
        if(! codec)
            throw std::runtime_error("avcodec_find_encoder failed");

        stream = avformat_new_stream(avfctx, codec);
        if(! stream)
            throw std::runtime_error("avformat_new_stream failed");

        avcctx.reset(avcodec_alloc_context3(codec));
        if(! avcctx)
            throw std::runtime_error("avcodec_alloc_context3 failed");

        stream->id = avfctx->nb_streams - 1;

        avcctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        avcctx->bit_rate = bitrate * 1024;
        avcctx->sample_rate = 44100;

        avcctx->channel_layout = AV_CH_LAYOUT_STEREO;
        avcctx->channels = av_get_channel_layout_nb_channels(avcctx->channel_layout);

        stream->time_base = (AVRational){ 1, avcctx->sample_rate };

        pulse.reset(new PulseAudio::Context("XcbWindowCapture"));

        auto format = avFormatFromPulse(pulse->format());
        if(format == AV_SAMPLE_FMT_NONE)
            throw std::runtime_error("unknown sample format");

        tail.reserve(1024 * 1024);
    }

    void AudioEncoder::start(void)
    {
        auto sampleFormat = avFormatFromPulse(pulse->format());
        int sampleChannels = pulse->channels();
        int sampleLayout = 1 < pulse->channels() ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
        int sampleRate = pulse->rate();

        int ret = avcodec_open2(avcctx.get(), codec, nullptr);
        if(0 > ret)
            throw FFMPEG::runtimeException("avcodec_open2", ret);

        ret = avcodec_parameters_from_context(stream->codecpar, avcctx.get());
        if(0 > ret)
            throw FFMPEG::runtimeException("avcodec_parameters_from_context", ret);

        frameDst.init(avcctx.get());
        frameSrc.init(sampleFormat, sampleLayout, sampleRate, frameDst->nb_samples);

        swrctx.reset(swr_alloc());
        if(! swrctx)
            throw std::runtime_error("swr_alloc failed");

        av_opt_set_int(swrctx.get(), "in_channel_count", sampleChannels, 0);
        av_opt_set_int(swrctx.get(), "in_sample_rate", sampleRate, 0);
        av_opt_set_sample_fmt(swrctx.get(), "in_sample_fmt", sampleFormat, 0);

        av_opt_set_int(swrctx.get(), "out_channel_count", avcctx->channels, 0);
        av_opt_set_int(swrctx.get(), "out_sample_rate", avcctx->sample_rate, 0);
        av_opt_set_sample_fmt(swrctx.get(), "out_sample_fmt", avcctx->sample_fmt, 0);

        ret = swr_init(swrctx.get());
        if(0 > ret)
            throw FFMPEG::runtimeException("swr_init", ret);
    }

    bool AudioEncoder::encodeFrame(void)
    {
        auto raw = pulse->popDataBuf();
        if(raw.empty())
            return false;

        auto sampleFormat = avFormatFromPulse(pulse->format());
        bool align = true;

        if(tail.empty())
            tail.swap(raw);
        else
            tail.insert(tail.end(), raw.begin(), raw.end());

        int ret = av_samples_get_buffer_size(nullptr, frameSrc->channels, frameSrc->nb_samples, sampleFormat, align);
        if(ret < 0)
            throw FFMPEG::runtimeException("av_samples_get_buffer_size", ret);

        const size_t blocksz = ret;
        if(tail.size() < blocksz)
            return false;

        for(size_t offset = 0; offset < tail.size(); offset += blocksz)
        {
            // small data
            if(tail.size() - offset < blocksz)
            {
                tail = std::vector<uint8_t>(tail.begin() + offset, tail.end());
                return false;
            }

            ret = frameSrc.fill(tail.data() + offset, blocksz, align);

            // small data
            if(ret == AVERROR(EINVAL))
                return false;

            if(ret < 0)
                throw FFMPEG::runtimeException("av_codec_fill_audio_frame", ret);

            // encode frameSrc to frameDst
            int delay = swr_get_delay(swrctx.get(), frameSrc->sample_rate);
            int dst_nb_samples = av_rescale_rnd(frameSrc->nb_samples, frameDst->sample_rate, frameSrc->sample_rate, AV_ROUND_UP);

            ret = av_frame_make_writable(frameDst.get());
            if(0 > ret)
                throw FFMPEG::runtimeException("av_frame_make_writable", ret);
    
            ret = swr_convert(swrctx.get(), frameDst->data, dst_nb_samples, (const uint8_t**) frameSrc->data, frameSrc->nb_samples);
            if(0 > ret)
                throw FFMPEG::runtimeException("swr_convert", ret);

            frameDst->pts = av_rescale_q(pts, (AVRational){1, avcctx->sample_rate}, avcctx->time_base);
            pts += dst_nb_samples;

            // write
            writeFrame(frameDst.get());
        }

        tail.clear();
        return true;
    }

    /* H264Encoder */
    H264Encoder::H264Encoder(const H264Preset::type & h264Preset, int videoBitrate, const AudioPlugin & audioPlugin, int audioBitrate)
        : oformat(nullptr), captureStarted(false)
    {
#ifdef BUILD_DEBUG
        av_log_set_level(AV_LOG_DEBUG);
#else
        av_log_set_level(AV_LOG_ERROR);
#endif

#if LIBAVFORMAT_VERSION_MAJOR < 58
        av_register_all();
        avcodec_register_all();
#endif
        oformat = av_guess_format("mp4", nullptr, nullptr);
        if(! oformat)
            throw std::runtime_error("av_guess_format failed");

        AVFormatContext* avfctx2 = nullptr;
        int ret = avformat_alloc_output_context2(& avfctx2, oformat, nullptr, nullptr);

        if(0 > ret)
            throw FFMPEG::runtimeException("avformat_alloc_output_context2", ret);

        avfctx.reset(avfctx2);

        video.init(avfctx2, h264Preset, videoBitrate);

        if(audioPlugin != AudioPlugin::None)
        {
            audio.reset(new AudioEncoder());
            audio->init(avfctx2, audioPlugin, audioBitrate);
        }
    }

    H264Encoder::~H264Encoder()
    {
        if(captureStarted)
            stopRecord();
    }

    void H264Encoder::startRecord(const char* filename, int width, int height)
    {
        video.start(width, height);

        if(audio)
            audio->start();

        int ret = avio_open(& avfctx->pb, filename, AVIO_FLAG_WRITE);
        if(0 > ret)
            throw FFMPEG::runtimeException("avio_open", ret);

        ret = avformat_write_header(avfctx.get(), nullptr);
        if(0 > ret)
            throw FFMPEG::runtimeException("avformat_write_header", ret);

        captureStarted = true;
    }

    void H264Encoder::stopRecord(void)
    {
        video.writeFrame(nullptr);
        if(audio) audio->writeFrame(nullptr);

        captureStarted = false;
    
        av_write_trailer(avfctx.get());
        avio_close(avfctx->pb);
    }

    void H264Encoder::encodeFrame(const uint8_t* pixels, int pitch, int height)
    {
        if(! audio || 0 >= av_compare_ts(video.pts, video.avcctx->time_base,
                                            audio->pts, audio->avcctx->time_base))
            video.encodeFrame(pixels, pitch, height);
        else
        {
            audio->encodeFrame();
            video.encodeFrame(pixels, pitch, height);
        }
    }
}
