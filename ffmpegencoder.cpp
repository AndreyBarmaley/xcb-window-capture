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
        codecpar->bit_rate = bitrate * 1000;

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
        frameWidth = width;
        frameHeight = height;

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

        frame.reset(av_frame_alloc());
        if(! frame)
            throw std::runtime_error("av_frame_alloc failed");

        frame->format = AV_PIX_FMT_YUV420P;

        frame->width = avcctx->width;
        frame->height = avcctx->height;

        ret = av_frame_get_buffer(frame.get(), 32);
        if(0 > ret)
            throw FFMPEG::runtimeException("av_frame_get_buffer", ret);

#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        AVPixelFormat avPixelFormat = AV_PIX_FMT_BGR0;
#else
        AVPixelFormat avPixelFormat = AV_PIX_FMT_0RGB;
#endif
        swsctx.reset(sws_getContext(avcctx->width, avcctx->height, avPixelFormat,
                        frame->width, frame->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr));

        if(! swsctx)
            throw std::runtime_error("sws_getContext failed");

        frameCounter = 0;
    }

    void VideoEncoder::sendFrame(bool withFrame)
    {
        AVFrame* framePtr = nullptr;
        if(withFrame)
            framePtr = frame.get();

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
            AVRational* time_base = & avfctx->streams[pkt->stream_index]->time_base;
 
            std::cout << "pts:" << av_ts2string(pkt->pts) << " pts_time:" << av_ts2timestring(pkt->pts, time_base) <<
                    " dts:" << av_ts2string(pkt->dts) << " dts_time:" << av_ts2timestring(pkt->dts, time_base) <<
                    " duration:" << av_ts2string(pkt->duration) << " duration_time:" << av_ts2timestring(pkt->duration, time_base) <<
                    " stream_index:" << pkt->stream_index << std::endl;
#endif

            ret = av_interleaved_write_frame(avfctx, pkt.get());
            if(0 > ret)
                throw FFMPEG::runtimeException("av_interleaved_write_frame", ret);
        }
    }

    void VideoEncoder::pushFrame(const uint8_t* pixels, int pitch, int height)
    {
        const uint8_t* data[1] = { pixels };
        int lines[1] = { pitch };

        // align
        if(height % 2) height -= 1;
        
        sws_scale(swsctx.get(), data, lines, 0, height, frame->data, frame->linesize);
        frame->pts = frameCounter++;

        sendFrame(true);
    }

    /* AudioEncoder */
    void AudioEncoder::init(AVFormatContext* ptr, int bitrate)
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

        avcctx->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] :  AV_SAMPLE_FMT_FLTP;
        avcctx->bit_rate = bitrate;
        avcctx->sample_rate = 44100;

        avcctx->channels = av_get_channel_layout_nb_channels(avcctx->channel_layout);
        avcctx->channel_layout = AV_CH_LAYOUT_STEREO;

        stream->time_base = (AVRational){ 1, avcctx->sample_rate };
    }

    void AudioEncoder::start(void)
    {
        int ret = avcodec_open2(avcctx.get(), codec, nullptr);
        if(0 > ret)
            throw FFMPEG::runtimeException("avcodec_open2", ret);

        auto nb_samples = avcctx->frame_size;

        frame.reset(av_frame_alloc());

        if(! frame)
            throw std::runtime_error("av_frame_alloc failed");

        frame->format = avcctx->sample_fmt;
        frame->channel_layout = avcctx->channel_layout;
        frame->sample_rate = avcctx->sample_rate;
        frame->nb_samples = nb_samples;

        ret = av_frame_get_buffer(frame.get(), 0);
        if(0 > ret)
            throw FFMPEG::runtimeException("av_frame_get_buffer", ret);

        ret = avcodec_parameters_from_context(stream->codecpar, avcctx.get());
        if(0 > ret)
            throw FFMPEG::runtimeException("avcodec_parameters_from_context", ret);

        swrctx.reset(swr_alloc());
        if(! swrctx)
            throw std::runtime_error("swr_alloc failed");

        av_opt_set_int(swrctx.get(), "in_channel_count", avcctx->channels, 0);
        av_opt_set_int(swrctx.get(), "in_sample_rate", avcctx->sample_rate, 0);
        av_opt_set_sample_fmt(swrctx.get(), "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
        av_opt_set_int(swrctx.get(), "out_channel_count", avcctx->channels, 0);
        av_opt_set_int(swrctx.get(), "out_sample_rate", avcctx->sample_rate, 0);
        av_opt_set_sample_fmt(swrctx.get(), "out_sample_fmt", avcctx->sample_fmt, 0);

        ret = swr_init(swrctx.get());
        if(0 > ret)
            throw FFMPEG::runtimeException("swr_init", ret);

        pulse.reset(new PulseAudio::Context("XcbWindowCapture"));
    }

    /* H264Encoder */
    H264Encoder::H264Encoder(const H264Preset::type & h264Preset, int videoBitrate)
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
        //audio.init(avfctx2, 64000);
    }

    H264Encoder::~H264Encoder()
    {
        if(captureStarted)
            stopRecord();
    }

    void H264Encoder::startRecord(const char* filename, int width, int height)
    {
        video.start(width, height);
        //audio.start();

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
        video.sendFrame(false);

        captureStarted = false;
    
        av_write_trailer(avfctx.get());
        avio_close(avfctx->pb);
    }
}
