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

#ifdef __cplusplus
}
#endif

namespace FFMPEG
{
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


    class H264Encoder
    {
        AVStream* stream;
#if LIBAVFORMAT_VERSION_MAJOR < 59
        AVOutputFormat* oformat;
        AVCodec* codec;
#else
        const AVOutputFormat* oformat;
        const AVCodec* codec;
#endif
        std::unique_ptr<AVCodecContext, AVCodecContextDeleter> avcctx;
        std::unique_ptr<AVFormatContext, AVFormatContextDeleter> avfctx;
        std::unique_ptr<SwsContext, SwsContextDeleter> swsctx;
        std::unique_ptr<AVFrame, AVFrameDeleter> ovframe;

protected:
        int fps;
        int frameCounter;
        int frameWidth;
        int frameHeight;
        bool captureStarted;

        void sendFrame(AVFrame* frame);

    public:
        H264Encoder(const H264Preset::type &, int bitrate, bool debug);
        ~H264Encoder();

        int getFrameWidth(void) const;
        int getFrameHeight(void) const;

        void startRecord(const char* filename, int width, int height);
        void stopRecord(void);
        void pushFrame(const uint8_t* pixels, int pitch, int height);
    };
}

#endif // FFMPEG_ENCODER_H
