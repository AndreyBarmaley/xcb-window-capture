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

#include "pulseaudio.h"

namespace PulseAudio
{
    Context::Context(const char* appname)
    {
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

    std::vector<uint8_t> Context::popDataBuf(void)
    {
        const std::lock_guard<std::mutex> lock(dataLock);

        std::vector<uint8_t> res;
        auto size = std::accumulate(dataBuf.begin(), dataBuf.end(), 0,
                                [](int res, auto & val){ return res += val.size(); });
        if(0 < size)
        {
            for(auto & vec : dataBuf)
                res.insert(res.end(), vec.begin(), vec.end());

            dataBuf.clear();
        }

        return res;
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
                auto begin = static_cast<const uint8_t*>(streamData);
                context->dataBuf.emplace_back(begin, begin + streamBytes);
            }

            if(streamBytes)
                pa_stream_drop(stream);
        }
    }

    void Context::serverInfoCallback(pa_context* ctx, const pa_server_info* info, void* userData)
    {
        pa_sample_spec spec;
        spec.format = PA_SAMPLE_S16LE;
        spec.rate = 44100;
        spec.channels = 1;

        pa_stream* stream = pa_stream_new(ctx, "capture monitor", &spec, nullptr);

        pa_stream_set_state_callback(stream, & streamNotifyCallback, userData);
        pa_stream_set_read_callback(stream, & streamReadCallback, userData);

        auto monitorName = std::string(info->default_sink_name).append(".monitor");

        if(0 != pa_stream_connect_record(stream, monitorName.c_str(), nullptr, PA_STREAM_NOFLAGS))
            throw std::runtime_error("pa_stream_connect_record failed");
    }

    void Context::connectNotifyCallback(pa_context* ctx, void* userData)
    {
        auto state = pa_context_get_state(ctx);

        if(PA_CONTEXT_READY == state)
            pa_context_get_server_info(ctx, & serverInfoCallback, userData);
        else
        if(PA_CONTEXT_FAILED == state)
            throw std::runtime_error("pa_context_get_state failed");
    }
}
