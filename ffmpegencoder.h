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

#ifndef FFMPEG_ENCODER_H
#define FFMPEG_ENCODER_H

#include <memory>

#ifdef __cplusplus
extern "C" {
#endif

#include "libavdevice/avdevice.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/timestamp.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

#ifdef __cplusplus
}
#endif

#ifdef BOOST_STACKTRACE_USE
#include "boost/stacktrace.hpp"
#include <sstream>
#endif

#include "pulseaudio.h"

enum class AudioPlugin { None, PulseAudio };

namespace FFMPEG
{
    AVSampleFormat avFormatFromPulse(int pulse);

    struct AVCodecContextDeleter
    {
        void operator()(AVCodecContext* ctx)
        {
            avcodec_free_context(& ctx);
        }
    };

    struct AVFormatContextDeleter
    {
        void operator()(AVFormatContext* ctx)
        {
            avformat_free_context(ctx);
        }
    };

    struct SwsContextDeleter
    {
        void operator()(SwsContext* ctx)
        {
            sws_freeContext(ctx);
        }
    };

    struct SwrContextDeleter
    {
        void operator()(SwrContext* ctx)
        {
            swr_free(& ctx);
        }
    };

    struct AVFrameDeleter
    {
        void operator()(AVFrame* ptr)
        {
            av_frame_free(& ptr);
        }
    };

    struct AVPacketDeleter
    {
        void operator()(AVPacket* ptr)
        {
            av_packet_free(& ptr);
        }
    };

    namespace H264Preset
    {
        enum type { UltraFast = 1, SuperFast = 2, VeryFast = 3, Faster = 4, Fast = 5, Medium = 6, Slow = 7, Slower = 8, VerySlow = 9 }; 
        const char* name(const type &);
    };

    struct runtimeException
    {
        const char* func;
        int code;
#ifdef BOOST_STACKTRACE_USE
        std::string trace;
#endif
        
        runtimeException(const char* f, int e) : func(f), code(e)
        {
#ifdef BOOST_STACKTRACE_USE
            std::ostringstream os;
            os << boost::stacktrace::stacktrace();
            trace = os.str();
#endif
        }
    };

    QString errorString(int);

    struct AVFrameBase : std::unique_ptr<AVFrame, AVFrameDeleter>
    {
        AVFrameBase() {}
        virtual ~AVFrameBase() {}
    };

    struct AudioFrame : AVFrameBase
    {
        AudioFrame() {}

        void init(const AVCodecContext*);
        void init(const AVSampleFormat &, int layout, int rate, int samples);

        int  fill(const uint8_t* buf, size_t len, bool);
    };

    struct VideoFrame : AVFrameBase
    {
        VideoFrame() {}

        void init(const AVPixelFormat &, int width, int height);
    };

    struct EncoderBase
    {
        virtual ~EncoderBase() {}

        AVStream* stream = nullptr;
        AVFormatContext* avfctx = nullptr;

        std::unique_ptr<AVCodecContext, AVCodecContextDeleter> avcctx;

        void writeFrame(const AVFrame*);
    };

    struct VideoEncoder : EncoderBase
    {
#if LIBAVFORMAT_VERSION_MAJOR < 59
        AVCodec* codec = nullptr;
#else
        const AVCodec* codec = nullptr;
#endif
        std::unique_ptr<SwsContext, SwsContextDeleter> swsctx;

        VideoFrame frame;

        int fps = 25;
        int pts = 0;

        void init(AVFormatContext*, const H264Preset::type & h264Preset, int bitrate);
        void start(int width, int height);

        void encodeFrame(const uint8_t* pixels, int pitch, int height);
    };

    struct AudioEncoder : EncoderBase
    {
#if LIBAVFORMAT_VERSION_MAJOR < 59
        AVCodec* codec = nullptr;
#else
        const AVCodec* codec = nullptr;
#endif
        std::unique_ptr<SwrContext, SwrContextDeleter> swrctx{nullptr, SwrContextDeleter()};
        std::unique_ptr<PulseAudio::Context> pulse;
        std::vector<uint8_t> tail;

        AudioFrame frameSrc, frameDst;

        int pts = 0;

        void init(AVFormatContext*, const AudioPlugin &, int bitrate);
        void start(void);

        bool encodeFrame(void);
    };

    class H264Encoder
    {
#if LIBAVFORMAT_VERSION_MAJOR < 59
        AVOutputFormat* oformat;
#else
        const AVOutputFormat* oformat;
#endif
        std::unique_ptr<AVFormatContext, AVFormatContextDeleter> avfctx;

protected:
        VideoEncoder video;
        std::unique_ptr<AudioEncoder> audio;

        bool captureStarted;

    public:
        H264Encoder(const H264Preset::type &, int vbitrate, const AudioPlugin &, int abitrate);
        ~H264Encoder();

        void startRecord(const char* filename, int width, int height);
        void stopRecord(void);

        void encodeFrame(const uint8_t* pixels, int pitch, int height);
    };
}

#endif // FFMPEG_ENCODER_H
