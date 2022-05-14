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

#ifndef XCB_WRAPPER_H
#define XCB_WRAPPER_H

#include <memory>

#include <QSize>
#include <QPair>
#include <QRect>
#include <QList>
#include <QString>
#include <QStringList>

#include "xcb/xcb.h"
#include "xcb/shm.h"

template<typename ReplyType>
struct GenericReply : std::shared_ptr<ReplyType>
{
    GenericReply(ReplyType* ptr) : std::shared_ptr<ReplyType>(ptr, std::free) {}
};

struct GenericError : std::shared_ptr<xcb_generic_error_t>
{
    GenericError(xcb_generic_error_t* err) : std::shared_ptr<xcb_generic_error_t>(err, std::free) {}
    QString toString(const char* func = nullptr) const;
};

struct GenericEvent : std::shared_ptr<xcb_generic_event_t>
{
    GenericEvent(xcb_generic_event_t* ev) : std::shared_ptr<xcb_generic_event_t>(ev, std::free) {}
    const xcb_generic_error_t*  toerror(void) const { return reinterpret_cast<const xcb_generic_error_t*>(get()); }
};

template<typename ReplyType>
struct ReplyError : std::pair<GenericReply<ReplyType>, GenericError>
{
    ReplyError(ReplyType* ptr, xcb_generic_error_t* err) : std::pair<GenericReply<ReplyType>, GenericError>(ptr, err) {}

    const GenericReply<ReplyType> & reply(void) const { return std::pair<GenericReply<ReplyType>, GenericError>::first; }
    const GenericError & error(void) const { return std::pair<GenericReply<ReplyType>, GenericError>::second; }
};

template<typename Reply, typename Cookie>
ReplyError<Reply> getReply1(std::function<Reply*(xcb_connection_t*, Cookie, xcb_generic_error_t**)> func, xcb_connection_t* conn, Cookie cookie)
{
    xcb_generic_error_t* error = nullptr;
    Reply* reply = func(conn, cookie, & error);
    return ReplyError<Reply>(reply, error);
}

/// XcbPropertyReply
struct XcbPropertyReply : GenericReply<xcb_get_property_reply_t>
{
    uint32_t length(void) { return xcb_get_property_value_length(get()); }
    void* value(void) { return xcb_get_property_value(get()); }
        
    XcbPropertyReply(xcb_get_property_reply_t* ptr) : GenericReply<xcb_get_property_reply_t>(ptr) {}
    XcbPropertyReply(const GenericReply<xcb_get_property_reply_t> & ptr) : GenericReply<xcb_get_property_reply_t>(ptr) {}
};


/// XcbPixmapInfoBase
class XcbPixmapInfoBase
{
protected:
    size_t _depth;
    xcb_visualid_t _visid;

public:
    XcbPixmapInfoBase(uint8_t d = 0, xcb_visualid_t v = 0) : _depth(d), _visid(v) {}
    virtual ~XcbPixmapInfoBase() {}

    size_t depth(void) const { return _depth; }
    xcb_visualid_t visId(void) const { return _visid; }

    virtual uint8_t* data(void) = 0;
    virtual const uint8_t* data(void) const = 0;
    virtual size_t size(void) const = 0;
};

typedef std::shared_ptr<XcbPixmapInfoBase> XcbPixmapInfoReply;

struct shm_t
{
    uint32_t _xcb;
    xcb_connection_t* _conn;
    int _shm;
    uint8_t* _addr;

    shm_t(int shmid, uint8_t* ptr, xcb_connection_t* xcon, uint32_t id = 0) : _xcb(id), _conn(xcon), _shm(shmid), _addr(ptr) {}
    ~shm_t();
};

struct shm_error
{
    const char* _err;
    shm_error(const char* err) : _err(err) {}
};

/// XcbSHM
struct XcbSHM : std::shared_ptr<shm_t>
{
    XcbSHM() {}
    XcbSHM(int shmid, uint8_t* addr, xcb_connection_t*);

    QPair<XcbPixmapInfoReply, GenericError>
        getPixmapRegion(xcb_drawable_t, const QRect &, size_t offset = 0, uint32_t planeMask = 0xFFFFFFFF) const;

    uint8_t*  data(void);
    const uint8_t* data(void) const;
    xcb_connection_t* connection(void) const;
    uint32_t xid(void) const;
};

/// XcbPixmapInfoSHM
class XcbPixmapInfoSHM : public XcbPixmapInfoBase
{
protected:
    XcbSHM _shm;
    size_t _size;

public:
    XcbPixmapInfoSHM() : _size(0) {}
    XcbPixmapInfoSHM(uint8_t depth, xcb_visualid_t vis, const XcbSHM & shm, size_t len)
        : XcbPixmapInfoBase(depth, vis), _shm(shm), _size(len) {}

    uint8_t* data(void) override;
    const uint8_t* data(void) const override;
    size_t size(void) const override;

    XcbSHM shm(void) const { return _shm; }
};

/// XcbPixmapInfoBuffer
class XcbPixmapInfoBuffer : public XcbPixmapInfoBase
{
protected:
    std::vector<uint8_t> _pixels;

public:
    XcbPixmapInfoBuffer() {}
    XcbPixmapInfoBuffer(uint8_t depth, xcb_visualid_t vis, size_t res = 0)
        : XcbPixmapInfoBase(depth, vis) { _pixels.reserve(res); }

    uint8_t* data(void) override;
    const uint8_t* data(void) const override;
    size_t size(void) const override;

    std::vector<uint8_t> & pixels(void) { return _pixels; };
    const std::vector<uint8_t> & pixels(void) const { return _pixels; };
};

/// XcbConnection
struct XcbConnection
{
protected:
    std::unique_ptr<xcb_connection_t, decltype(xcb_disconnect)*> conn;
    xcb_screen_t* screen;
    xcb_format_t* format;
    XcbSHM extSHM;

    bool        initSHM(void);
    XcbSHM      getSHM(size_t);

public:
    XcbConnection();
    virtual ~XcbConnection() {}

    xcb_format_t* findFormat(int depth) const;
    xcb_visualtype_t* findVisual(xcb_visualid_t vid) const;
    xcb_atom_t getAtom(const QString & name, bool create = true) const;
    QSize getWindowSize(xcb_window_t) const;
    QString getWindowName(xcb_window_t) const;
    xcb_window_t getActiveWindow(void) const;
    xcb_screen_t* getScreen(void) const;
    QList<xcb_window_t> getWindowList(void) const;
    QString getAtomName(xcb_atom_t) const;

    xcb_connection_t* connection(void) const { return conn.get(); }

    int bppFromDepth(int depth) const;
    int depthFromBPP(int bitsPerPixel) const;

    XcbPropertyReply getPropertyAnyType(xcb_window_t win, xcb_atom_t prop, uint32_t offset = 0, uint32_t length = 0xFFFFFFFF) const;
    xcb_atom_t getPropertyType(xcb_window_t win, xcb_atom_t prop) const;
    QStringList getPropertyStringList(xcb_window_t win, xcb_atom_t prop) const;
    QString getPropertyString(xcb_window_t win, xcb_atom_t prop) const;

    QPair<XcbPixmapInfoReply, QString>
        getWindowRegion(xcb_window_t, const QRect &, uint32_t planeMask = 0xFFFFFFFF) const;

    template<typename Reply, typename Cookie>
    ReplyError<Reply> getReply2(std::function<Reply*(xcb_connection_t*, Cookie, xcb_generic_error_t**)> func, Cookie cookie) const
    {
        return getReply1<Reply, Cookie>(func, conn.get(), cookie);
    }

#define getReplyFunc2(NAME,conn,...) getReply2<NAME##_reply_t,NAME##_cookie_t>(NAME##_reply,NAME(conn,##__VA_ARGS__))
};

#endif // XCB_WRAPPER_H
