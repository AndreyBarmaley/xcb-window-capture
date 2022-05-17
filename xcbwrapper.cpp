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

shm_t::~shm_t()
{
    if(_xcb) xcb_shm_detach(_conn, _xcb);

    if(_addr) shmdt(_addr);

    if(0 < _shm) shmctl(_shm, IPC_RMID, 0);
}

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

/* XcbSHM */
XcbSHM::XcbSHM(int shmid, uint8_t* addr, xcb_connection_t* conn)
{
    auto id = xcb_generate_id(conn);
    auto cookie = xcb_shm_attach_checked(conn, id, shmid, 0);

    if(auto err = GenericError(xcb_request_check(conn, cookie)))
    {
        qWarning() << err.toString("xcb_shm_attach");
        throw shm_error("xcb_shm_attach");
    }

    reset(new shm_t(shmid, addr, conn, id));
}

uint8_t* XcbSHM::data(void)
{
    return get() ? get()->_addr : nullptr;
}

const uint8_t* XcbSHM::data(void) const
{
    return get() ? get()->_addr : nullptr;
}

xcb_connection_t* XcbSHM::connection(void) const
{
    return get() ? get()->_conn : nullptr;
}

uint32_t XcbSHM::xid(void) const
{
    return get() ? get()->_xcb : 0;
}

QPair<XcbPixmapInfoReply, GenericError>
    XcbSHM::getPixmapRegion(xcb_drawable_t winid, const QRect & reg, size_t offset, uint32_t planeMask) const
{
    auto xcbReply = getReplyFunc1(xcb_shm_get_image, connection(), winid, reg.x(), reg.y(), reg.width(), reg.height(),
                                    planeMask, XCB_IMAGE_FORMAT_Z_PIXMAP, xid(), offset);
    XcbPixmapInfoReply res;

    if(auto err = xcbReply.error())
    {
        //res->setError(err);
        qWarning() << err.toString("xcb_shm_get_image");
        return QPair<XcbPixmapInfoReply, GenericError>(res, err);
    }
        
    if(auto reply = xcbReply.reply())
        res = std::make_shared<XcbPixmapInfoSHM>(reply->depth, reply->visual, *this, reply->size);

    return QPair<XcbPixmapInfoReply, GenericError>(res, nullptr);
}

/* XcbPixmapInfoSHM */
uint8_t* XcbPixmapInfoSHM::data(void)
{
    return _shm.data();
}

const uint8_t* XcbPixmapInfoSHM::data(void) const
{
    return _shm.data();
}

size_t XcbPixmapInfoSHM::size(void) const
{
    return _size;
}

/* XcbPixmapInfoBuffer */
uint8_t* XcbPixmapInfoBuffer::data(void)
{
    return _pixels.data();
}

const uint8_t* XcbPixmapInfoBuffer::data(void) const
{
    return _pixels.data();
}

size_t XcbPixmapInfoBuffer::size(void) const
{
    return _pixels.size();
}

/* XcbConnection */
XcbConnection::XcbConnection() :
    conn{ xcb_connect(":0", nullptr), xcb_disconnect }, screen(nullptr), format(nullptr)
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

    // shm
    if(initSHM())
    {
        const int bpp = format->bits_per_pixel >> 3;
        const int pagesz = 4096;
        auto winsz = getWindowSize(screen->root);
        const size_t shmsz = ((winsz.width() * winsz.height() * bpp / pagesz) + 1) * pagesz;

        try
        {
            extSHM = getSHM(shmsz);
        }
        catch(const shm_error &)
        {
            qWarning() << "shm skipped";
        }
    }
    else
    {
        qWarning() << "xcb init failed";
    }

    // xfixes
    if(! initXFIXES())
        qWarning() << "xfixes init failed";

    // event filter
    const uint32_t values[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes(conn.get(), screen->root, XCB_CW_EVENT_MASK, values);

    xcb_flush(conn.get());
}

bool XcbConnection::initSHM(void)
{
    auto shm = xcb_get_extension_data(conn.get(), &xcb_shm_id);
    if(! shm || ! shm->present)
        return false;

    auto xcbReply = getReplyFunc2(xcb_shm_query_version, conn.get());

    if(auto err = xcbReply.error())
        qWarning() << err.toString("xcb_shm_query_version");
    else
    if(xcbReply.reply())
    {
        // qWarning() << QString("shm version: %1.%2").arg((int) reply->major_version).arg((int) reply->minor_version);
        return true;
    }

    return false;
}

bool XcbConnection::initXFIXES(void)
{
    auto xfixes = xcb_get_extension_data(conn.get(), &xcb_xfixes_id);
    if(! xfixes || ! xfixes->present)
        return false;

    auto xcbReply = getReplyFunc2(xcb_xfixes_query_version, conn.get(), XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);

    if(auto err = xcbReply.error())
        qWarning() << err.toString("xcb_xfixes_query_version");
    else
    if(xcbReply.reply())
    {
        // qWarning() << QString("xfixes version: %1.%1").arg(reply->major_version).arg(reply->minor_version);
        return true;
    }

    return false;
}

XcbSHM XcbConnection::getSHM(size_t shmsz)
{
    // init shm
    int shmid = shmget(IPC_PRIVATE, shmsz, IPC_CREAT | S_IRUSR | S_IWUSR);

    if(shmid == -1)
        throw shm_error("shmget failed");

    uint8_t* shmaddr = reinterpret_cast<uint8_t*>(shmat(shmid, 0, 0));

    // man shmat: check result
    if(shmaddr == reinterpret_cast<uint8_t*>(-1))
        throw shm_error("shmat failed");

    return XcbSHM(shmid, shmaddr, conn.get());
}

QRect XcbConnection::getWindowGeometry(xcb_window_t win) const
{
    auto xcbReply = getReplyFunc2(xcb_get_geometry, conn.get(), win);

    if(auto reply = xcbReply.reply())
    {
        auto xcbReply2 = getReplyFunc2(xcb_translate_coordinates, conn.get(), win, screen->root, reply->x, reply->y);

        if(auto reply2 = xcbReply2.reply())
            return QRect(reply2->dst_x, reply2->dst_y, reply->width, reply->height);

        return QRect(reply->x, reply->y, reply->width, reply->height);
    }

    return QRect();
}

QPoint XcbConnection::getWindowPosition(xcb_window_t win) const
{
    return getWindowGeometry(win).topLeft();
}

QSize XcbConnection::getWindowSize(xcb_window_t win) const
{
    return getWindowGeometry(win).size();
}

QString XcbConnection::getAtomName(xcb_atom_t atom) const
{
    auto xcbReply = getReplyFunc2(xcb_get_atom_name, conn.get(), atom);

    if(auto reply = xcbReply.reply())
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

xcb_screen_t* XcbConnection::getScreen(void) const
{
    return screen;
}

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

    if(auto reply = xcbReply.reply())
    {
        if(auto res = static_cast<xcb_window_t*>(xcb_get_property_value(reply.get())))
            return *res;
    }

    return XCB_WINDOW_NONE;
}

XcbPropertyReply XcbConnection::getPropertyAnyType(xcb_window_t win, xcb_atom_t prop, uint32_t offset, uint32_t length) const
{
    auto xcbReply = getReplyFunc2(xcb_get_property, conn.get(), false, win, prop, XCB_GET_PROPERTY_TYPE_ANY, offset, length);

    if(auto err = xcbReply.error())
        qWarning() << err.toString("xcb_get_property");

    return xcbReply.reply();
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

QPair<XcbPixmapInfoReply, QString>
    XcbConnection::getWindowRegion(xcb_window_t win, const QRect & reg, uint32_t planeMask) const
{
    if(extSHM)
    {
        auto res = extSHM.getPixmapRegion(win, reg, 0, planeMask);
        return QPair<XcbPixmapInfoReply, QString>(res.first, res.second.toString());
    }

    QPair<XcbPixmapInfoReply, QString> res;
    int pitch = reg.width() * (format->bits_per_pixel >> 2);

    if(0 >= pitch || 0 >= reg.height())
    {
        res.second = QString("incorrect size: %1 %2").arg(reg.width()).arg(reg.height());
        qWarning() << res.second;
        return res;
    }

    XcbPixmapInfoBuffer* info = nullptr;
    uint32_t maxReqLength = xcb_get_maximum_request_length(conn.get());
    uint32_t allowRows = qMin(maxReqLength / pitch, (uint32_t) reg.height());

    for(int64_t yy = reg.y(); yy < reg.y() + reg.height(); yy += allowRows)
    {
        // last rows
        if(yy + allowRows > reg.y() + reg.height())
            allowRows = reg.y() + reg.height() - yy;

        auto xcbReply = getReplyFunc2(xcb_get_image, conn.get(), XCB_IMAGE_FORMAT_Z_PIXMAP, win, reg.x(), yy, reg.width(), allowRows, planeMask);
        if(auto err = xcbReply.error())
        {
            res.second = err.toString("xcb_get_image");
            qWarning() << res.second;
            break;
        }

        if(auto reply = xcbReply.reply())
        {
            if(! info)
                info = new XcbPixmapInfoBuffer(reply->depth, reply->visual, reg.height() * pitch);

            auto length = xcb_get_image_data_length(reply.get());
            auto data = xcb_get_image_data(reply.get());
            auto & pixels = info->pixels();

            pixels.insert(pixels.end(), data, data + length);
        }
    }

    res.first.reset(info);
    return res;
}
