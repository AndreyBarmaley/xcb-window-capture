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

#ifndef PULSE_AUDIO_H
#define PULSE_AUDIO_H

#include <list>
#include <mutex>
#include <vector>
#include <thread>
#include <memory>

#include "pulse/context.h"
#include "pulse/mainloop.h"
#include "pulse/introspect.h"
#include "pulse/stream.h"

namespace PulseAudio
{
    struct MainLoopDeleter
    {
        void operator()(pa_mainloop* loop)
        {
            pa_mainloop_free(loop);
        }
    };

    struct ContextDeleter
    {
        void operator()(pa_context* ctx)
        {
            if(pa_context_get_state(ctx) != PA_CONTEXT_UNCONNECTED)
                pa_context_disconnect(ctx);

            pa_context_unref(ctx);
        }
    };

    struct StreamDeleter
    {
        void operator()(pa_stream* st)
        {
            if(pa_stream_get_state(st) != PA_STREAM_UNCONNECTED)
                pa_stream_disconnect(st);

            pa_stream_unref(st);
        }
    };

    typedef std::vector<uint8_t> BufSamples;

    class Context
    {
    protected:
        pa_sample_spec spec;

        std::unique_ptr<pa_mainloop, MainLoopDeleter> loop;
        std::unique_ptr<pa_context, ContextDeleter> ctx;
        std::unique_ptr<pa_stream, StreamDeleter> stream;

        std::thread thread;

        std::mutex dataLock;
        BufSamples dataBuf;
        std::string monitorName;

        static void connectNotifyCallback(pa_context*, void*);
        static void serverInfoCallback(pa_context*, const pa_server_info*, void*);
        static void streamNotifyCallback(pa_stream* stream, void*);
        static void streamReadCallback(pa_stream*, const size_t, void*);

        void streamCreate(const pa_server_info* info);

    public:
        Context(const char* appname);
        ~Context();

        BufSamples popDataBuf(void);

        int format(void) const { return spec.format; }
        int rate(void) const { return spec.rate; }
        int channels(void) const { return spec.channels; }
    };
}

#endif // PULSE_AUDIO
