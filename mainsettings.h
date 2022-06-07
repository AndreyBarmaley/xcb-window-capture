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

#ifndef MAIN_SETTINGS_H
#define MAIN_SETTINGS_H

#define VERSION 20220607

#include <QList>
#include <QObject>
#include <QThread>
#include <QWidget>
#include <QAction>
#include <QString>
#include <QStringList>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QTreeWidgetItem>

#include <atomic>

#include "ffmpegencoder.h"
#include "xcbwrapper.h"

namespace Ui
{
    class MainSettings;
}

/// FFmpegEncoderPool
class FFmpegEncoderPool : public QThread, public FFMPEG::H264Encoder
{
    Q_OBJECT

    xcb_window_t windowId;
    QRect windowRegion;
    std::shared_ptr<XcbConnection> xcb;
    std::atomic<bool> shutdown;
    std::unique_ptr<char[]> outputPath;
    bool showCursor;
    bool startFocused;

public:
    FFmpegEncoderPool(const FFMPEG::H264Preset::type &, int bitrate, xcb_window_t win, const QRect &,
            std::shared_ptr<XcbConnection>, const std::string &, bool, bool, const AudioPlugin &, int, QObject*);
    ~FFmpegEncoderPool();

protected:
    void run(void) override;

signals:
    void startedNotify(quint32);
    void restartNotify(void);
    void shutdownNotify(void);
    void errorNotify(QString);
};

/// MainSettings
class MainSettings : public QWidget
{
    Q_OBJECT

    std::shared_ptr<XcbConnection> xcb;
    std::unique_ptr<PulseAudio::Context> pulse;
    std::unique_ptr<FFmpegEncoderPool> encoder;

    Ui::MainSettings* ui;
    QSystemTrayIcon* trayIcon;
    QAction* actionSettings;
    QAction* actionStart;
    QAction* actionStop;
    QAction* actionExit;
    QString windowClass;
    xcb_window_t windowId;

public:
    explicit MainSettings(QWidget* parent = 0);
    ~MainSettings();

protected:
    void keyPressEvent(QKeyEvent*) override;
    void closeEvent(QCloseEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;
    void configSave(void);
    void configLoad(void);

private slots:
    void selectWindows(void);
    void pushButton(void);
    bool startRecord(void);
    void startedRecord(quint32);
    void stopRecord(void);
    void stopRecord(QString);
    void restartRecord(void);
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void exitProgram(void);
    void updatePreviewLabel(quint32);
    void setRemoveWinDecoration(bool);

signals:
    void updatePreviewNotify(quint32);
};

#endif // MAIN_SETTINGS_H
