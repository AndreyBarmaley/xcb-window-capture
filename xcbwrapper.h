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
#include "xcb/xfixes.h"
#include "xcb/composite.h"

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


/// XcbPixmapInfo
class XcbPixmapInfo
{
protected:
    int depth = 0;
    xcb_visualid_t visual = 0;

public:
    XcbPixmapInfo() = default;
    virtual ~XcbPixmapInfo() = default;

    XcbPixmapInfo(int d, xcb_visualid_t v) : depth(d), visual(v) {}

    int pixmapDepth(void) const { return depth; }
    const xcb_visualid_t & pixmapVisual(void) const { return visual; }

    virtual const uint8_t* pixmapData(void) const = 0;
    virtual uint8_t* pixmapData(void) = 0;
    virtual size_t pixmapSize(void) const = 0;

};

typedef std::unique_ptr<XcbPixmapInfo> XcbPixmapInfoReply;

struct xcb_error : public std::runtime_error
{
    explicit xcb_error( const std::string_view err ) : std::runtime_error( err.data() ) {}
};

/// PixmapInfoBuffer
class PixmapInfoBuffer : public XcbPixmapInfo
{
protected:
    std::vector<uint8_t> buf;

public:
    PixmapInfoBuffer() = default;

    PixmapInfoBuffer(int depth, xcb_visualid_t visual, size_t res = 0)
        : XcbPixmapInfo(depth, visual) { buf.reserve(res); }

    uint8_t* pixmapData(void) override { return buf.data(); }
    const uint8_t* pixmapData(void) const override { return buf.data(); }
    size_t pixmapSize(void) const override { return buf.size(); }

    std::vector<uint8_t> & pixels(void) { return buf; };
    const std::vector<uint8_t> & pixels(void) const { return buf; };
};

/// PixmapInfoShm
class PixmapInfoShm : public XcbPixmapInfo
{
protected:
    uint8_t* buf = nullptr;
    uint32_t len = 0;

public:
    PixmapInfoShm() = default;

    PixmapInfoShm(int depth, xcb_visualid_t visual, uint8_t* ptr, size_t sz)
        : XcbPixmapInfo(depth, visual), buf(ptr), len(sz) {}

    uint8_t* pixmapData(void) override { return buf; }
    const uint8_t* pixmapData(void) const override { return buf; }
    size_t pixmapSize(void) const override { return len; }
};

/// XcbComposite
class XcbComposite
{
protected:

public:
    XcbComposite(xcb_connection_t* conn);

    bool redirectWindow(xcb_connection_t*, xcb_window_t, bool autoUpdate = true) const;
    bool unredirectWindow(xcb_connection_t*, xcb_window_t, bool autoUpdate = true) const;

    bool redirectSubWindows(xcb_connection_t*, xcb_window_t, bool autoUpdate = true) const;
    bool unredirectSubWindows(xcb_connection_t*, xcb_window_t, bool autoUpdate = true) const;

    bool nameWindowPixmap(xcb_connection_t*, xcb_window_t, xcb_pixmap_t) const;
    xcb_pixmap_t nameWindowPixmap(xcb_connection_t*, xcb_window_t) const;

    xcb_window_t getOverlayWindow(xcb_connection_t*, xcb_window_t) const;
    bool releaseOverlayWindow(xcb_connection_t*, xcb_window_t) const;
};

/// XcbShm
class XcbShm
{
protected:

public:
    XcbShm(xcb_connection_t* conn);

    bool attach(xcb_connection_t*, xcb_shm_seg_t, uint32_t shmid, bool readOnly = false) const;
    bool detach(xcb_connection_t*, xcb_shm_seg_t) const;

    bool putImage(xcb_connection_t*, xcb_drawable_t drawable, xcb_gcontext_t gc, const QSize & total, const QRect & src, const QPoint & dst, uint8_t depth, uint8_t format, uint8_t send_event, xcb_shm_seg_t shmseg, uint32_t offset = 0) const;
    bool createPixmap(xcb_connection_t*, xcb_pixmap_t pid, xcb_drawable_t drawable, const QSize &, uint8_t depth, xcb_shm_seg_t shmseg, uint32_t offset = 0) const;
};

typedef GenericReply<xcb_shm_get_image_reply_t> XcbShmGetImageReply;

/// XcbShPixmap
class XcbShmPixmap : protected XcbShm
{
    int shmid = -1;
    uint8_t* addr = nullptr;
    xcb_shm_seg_t shmseg = XCB_NONE;

public:
    XcbShmPixmap(xcb_connection_t* conn, size_t);
    ~XcbShmPixmap();

    XcbShmGetImageReply getImageReply(xcb_connection_t*, xcb_drawable_t drawable, const QRect & reg, uint32_t offset = 0);

    XcbPixmapInfoReply getPixmap(const XcbShmGetImageReply &) const;
    bool detach(xcb_connection_t*) const;
};

typedef GenericReply<xcb_xfixes_get_cursor_image_reply_t> XcbXfixesGetCursorImageReply;

/// XcbXfixes
class XcbXfixes
{
protected:

public:
    XcbXfixes(xcb_connection_t* conn);

    XcbXfixesGetCursorImageReply getCursorImageReply(xcb_connection_t*) const;

    uint32_t* getCursorImageData(const XcbXfixesGetCursorImageReply &) const;
    size_t getCursorImageLength(const XcbXfixesGetCursorImageReply &) const;
};

struct WinFrameSize
{
    uint32_t left = 0;
    uint32_t right = 0;
    uint32_t top = 0;
    uint32_t bottom = 0;
};

/// XcbConnection
struct XcbConnection
{
protected:
    std::unique_ptr<xcb_connection_t, decltype(xcb_disconnect)*> conn;

    std::unique_ptr<XcbXfixes> xfixes;
    std::unique_ptr<XcbShmPixmap> shmpix;
    std::unique_ptr<XcbComposite> composite;

    xcb_screen_t* screen;
    xcb_format_t* format;

public:
    XcbConnection();
    virtual ~XcbConnection();

    xcb_format_t* findFormat(int depth) const;
    xcb_visualtype_t* findVisual(xcb_visualid_t vid) const;
    xcb_atom_t getAtom(const QString & name, bool create = true) const;

    QRect getWindowGeometry(xcb_window_t, bool abspos = true) const;
    QSize getWindowSize(xcb_window_t) const;

    QPoint getWindowPosition(xcb_window_t, bool abspos = true) const;
    QPoint translateCoordinates(xcb_window_t, const QPoint &, xcb_window_t parent = XCB_WINDOW_NONE) const;

    QString getWindowName(xcb_window_t) const;
    xcb_window_t getWindowParent(xcb_window_t) const;
    xcb_window_t getActiveWindow(void) const;
    xcb_window_t getScreenRoot(void) const;
    //xcb_screen_t* getScreen(void) const;
    QList<xcb_window_t> getWindowList(void) const;
    WinFrameSize getWindowFrame(xcb_window_t) const;
    QString getAtomName(xcb_atom_t) const;

    xcb_connection_t* connection(void) const { return conn.get(); }

    const XcbXfixes* getXfixesExtension(void) const { return xfixes.get(); }
    const XcbShmPixmap* getShmExtension(void) const { return shmpix.get(); }
    const XcbComposite* getCompositeExtension(void) const { return composite.get(); }

    int bppFromDepth(int depth) const;
    int depthFromBPP(int bitsPerPixel) const;

    XcbPropertyReply getPropertyAnyType(xcb_window_t win, xcb_atom_t prop, uint32_t offset = 0, uint32_t length = 0xFFFFFFFF) const;
    xcb_atom_t getPropertyType(xcb_window_t win, xcb_atom_t prop) const;
    QStringList getPropertyStringList(xcb_window_t win, xcb_atom_t prop) const;
    QString getPropertyString(xcb_window_t win, xcb_atom_t prop) const;

    XcbPixmapInfoReply getWindowRegion(xcb_window_t, const QRect &, QString* errstr = nullptr) const;

    template<typename Reply, typename Cookie>
    ReplyError<Reply> getReply2(std::function<Reply*(xcb_connection_t*, Cookie, xcb_generic_error_t**)> func, Cookie cookie) const
    {
        return getReply1<Reply, Cookie>(func, conn.get(), cookie);
    }

#define getReplyFunc2(NAME,conn,...) getReply2<NAME##_reply_t,NAME##_cookie_t>(NAME##_reply,NAME(conn,##__VA_ARGS__))
};

#endif // XCB_WRAPPER_H
