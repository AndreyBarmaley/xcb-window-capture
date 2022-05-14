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

    H264Encoder::H264Encoder(const H264Preset::type & h264Preset, int bitrate, bool debug)
        : stream(nullptr), oformat(nullptr), codec(nullptr), fps(25), frameCounter(0), frameWidth(0), frameHeight(0), captureStarted(false)
    {
        av_log_set_level(debug ? AV_LOG_DEBUG : AV_LOG_ERROR);

#if LIBAVFORMAT_VERSION_MAJOR < 58
        av_register_all();
        avcodec_register_all();
#endif
        oformat = av_guess_format("mp4", nullptr, nullptr);
        if(! oformat)
            throw std::runtime_error("av_guess_format failed");

        AVFormatContext* avfctx2 = nullptr;
        if(0 > avformat_alloc_output_context2(& avfctx2, oformat, nullptr, nullptr))
            throw std::runtime_error("avformat_alloc_output_context2 failed");

        avfctx.reset(avfctx2);

        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if(! codec)
            throw std::runtime_error("avcodec_find_encoder failed");

        stream = avformat_new_stream(avfctx.get(), codec);
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
    
        if(0 > avcodec_parameters_to_context(avcctx.get(), codecpar))
            throw std::runtime_error("avcodec_parameters_to_context failed");

        avcctx->time_base = (AVRational){1, fps};
        avcctx->framerate = (AVRational){fps, 1};
        avcctx->gop_size = 12;

        const char* preset = H264Preset::name(h264Preset);
        if(preset)
            av_opt_set(avcctx.get(), "preset", preset, 0);
    }

    H264Encoder::~H264Encoder()
    {
        if(captureStarted)
            stopRecord();

        ovframe.reset();
        swsctx.reset();
        avcctx.reset();
        avfctx.reset();
    }

    int H264Encoder::getFrameWidth(void) const
    {
        return frameWidth;
    }

    int H264Encoder::getFrameHeight(void) const
    {
        return frameHeight;
    }

    void H264Encoder::startRecord(const char* filename, int width, int height)
    {
        frameWidth = width;
        frameHeight = height;

        // align width, height
        if(height % 2) height -= 1;
        if(width % 8) width -= (width % 8);

        avcctx->pix_fmt = AV_PIX_FMT_YUV420P;
        avcctx->width = width;
        avcctx->height = height;

        if(0 > avcodec_parameters_from_context(stream->codecpar, avcctx.get()))
            throw std::runtime_error("avcodec_parameters_from_context failed");

        if(0 > avcodec_open2(avcctx.get(), codec, nullptr))
            throw std::runtime_error("avcodec_open2 failed");

        if(0 > avio_open(& avfctx->pb, filename, AVIO_FLAG_WRITE))
            throw std::runtime_error("avio_open failed");

        if(0 > avformat_write_header(avfctx.get(), nullptr))
            throw std::runtime_error("avformat_write_header failed");

        ovframe.reset(av_frame_alloc());
        ovframe->format = AV_PIX_FMT_YUV420P;

        ovframe->width = avcctx->width;
        ovframe->height = avcctx->height;

        if(0 > av_frame_get_buffer(ovframe.get(), 32))
            throw std::runtime_error("av_frame_get_buffer failed");

#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        AVPixelFormat avPixelFormat = AV_PIX_FMT_BGR0;
#else
        AVPixelFormat avPixelFormat = AV_PIX_FMT_0RGB;
#endif
        swsctx.reset(sws_getContext(avcctx->width, avcctx->height, avPixelFormat,
                        ovframe->width, ovframe->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr));

        if(! swsctx)
            throw std::runtime_error("sws_getContext failed");

        frameCounter = 0;
        captureStarted = true;
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

    void H264Encoder::sendFrame(AVFrame* frame)
    {
        if(0 > avcodec_send_frame(avcctx.get(), frame))
            throw std::runtime_error("avcodec_send_frame failed");

        while(true)
        {
            std::unique_ptr<AVPacket, AVPacketDeleter> pkt(av_packet_alloc());

            int ret = avcodec_receive_packet(avcctx.get(), pkt.get());
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;

            if(0 > ret)
                throw std::runtime_error("avcodec_receive_packet failed");

            av_packet_rescale_ts(pkt.get(), avcctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;

            // log packet 
            if(0)
            {
                AVRational* time_base = & avfctx->streams[pkt->stream_index]->time_base;
 
                std::cout << "pts:" << av_ts2string(pkt->pts) << " pts_time:" << av_ts2timestring(pkt->pts, time_base) <<
                    " dts:" << av_ts2string(pkt->dts) << " dts_time:" << av_ts2timestring(pkt->dts, time_base) <<
                    " duration:" << av_ts2string(pkt->duration) << " duration_time:" << av_ts2timestring(pkt->duration, time_base) <<
                    " stream_index:" << pkt->stream_index << std::endl;
            }

            if(0 > av_interleaved_write_frame(avfctx.get(), pkt.get()))
                throw std::runtime_error("av_interleaved_write_frame failed");
        }
    }

    void H264Encoder::stopRecord(void)
    {
        sendFrame(nullptr);
        captureStarted = false;
    
        av_write_trailer(avfctx.get());
        avio_close(avfctx->pb);
    }

    void H264Encoder::pushFrame(const uint8_t* pixels, int pitch, int height)
    {
        const uint8_t* data[1] = { pixels };
        int lines[1] = { pitch };

        // align
        if(height % 2) height -= 1;
        
        sws_scale(swsctx.get(), data, lines, 0, height, ovframe->data, ovframe->linesize);
        ovframe->pts = frameCounter++;
        sendFrame(ovframe.get());
    }
}
