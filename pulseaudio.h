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
             pa_context_disconnect(ctx);
        }
    };

    class Context
    {
    protected:
        std::unique_ptr<pa_mainloop, MainLoopDeleter> loop;
        std::unique_ptr<pa_context, ContextDeleter> ctx;

        std::thread thread;

        std::mutex dataLock;
        std::list<std::vector<uint8_t>> dataBuf;

        static void connectNotifyCallback(pa_context*, void*);
        static void serverInfoCallback(pa_context*, const pa_server_info*, void*);
        static void streamNotifyCallback(pa_stream* stream, void*);
        static void streamReadCallback(pa_stream*, const size_t, void*);

    public:
        Context(const char* appname);
        ~Context();

        std::vector<uint8_t> popDataBuf(void);
    };
}

#endif // PULSE_AUDIO
