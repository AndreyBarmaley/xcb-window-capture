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

#include "mainsettings.h"

#include <QDir>
#include <QDebug>
#include <QLockFile>
#include <QApplication>
#include <QStandardPaths>

#include <exception>

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("XcbWindowCapture");
    QCoreApplication::setApplicationVersion(QString::number(VERSION));

    auto localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(localData);
    auto lockPath = QDir(localData).absoluteFilePath("lock");
    QLockFile lockFile(lockPath);

    if(! lockFile.tryLock(100))
    {
        qWarning() << "also running, see lock" << lockPath;
        return 1;
    }

    QApplication a(argc, argv);
    try
    {
        MainSettings w;
        w.show();
        return a.exec();
    }
    catch(const FFMPEG::runtimeException & err)
    {
        auto str = QString("%1 failed, code: %2, error: %3").arg(err.func).arg(err.code).arg(FFMPEG::errorString(err.code));
        qWarning() << str;
#ifdef BOOST_STACKTRACE_USE
        qWarning() << "stacktrace: " << err.trace.c_str();
#endif
    }
    catch(const std::exception & err)
    {
        qWarning() << err.what();
    }

    return -1;
}
