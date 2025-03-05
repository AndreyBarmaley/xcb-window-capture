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

#include <numeric>
#include <exception>
#include <algorithm>
#include <iostream>

#include <QDebug>

#include "pulseaudio.h"

namespace PulseAudio
{
    Context::Context(const char* appname, bool defSink) : defaultSink(defSink)
    {
        spec.format = PA_SAMPLE_S16LE;
        spec.rate = 44100;
        spec.channels = 2;

        loop.reset(pa_mainloop_new());
        if(! loop)
            throw std::runtime_error("pa_mainloop_new failed");

        pa_mainloop_api* api = pa_mainloop_get_api(loop.get());
        if(! api)
            throw std::runtime_error("pa_mainloop_get_api failed");

        ctx.reset(pa_context_new(api, appname));
        if(! ctx)
            throw std::runtime_error("pa_context_new failed");

        pa_context_set_state_callback(ctx.get(), & connectNotifyCallback, this);

        if(0 > pa_context_connect(ctx.get(), nullptr, PA_CONTEXT_NOFLAGS, nullptr))
            throw std::runtime_error("pa_context_connect failed");

        //qWarning() << "pulseaudio server: " << pa_context_get_server( ctx.get() );

        thread = std::thread([this]
        {
            pa_mainloop_run(loop.get(), nullptr);
        });
    }

    Context::~Context()
    {
        pa_mainloop_quit(loop.get(), 0);

        if(thread.joinable())
            thread.join();
    }

    void Context::streamCreate(const pa_server_info* info)
    {
        stream.reset( pa_stream_new(ctx.get(), "capture monitor", & spec, nullptr) );

        pa_stream_set_state_callback(stream.get(), & streamNotifyCallback, this);
        pa_stream_set_read_callback(stream.get(), & streamReadCallback, this);

        const pa_buffer_attr attr = {
            .maxlength = UINT32_MAX,
            .tlength = UINT32_MAX,
            .prebuf = UINT32_MAX,
            .minreq = UINT32_MAX,
            .fragsize = 1024 };

        pa_stream_flags_t flags = PA_STREAM_ADJUST_LATENCY; /* PA_STREAM_NOFLAGS */
    
        monitorName = std::string(defaultSink ? info->default_sink_name : info->default_source_name).append(".monitor");

        if(0 != pa_stream_connect_record(stream.get(), monitorName.c_str(), & attr, flags))
            throw std::runtime_error("pa_stream_connect_record failed");
    }

    BufSamples Context::popDataBuf(void)
    {
        const std::lock_guard<std::mutex> lock(dataLock);
        return std::move(dataBuf);
    }

    void Context::streamNotifyCallback(pa_stream* stream, void* userData)
    {
        auto state = pa_stream_get_state(stream);

        if(PA_STREAM_READY == state)
        {
        }
        else
        if(PA_STREAM_FAILED == state)
            throw std::runtime_error("pa_stream_get_state failed");
    }

    void Context::streamReadCallback(pa_stream* stream, const size_t nbytes, void* userData)
    {
        const void* streamData = nullptr;
        size_t streamBytes = 0;

        if(0 == pa_stream_peek(stream, & streamData, & streamBytes))
        {
            auto context = static_cast<Context*>(userData);

            if(streamData && streamBytes && context)
            {
                const std::lock_guard<std::mutex> lock(context->dataLock);
                const size_t reserve = 1024 * 1024;

                if(context->dataBuf.capacity() < reserve)
                    context->dataBuf.reserve(reserve);

                if(context->dataBuf.capacity() < context->dataBuf.size() + streamBytes)
                    context->dataBuf.clear();

                auto begin = static_cast<const uint8_t*>(streamData);
                context->dataBuf.insert(context->dataBuf.end(), begin, begin + streamBytes);
            }

            if(streamBytes)
                pa_stream_drop(stream);
        }
    }

    void Context::serverInfoCallback(pa_context* ctx, const pa_server_info* info, void* userData)
    {
        if(auto context = static_cast<Context*>(userData))
            context->streamCreate(info);
    }

    void Context::connectNotifyCallback(pa_context* ctx, void* userData)
    {
        auto state = pa_context_get_state(ctx);

        if(PA_CONTEXT_READY == state) {
            if(auto op = pa_context_get_server_info(ctx, & serverInfoCallback, userData) ) {
                pa_operation_unref( op );
            }
        } else
        if(PA_CONTEXT_FAILED == state)
            throw std::runtime_error("pa_context_get_state failed");
    }
}
