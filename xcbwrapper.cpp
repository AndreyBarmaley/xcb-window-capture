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

#include <exception>

#include <QDebug>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "xcbwrapper.h"

#define getReplyFunc1(NAME,conn,...) getReply1<NAME##_reply_t,NAME##_cookie_t>(NAME##_reply,conn,NAME(conn,##__VA_ARGS__))

QString GenericError::toString(const char* func) const
{
    auto err = get();

    if(err)
    {
        auto str = QString("error code: %1, major: 0x%2, minor: 0x%3, sequence: %4").
            arg((int) err->error_code).
            arg(err->major_code, 2, 16, QChar('0')).
            arg(err->minor_code, 4, 16, QChar('0')).
            arg((uint) err->sequence);

        if(func)
            return QString(func).append(" ").append(str);
    
        return str;
    }

    return nullptr;
}

/* Xcb Composite */
XcbComposite::XcbComposite(xcb_connection_t* conn)
{
    auto xcomp = xcb_get_extension_data(conn, & xcb_composite_id);

    if(! xcomp || ! xcomp->present)
    {
        qWarning() << "xcb_composite failed";
        throw xcb_error(__FUNCTION__);
    }

    auto xcbReply = getReplyFunc1(xcb_composite_query_version, conn, XCB_COMPOSITE_MAJOR_VERSION, XCB_COMPOSITE_MINOR_VERSION);

    if(auto & err = xcbReply.error())
    {
        qWarning() << err.toString("xcb_composite_query_version");
        throw xcb_error(__FUNCTION__);
    }

    if(auto & reply = xcbReply.reply())
    {
        qDebug() << QString("composite version: %1.%2").arg(reply->major_version).arg(reply->minor_version);
    }
    else
    {
        qWarning() << "xcb_composite_query_version failed";
        throw xcb_error(__FUNCTION__);
    }
}

bool XcbComposite::redirectWindow(xcb_connection_t* conn, xcb_window_t win, bool autoUpdate) const
{
    auto cookie = xcb_composite_redirect_window_checked(conn, win, autoUpdate ? XCB_COMPOSITE_REDIRECT_AUTOMATIC : XCB_COMPOSITE_REDIRECT_MANUAL);
 
    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_composite_redirect_window");
        return false;
    }

    return true;
}

bool XcbComposite::unredirectWindow(xcb_connection_t* conn, xcb_window_t win, bool autoUpdate) const
{
    auto cookie = xcb_composite_unredirect_window_checked(conn, win, autoUpdate ? XCB_COMPOSITE_REDIRECT_AUTOMATIC : XCB_COMPOSITE_REDIRECT_MANUAL);
 
    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_composite_unredirect_window");
        return false;
    }

    return true;
}

bool XcbComposite::redirectSubWindows(xcb_connection_t* conn, xcb_window_t win, bool autoUpdate) const
{
    auto cookie = xcb_composite_redirect_subwindows_checked(conn, win, autoUpdate ? XCB_COMPOSITE_REDIRECT_AUTOMATIC : XCB_COMPOSITE_REDIRECT_MANUAL);
 
    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_composite_redirect_window");
        return false;
    }

    return true;
}

bool XcbComposite::unredirectSubWindows(xcb_connection_t* conn, xcb_window_t win, bool autoUpdate) const
{
    auto cookie = xcb_composite_unredirect_subwindows_checked(conn, win, autoUpdate ? XCB_COMPOSITE_REDIRECT_AUTOMATIC : XCB_COMPOSITE_REDIRECT_MANUAL);
 
    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_composite_unredirect_subwindows");
        return false;
    }

    return true;
}

bool XcbComposite::nameWindowPixmap(xcb_connection_t* conn, xcb_window_t win, xcb_pixmap_t pix) const
{
    auto cookie = xcb_composite_name_window_pixmap_checked(conn, win, pix);
 
    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_composite_name_window_pixmap");
        return false;
    }

    return true;
}

xcb_pixmap_t XcbComposite::nameWindowPixmap(xcb_connection_t* conn, xcb_window_t win) const
{
    xcb_pixmap_t pixmap = xcb_generate_id(conn);
    auto cookie = xcb_composite_name_window_pixmap_checked(conn, win, pixmap);
 
    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_composite_name_window_pixmap");
        return XCB_PIXMAP_NONE;
    }

    return pixmap;
}

xcb_window_t XcbComposite::getOverlayWindow(xcb_connection_t* conn, xcb_window_t win) const
{
    auto xcbReply = getReplyFunc1(xcb_composite_get_overlay_window, conn, win);

    if(auto & err = xcbReply.error())
    {
        qWarning() << err.toString("xcb_composite_get_overlay_window");
        return XCB_WINDOW_NONE;
    }

    if(auto & reply = xcbReply.reply())
        return reply->overlay_win;

    return XCB_WINDOW_NONE;
}

bool XcbComposite::releaseOverlayWindow(xcb_connection_t* conn, xcb_window_t win) const
{
    auto cookie = xcb_composite_release_overlay_window_checked(conn, win);
 
    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_composite_release_overlay_window");
        return false;
    }

    return true;
}

/* Xcb Shm */
XcbShm::XcbShm(xcb_connection_t* conn)
{
    auto shm = xcb_get_extension_data(conn, &xcb_shm_id);
    if(! shm || ! shm->present)
    {
        qWarning() << "xcb_shm failed";
        throw xcb_error(__FUNCTION__);
    }

    auto xcbReply = getReplyFunc1(xcb_shm_query_version, conn);

    if(auto & err = xcbReply.error())
    {
        qWarning() << err.toString("xcb_shm_query_version");
        throw xcb_error(__FUNCTION__);
    }

    if(auto & reply = xcbReply.reply())
    {
        qDebug() << QString("shm version: %1.%2").arg((int) reply->major_version).arg((int) reply->minor_version);
    }
    else
    {
        qWarning() << "xcb_shm_query_version failed";
        throw xcb_error(__FUNCTION__);
    }
}

bool XcbShm::attach(xcb_connection_t* conn, xcb_shm_seg_t seg, uint32_t shmid, bool readOnly) const
{
    auto cookie = xcb_shm_attach(conn, seg, shmid, readOnly);
 
    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_shm_attach");
        return false;
    }

    return true;
}

bool XcbShm::detach(xcb_connection_t* conn, xcb_shm_seg_t seg) const
{
    auto cookie = xcb_shm_detach_checked(conn, seg);
 
    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_shm_detach");
        return false;
    }

    return true;
}

bool XcbShm::putImage(xcb_connection_t* conn, xcb_drawable_t drawable, xcb_gcontext_t gc, const QSize & total, const QRect & src, const QPoint & dst, uint8_t depth, uint8_t format, uint8_t send_event, xcb_shm_seg_t shmseg, uint32_t offset) const
{
    auto cookie = xcb_shm_put_image(conn, drawable, gc, total.width(), total.height(), 
        src.x(), src.y(), src.width(), src.height(), dst.x(), dst.y(), depth, format, send_event, shmseg, offset);

    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_shm_put_image");
        return false;
    }

    return true;
}

bool XcbShm::createPixmap(xcb_connection_t* conn, xcb_pixmap_t pid, xcb_drawable_t drawable, const QSize & sz, uint8_t depth, xcb_shm_seg_t shmseg, uint32_t offset) const
{
    auto cookie = xcb_shm_create_pixmap_checked(conn, pid, drawable, sz.width(), sz.height(), depth, shmseg, offset);
 
    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_shm_create_pixmap_checked");
        return false;
    }

    return true;
}

/* Xcb Shm Pixmap */
XcbShmPixmap::XcbShmPixmap(xcb_connection_t* conn, size_t sz) : XcbShm(conn)
{
    // init shm
    shmid = shmget(IPC_PRIVATE, sz, IPC_CREAT | S_IRUSR | S_IWUSR);

    if(shmid == -1)
    {
        qWarning() << "shmget failed";
        throw xcb_error(__FUNCTION__);
    }

    addr = reinterpret_cast<uint8_t*>(shmat(shmid, 0, 0));

    // man shmat: check result
    if(addr == reinterpret_cast<uint8_t*>(-1))
    {
        qWarning() << "shmat failed";
        throw xcb_error(__FUNCTION__);
    }

    shmseg = xcb_generate_id(conn);

    if(! attach(conn, shmseg, shmid, false))
        throw xcb_error(__FUNCTION__);
}

XcbShmPixmap::~XcbShmPixmap()
{
    if(addr)
        shmdt(addr);
    
    if(0 <= shmid)
        shmctl(shmid, IPC_RMID, 0);
}

bool XcbShmPixmap::detach(xcb_connection_t* conn) const
{
    return XcbShm::detach(conn, shmseg);
}


XcbShmGetImageReply XcbShmPixmap::getImageReply(xcb_connection_t* conn, xcb_drawable_t drawable, const QRect & reg, uint32_t offset)
{
    auto xcbReply = getReplyFunc1(xcb_shm_get_image, conn, drawable, reg.x(), reg.y(), reg.width(), reg.height(),
                                    0xFFFFFFFF, XCB_IMAGE_FORMAT_Z_PIXMAP, shmseg, offset);
    if(auto & err = xcbReply.error())
    {
        qWarning() << err.toString("xcb_shm_get_image");
        return nullptr;
    }

    return std::move(xcbReply.first);
}

XcbPixmapInfoReply XcbShmPixmap::getPixmap(const XcbShmGetImageReply & reply) const
{
    return reply ? std::make_unique<PixmapInfoShm>(reply->depth, reply->visual, addr, reply->size) : nullptr;
}

/* Xcb Xfixes */
XcbXfixes::XcbXfixes(xcb_connection_t* conn)
{
    auto xfixes = xcb_get_extension_data(conn, &xcb_xfixes_id);
    if(! xfixes || ! xfixes->present)
    {
        qWarning() << "xcb_xfixes failed";
        throw xcb_error(__FUNCTION__);
    }

    auto xcbReply = getReplyFunc1(xcb_xfixes_query_version, conn, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);

    if(auto & err = xcbReply.error())
    {
        qWarning() << err.toString("xcb_xfixes_query_version");
        throw xcb_error(__FUNCTION__);
    }

    if(auto & reply = xcbReply.reply())
    {
        qDebug() << QString("xfixes version: %1.%2").arg((int) reply->major_version).arg((int) reply->minor_version);
    }
    else
    {
        qWarning() << "xcb_xfixes_query_version failed";
        throw xcb_error(__FUNCTION__);
    }
}

XcbXfixesGetCursorImageReply XcbXfixes::getCursorImageReply(xcb_connection_t* conn) const
{
    auto xcbReply = getReplyFunc1(xcb_xfixes_get_cursor_image, conn);

    if(auto & err = xcbReply.error())
    {
        qWarning() << err.toString("xcb_xfixes_get_cursor_image");
        return nullptr;
    }

    return std::move(xcbReply.first);
}

uint32_t* XcbXfixes::getCursorImageData(const XcbXfixesGetCursorImageReply & reply) const
{
    return xcb_xfixes_get_cursor_image_cursor_image(reply.get());
}

size_t XcbXfixes::getCursorImageLength(const XcbXfixesGetCursorImageReply & reply) const
{
    return xcb_xfixes_get_cursor_image_cursor_image_length(reply.get());
}

/* XcbConnection */
XcbConnection::XcbConnection() :
    conn{ xcb_connect(nullptr, nullptr), xcb_disconnect }, screen(nullptr), format(nullptr)
{
    if(xcb_connection_has_error(conn.get()))
        throw std::runtime_error("xcb connect");

    auto setup = xcb_get_setup(conn.get());
    if(! setup)
        throw std::runtime_error("xcb init setup");

    screen = xcb_setup_roots_iterator(setup).data;
    if(! screen)
        throw std::runtime_error("xcb init screen");

    format = findFormat(screen->root_depth);
    if(! format)
        throw std::runtime_error("xcb init format");

    const int bpp = format->bits_per_pixel >> 3;
    const int pagesz = 4096;
    auto winsz = getWindowSize(screen->root);
    const size_t shmsz = ((winsz.width() * winsz.height() * bpp / pagesz) + 1) * pagesz;

    // shm
    try
    {
        shmpix = std::make_unique<XcbShmPixmap>(conn.get(), shmsz);
    }
    catch( const xcb_error &)
    {
        qWarning() << "shm pixmap init failed";
    }

    // composite
    try
    {
        composite = std::make_unique<XcbComposite>(conn.get());
    }
    catch( const xcb_error &)
    {
        qWarning() << "composite init failed";
    }

    // xfixes
    try
    {
        xfixes = std::make_unique<XcbXfixes>(conn.get());
    }
    catch( const xcb_error &)
    {
        qWarning() << "xfixes init failed";
    }

    // event filter
    const uint32_t values[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes(conn.get(), screen->root, XCB_CW_EVENT_MASK, values);

    xcb_flush(conn.get());
}

XcbConnection::~XcbConnection()
{
    if(shmpix)
        shmpix->detach(conn.get());
}

xcb_window_t XcbConnection::getWindowParent(xcb_window_t win) const
{
    if(screen->root != win)
    {
        auto xcbReply = getReplyFunc2(xcb_query_tree, conn.get(), win);

        if(auto & reply = xcbReply.reply())
            return reply->parent;

        qWarning() << "xcb_query_tree failed";
    }
    return XCB_WINDOW_NONE;
}

QPoint XcbConnection::translateCoordinates(xcb_window_t win, const QPoint & pos, xcb_window_t parent) const
{
    if(parent != XCB_WINDOW_NONE)
    {
        auto xcbReply = getReplyFunc2(xcb_translate_coordinates, conn.get(), win, parent, pos.x(), pos.y());

        if(auto & reply = xcbReply.reply())
            return QPoint(reply->dst_x, reply->dst_y);
    }

    return pos;
}

QRect XcbConnection::getWindowGeometry(xcb_window_t win, bool abspos) const
{
    auto xcbReply = getReplyFunc2(xcb_get_geometry, conn.get(), win);

    if(auto & reply = xcbReply.reply())
    {
        // ref: https://xcb.freedesktop.org/windowcontextandmanipulation/
        if(abspos)
        {
            auto parent = getWindowParent(win);

            if(parent == XCB_WINDOW_NONE)
                return QRect(reply->x, reply->y, reply->width, reply->height);

            auto geom = getWindowGeometry(parent, abspos);
            return QRect(reply->x + geom.x(), reply->y + geom.y(), reply->width, reply->height);
        }

        return QRect(reply->x, reply->y, reply->width, reply->height);
    }

    return QRect();
}

QPoint XcbConnection::getWindowPosition(xcb_window_t win, bool abspos) const
{
    return getWindowGeometry(win, abspos).topLeft();
}

QSize XcbConnection::getWindowSize(xcb_window_t win) const
{
    return getWindowGeometry(win).size();
}

QString XcbConnection::getAtomName(xcb_atom_t atom) const
{
    auto xcbReply = getReplyFunc2(xcb_get_atom_name, conn.get(), atom);

    if(auto & reply = xcbReply.reply())
    {
        const char* name = xcb_get_atom_name_name(reply.get());
        size_t len = xcb_get_atom_name_name_length(reply.get());
        return QString(QByteArray(name, len));
    }

    return QString("NONE");
}

xcb_format_t* XcbConnection::findFormat(int depth) const
{
    auto setup = xcb_get_setup(conn.get());
    if(! setup)
        throw std::runtime_error("xcb_get_setup");

    for(auto fIter = xcb_setup_pixmap_formats_iterator(setup); fIter.rem; xcb_format_next(& fIter))
    {
        if(depth == fIter.data->depth)
            return fIter.data;
    }

    return nullptr;
}

int XcbConnection::bppFromDepth(int depth) const
{
    auto setup = xcb_get_setup(conn.get());
    if(! setup)
        throw std::runtime_error("xcb_get_setup");

    for(auto fIter = xcb_setup_pixmap_formats_iterator(setup); fIter.rem; xcb_format_next(& fIter))
        if(fIter.data->depth == depth) return fIter.data->bits_per_pixel;

    return 0;
}

int XcbConnection::depthFromBPP(int bitsPerPixel) const
{
    auto setup = xcb_get_setup(conn.get());
    if(! setup)
        throw std::runtime_error("xcb_get_setup");

    for(auto fIter = xcb_setup_pixmap_formats_iterator(setup); fIter.rem; xcb_format_next(& fIter))
        if(fIter.data->bits_per_pixel == bitsPerPixel) return fIter.data->depth;

    return 0;
}

xcb_visualtype_t* XcbConnection::findVisual(xcb_visualid_t vid) const
{
    for(auto dIter = xcb_screen_allowed_depths_iterator(screen); dIter.rem; xcb_depth_next(& dIter))
    {
        for(auto vIter = xcb_depth_visuals_iterator(dIter.data); vIter.rem; xcb_visualtype_next(& vIter))
        {
            if(vid == vIter.data->visual_id)
                return vIter.data;
        }
    }

    return nullptr;
}

xcb_window_t XcbConnection::getScreenRoot(void) const
{
    return screen->root;
}

/*
xcb_screen_t* XcbConnection::getScreen(void) const
{
    return screen;
}
*/

xcb_atom_t XcbConnection::getAtom(const QString & name, bool create) const
{
    auto xcbReply = getReplyFunc2(xcb_intern_atom, conn.get(), create ? 0 : 1, name.length(), name.toStdString().c_str());

    if(xcbReply.error())
        return XCB_ATOM_NONE;

    return xcbReply.reply() ? xcbReply.reply()->atom : (xcb_atom_t) XCB_ATOM_NONE;
}

xcb_window_t XcbConnection::getActiveWindow(void) const
{
    auto xcbReply = getReplyFunc2(xcb_get_property, conn.get(), false, screen->root,
                        getAtom("_NET_ACTIVE_WINDOW"), XCB_ATOM_WINDOW, 0, 1);

    if(xcbReply.error())
        return XCB_WINDOW_NONE;

    if(auto & reply = xcbReply.reply())
    {
        if(auto res = static_cast<xcb_window_t*>(xcb_get_property_value(reply.get())))
            return *res;
    }

    return XCB_WINDOW_NONE;
}

XcbPropertyReply XcbConnection::getPropertyAnyType(xcb_window_t win, xcb_atom_t prop, uint32_t offset, uint32_t length) const
{
    auto xcbReply = getReplyFunc2(xcb_get_property, conn.get(), false, win, prop, XCB_GET_PROPERTY_TYPE_ANY, offset, length);

    if(auto & err = xcbReply.error())
        qWarning() << err.toString("xcb_get_property");

    return XcbPropertyReply(std::move(xcbReply.first));
}

xcb_atom_t XcbConnection::getPropertyType(xcb_window_t win, xcb_atom_t prop) const
{
    auto reply = getPropertyAnyType(win, prop, 0, 0);
    return reply ? reply->type : (xcb_atom_t) XCB_ATOM_NONE;
}

QStringList XcbConnection::getPropertyStringList(xcb_window_t win, xcb_atom_t prop) const
{
    QStringList res;

    if(XCB_ATOM_STRING == getPropertyType(win, prop))
    {
        if(auto reply = getPropertyAnyType(win, prop, 0, 8192))
        {
            auto len = reply.length();
            auto ptr = reinterpret_cast<const char*>(reply.value());

            for(auto & ba : QByteArray(ptr, len - (ptr[len - 1] ? 0 : 1 /* remove last nul */)).split(0))
                res << QString(ba);
        }
    }

    return res;
}

QString XcbConnection::getWindowName(xcb_window_t win) const
{
    QString res = getPropertyString(win, XCB_ATOM_WM_NAME);

    if(res.isEmpty())
    {
        auto utf8 = getAtom("UTF8_STRING");
        auto prop = getAtom("_NET_WM_NAME");
        auto type = getPropertyType(win, prop);

        if(type == utf8)
        {
            if(auto reply = getPropertyAnyType(win, prop, 0, 8192))
            {
                auto ptr = reinterpret_cast<const char*>(reply.value());
                if(ptr) res.append(ptr);
            }
        }
    }

    return res;
}

QString XcbConnection::getPropertyString(xcb_window_t win, xcb_atom_t prop) const
{
    if(XCB_ATOM_STRING == getPropertyType(win, prop))
    {
        if(auto reply = getPropertyAnyType(win, prop, 0, 8192))
        {
            auto ptr = reinterpret_cast<const char*>(reply.value());
            if(ptr) return QString(ptr);
        }
    }

    return nullptr;
}

QList<xcb_window_t> XcbConnection::getWindowList(void) const
{
    QList<xcb_window_t> res;
    auto prop = getAtom("_NET_CLIENT_LIST");

    if(XCB_ATOM_WINDOW == getPropertyType(screen->root, prop))
    {
        if(auto reply = getPropertyAnyType(screen->root, prop, 0, 1024))
        {
            auto counts = reply.length() / sizeof(xcb_window_t);
            auto wins = reinterpret_cast<xcb_window_t*>(reply.value());

            for(uint32_t it = 0; it < counts; ++it)
                res << wins[it];
        }
    }

    return res;
}

WinFrameSize XcbConnection::getWindowFrame(xcb_window_t win) const
{
    WinFrameSize res;
    auto prop = getAtom("_NET_FRAME_EXTENTS");

    // left, right, top, bottom, CARDINAL[4]/32
    if(auto reply = getPropertyAnyType(win, prop, 0, 16))
    {
        if(16 > reply.length())
            throw std::runtime_error("_NET_FRAME_EXTENTS empty");

        auto vals = reinterpret_cast<uint32_t*>(reply.value());
        res.left = vals[0];
        res.right = vals[1];
        res.top = vals[2];
        res.bottom = vals[3];
    }

    return res;
}

XcbPixmapInfoReply XcbConnection::getWindowRegion(xcb_window_t win, const QRect & reg, QString* errstr) const
{
    if(shmpix)
    {
        auto reply = shmpix->getImageReply(conn.get(), win, reg);
        return shmpix->getPixmap(reply);
    }

    int pitch = reg.width() * (format->bits_per_pixel >> 2);

    if(0 >= pitch || 0 >= reg.height())
    {
        auto msg = QString("incorrect size: %1 %2").arg(reg.width()).arg(reg.height());
        qWarning() << msg;
        if(errstr)
            *errstr = msg;
        return nullptr;
    }

    XcbPixmapInfoReply res;
    PixmapInfoBuffer* info = nullptr;

    const uint32_t planeMask = 0xFFFFFFFF;
    const uint32_t maxReqLength = xcb_get_maximum_request_length(conn.get());
    uint32_t allowRows = qMin(maxReqLength / pitch, (uint32_t) reg.height());

    for(int64_t yy = reg.y(); yy < reg.y() + reg.height(); yy += allowRows)
    {
        // last rows
        if(yy + allowRows > reg.y() + reg.height())
            allowRows = reg.y() + reg.height() - yy;

        auto xcbReply = getReplyFunc2(xcb_get_image, conn.get(), XCB_IMAGE_FORMAT_Z_PIXMAP, win, reg.x(), yy, reg.width(), allowRows, planeMask);

        if(auto & err = xcbReply.error())
        {
            auto msg = err.toString("xcb_get_image");
            qWarning() << msg;
            if(errstr)
                *errstr = msg;
            break;
        }

        if(auto & reply = xcbReply.reply())
        {
            if(! res)
            {
                info = new PixmapInfoBuffer(reply->depth, reply->visual, reg.height() * pitch);
                res.reset(info);
            }

            auto length = xcb_get_image_data_length(reply.get());
            auto data = xcb_get_image_data(reply.get());
            auto & pixels = info->pixels();

            pixels.insert(pixels.end(), data, data + length);
        }
    }

    return res;
}
