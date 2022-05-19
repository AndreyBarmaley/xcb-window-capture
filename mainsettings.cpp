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

#include <QMap>
#include <QDir>
#include <QMenu>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QProcess>
#include <QByteArray>
#include <QFontDialog>
#include <QFileDialog>
#include <QDataStream>
#include <QTreeWidget>
#include <QInputDialog>
#include <QApplication>
#include <QColorDialog>
#include <QStandardPaths>
#include <QTreeWidgetItem>

#include <QDebug>

#include <ctime>
#include <chrono>
#include <thread>
#include <exception>

#include "xcb/xfixes.h"

#include "mainsettings.h"
#include "ui_mainsettings.h"

/* MainSettings */
MainSettings::MainSettings(QWidget* parent) :
    QWidget(parent), ui(new Ui::MainSettings), windowId(XCB_WINDOW_NONE)
{
    actionSettings = new QAction("Settings", this);
    actionStart = new QAction("Start", this);
    actionStop = new QAction("Stop", this);
    actionExit = new QAction("Exit", this);
    auto version = QString("%1 version: %2").arg(QCoreApplication::applicationName()).arg(QCoreApplication::applicationVersion());
    auto github = QString("https://github.com/AndreyBarmaley/xcb-window-capture");
    ui->setupUi(this);
    ui->tabWidget->setCurrentIndex(1);
    ui->aboutInfo->setText(QString("<center><b>%1</b></center><br><br>"
                                   "<p>Source code: <a href='%2'>%2</a></p>"
                                   "<p>Copyright © 2022 by Andrey Afletdinov <public.irkutsk@gmail.com></p>").arg(version).arg(github));

    ui->systemInfo->setText(QString("<center>FFMpeg info: avdevice-%1, avformat-%2</center>").arg(AV_STRINGIFY(LIBAVDEVICE_VERSION)).arg(AV_STRINGIFY(LIBAVFORMAT_VERSION)));

    for(auto type : { FFMPEG::H264Preset::UltraFast, FFMPEG::H264Preset::SuperFast, FFMPEG::H264Preset::VeryFast, FFMPEG::H264Preset::Faster, 
            FFMPEG::H264Preset::Fast, FFMPEG::H264Preset::Medium, FFMPEG::H264Preset::Slow, FFMPEG::H264Preset::Slower, FFMPEG::H264Preset::VerySlow })
    {
        ui->comboBoxH264Preset->addItem(FFMPEG::H264Preset::name(type), type);
    }
    ui->comboBoxH264Preset->setCurrentIndex(ui->comboBoxH264Preset->findData(FFMPEG::H264Preset::Medium));
    ui->checkBoxShowCursor->setChecked(true);
    ui->pushButtonStart->setDisabled(true);

    configLoad();

    QMenu* menu = new QMenu(this);
    menu->addAction(actionSettings);
    menu->addSeparator();
    menu->addAction(actionStart);
    menu->addAction(actionStop);
    menu->addSeparator();
    menu->addAction(actionExit);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QPixmap(QString(":/icons/streamr")));
    trayIcon->setToolTip(version);
    trayIcon->setContextMenu(menu);
    trayIcon->show();

    actionStart->setEnabled(false);
    actionStop->setEnabled(false);

    xcb.reset(new XcbConnection());

    connect(actionSettings, SIGNAL(triggered()), this, SLOT(show()));
    connect(actionStart, SIGNAL(triggered()), this, SLOT(startRecord()));
    connect(actionStop, SIGNAL(triggered()), this, SLOT(stopRecord()));
    connect(actionExit, SIGNAL(triggered()), this, SLOT(exitProgram()));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
    connect(this, SIGNAL(updatePreviewNotify(quint32)), this, SLOT(updatePreviewLabel(quint32)));
}

MainSettings::~MainSettings()
{
    delete ui;
}

void MainSettings::exitProgram(void)
{
    hide();
    close();
}

void MainSettings::showEvent(QShowEvent* event)
{
    actionSettings->setDisabled(true);
}

void MainSettings::hideEvent(QHideEvent* event)
{
    actionSettings->setEnabled(true);
}

void MainSettings::closeEvent(QCloseEvent* event)
{
    if(isVisible())
    {
        event->ignore();
        hide();
    }

    configSave();
}

void MainSettings::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
        setVisible(! isVisible());
}

void MainSettings::configSave(void)
{
    auto localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(localData);
    auto configPath = QDir(localData).absoluteFilePath("config");
    QFile file(configPath);

    if(! file.open(QIODevice::WriteOnly))
        return;

    QDataStream ds(&file);
    ds << int(VERSION);
    ds << pos();

    ds << ui->comboBoxH264Preset->currentData().toInt();
    ds << ui->lineEditBitRate->text().toInt();
    ds << ui->lineEditOutputFile->text();

    ds << ui->checkBoxShowCursor->isChecked();
}

void MainSettings::configLoad(void)
{
    auto localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(localData);
    auto configPath = QDir(localData).absoluteFilePath("config");
    QFile file(configPath);

    if(! file.open(QIODevice::ReadOnly))
        return;

    QDataStream ds(&file);
    int version;
    ds >> version;

    if(version != VERSION)
    {
        qWarning() << "unsupported version" << version;
        return;
    }

    QPoint pos;
    ds >> pos;
    move(pos);

    int h264Preset, h264BitRate;
    ds >> h264Preset >> h264BitRate;

    ui->comboBoxH264Preset->setCurrentIndex(ui->comboBoxH264Preset->findData(h264Preset));
    ui->lineEditBitRate->setText(QString::number(h264BitRate));

    QString outputPath;
    ds >> outputPath;
    ui->lineEditOutputFile->setText(outputPath);

    bool showCursor;
    ds >> showCursor;
    ui->checkBoxShowCursor->setChecked(showCursor);
}

void MainSettings::updatePreviewLabel(quint32 win)
{
    if(win != XCB_WINDOW_NONE)
    {
        auto winsz = xcb->getWindowSize(win);
        auto pair = xcb->getWindowRegion(win, QRect(0, 0, winsz.width(), winsz.height()));

        if(pair.first)
        {
            auto & reply = pair.first;
            int bytePerPixel = xcb->bppFromDepth(reply->depth()) >> 3;
            int bytesPerLine = bytePerPixel * winsz.width();

            if(reply->size() > (uint32_t) (winsz.width() * winsz.height() * bytePerPixel))
                    bytesPerLine += reply->size() / (winsz.height() * bytePerPixel) - winsz.width();

            auto width = ui->groupBoxPreview->width();
            auto image = QImage(reply->data(), winsz.width(), winsz.height(), bytesPerLine, QImage::Format_RGBX8888).scaled(width, width, Qt::KeepAspectRatio);

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            image = image.rgbSwapped();
#endif

            ui->labelPreview->setScaledContents(true);
            ui->labelPreview->setPixmap(QPixmap::fromImage(image));

            windowId = win;
            actionStart->setEnabled(true);
            ui->pushButtonStart->setEnabled(true);
        }
        else
        {
            ui->labelPreview->setText(pair.second);
        }
    }
}

void MainSettings::selectWindows(void)
{
    QMap<QString, xcb_window_t> windows;
    windows.insert("<root screen>", xcb->getScreen()->root);

    for(auto & win : xcb->getWindowList())
    {
        auto list = xcb->getPropertyStringList(win, XCB_ATOM_WM_CLASS);
        auto name = xcb->getWindowName(win);
        QString key = list.size() ? QString("%1.%2 (%3)").arg(list.front()).arg(list.back()).arg(name) : name;

        windows.insert(key, win);
    }

    if(windows.size())
    {
        bool ok = false;
        auto sel = QInputDialog::getItem(this, "Select", "Capture window", windows.keys(), 0, false, & ok);

        if(ok && sel.size())
        {
            ui->lineEditWindowDescription->setText(sel);
            emit updatePreviewNotify(windows[sel]);
        }
    }
}

bool MainSettings::startRecord(void)
{
    if(isVisible())
        hide();

    if(windowId != XCB_WINDOW_NONE)
    {
        bool error = false;

        auto h264Preset = static_cast<FFMPEG::H264Preset::type>(ui->comboBoxH264Preset->currentData().toInt());
        int bitrate = ui->lineEditBitRate->text().toInt();
        if(bitrate < 0) bitrate = 1024;

        try
        {
            auto format = ui->lineEditOutputFile->text();
            bool cursor = ui->checkBoxShowCursor->isChecked();
            bool audio = true;
            encoder.reset(new FFmpegEncoderPool(h264Preset, bitrate, windowId, xcb, format.toStdString(), cursor, audio, this));
        }
        catch(const std::runtime_error & err)
        {
            qWarning() << err.what();
            error = true;
            trayIcon->setToolTip(err.what());
        }

        if(! error)
        {
            connect(encoder.get(), SIGNAL(shutdownNotify()), this, SLOT(exitProgram()));
            connect(encoder.get(), SIGNAL(errorNotify(QString)), this, SLOT(stopRecord(QString)));
            connect(encoder.get(), SIGNAL(restartNotify()), this, SLOT(restartRecord()));
            actionStart->setEnabled(false);
            actionStop->setEnabled(true);
            trayIcon->setIcon(QPixmap(QString(":/icons/streamg")));
            trayIcon->setToolTip(QString("capture window id: %1").arg(windowId));
            encoder->start();
            return true;
        }
    }

    return false;
}

void MainSettings::restartRecord(void)
{
    stopRecord();
    startRecord();
}

void MainSettings::stopRecord(QString error)
{
    windowId = XCB_WINDOW_NONE;
    ui->lineEditWindowDescription->clear();
    ui->labelPreview->clear();
    actionStart->setDisabled(true);
    ui->pushButtonStart->setDisabled(true);

    trayIcon->setToolTip(QString("error: %1").arg(error));

    stopRecord();
}

void MainSettings::stopRecord(void)
{
    encoder.reset();

    actionStart->setEnabled(true);
    actionStop->setEnabled(false);

    trayIcon->setIcon(QPixmap(QString(":/icons/streamr")));
    auto version = QString("%1 version: %2").arg(QCoreApplication::applicationName()).arg(QCoreApplication::applicationVersion());
    trayIcon->setToolTip(version);
}

/* FFmpegEncoderPool */
FFmpegEncoderPool::FFmpegEncoderPool(const FFMPEG::H264Preset::type & preset, int bitrate, xcb_window_t win, std::shared_ptr<XcbConnection> ptr, const std::string & format, bool cursor, bool audio, QObject* obj)
    : QThread(obj), FFMPEG::H264Encoder(preset, bitrate, false), windowId(win), xcb(ptr), shutdown(false), showCursor(cursor), audioStream(audio)
{
    time_t raw;
    std::time(& raw);

    size_t len = 4096;
    outputPath.reset(new char[len]);

    struct tm* timeinfo = std::localtime(&raw);
    std::strftime(outputPath.get(), len - 1, format.c_str(), timeinfo);

    showCursor = cursor;
    audioStream = audio;
}

FFmpegEncoderPool::~FFmpegEncoderPool()
{
    shutdown = true;

    if(! wait(1000))
    {
        terminate();
        wait();
    }
}

void FFmpegEncoderPool::run(void)
{
    auto durationMS = std::chrono::milliseconds(1000/fps);
    auto point = std::chrono::steady_clock::now();
    auto winsz = xcb->getWindowSize(windowId);

    try
    {
        FFMPEG::H264Encoder::startRecord(outputPath.get(), winsz.width(), winsz.height());
    }
    catch(const std::runtime_error & err)
    {
        emit errorNotify(QString(err.what()));
        return;
    }

    // record loop
    while(true)
    {
        if(shutdown)
            break;

        if(int err = xcb_connection_has_error(xcb->connection()))
        {
            emit errorNotify(QString("xcb error code: %1").arg(err));
            emit shutdownNotify();
            break;
        }

        auto now = std::chrono::steady_clock::now();
        auto timeMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - point);

        if(timeMS >= durationMS)
        {
            point = now;

            auto winsz = xcb->getWindowSize(windowId);
            auto pair = xcb->getWindowRegion(windowId, QRect(0, 0, winsz.width(), winsz.height()));

            if(! pair.first)
            {
                emit errorNotify(QString("xcb getWindowRegion failed"));
                break;
            }

            if(winsz.width() != getFrameWidth() || winsz.height() != getFrameHeight())
            {
                qWarning() << "window size changed";
                emit restartNotify();
                break;
            }

            auto & reply = pair.first;
            int bytesPerLine = reply->size() / winsz.height();

            // sync cursor
            if(showCursor)
            {
                QImage windowImage(reply->data(), winsz.width(), winsz.height(), bytesPerLine, QImage::Format_RGBX8888);
                auto replyCursor = xcb->getReplyFunc2(xcb_xfixes_get_cursor_image, xcb->connection());

                if(auto err = replyCursor.error())
                {
                    qWarning() << err.toString("xcb_xfixes_get_cursor_image");
                }
                else
                if(auto reply = replyCursor.reply())
                {
                    auto geometry = xcb->getWindowGeometry(windowId);
                    if(geometry.contains(reply->x, reply->y))
                    {
                        uint32_t* ptr = xcb_xfixes_get_cursor_image_cursor_image(reply.get());
                        int len = xcb_xfixes_get_cursor_image_cursor_image_length(reply.get());

                        QImage cursorImage((uint8_t*) ptr, reply->width, reply->height, QImage::Format_RGBA8888);
                        QPoint cursorPosition(reply->x, reply->y);
                        QPainter painter(& windowImage);
                        painter.drawImage(cursorPosition - geometry.topLeft(), cursorImage);
                    }
                }
            }

            try
            {
                FFMPEG::H264Encoder::pushFrame(reply->data(), bytesPerLine, winsz.height());
            }
            catch(const std::runtime_error & err)
            {
                emit errorNotify(QString(err.what()));
                shutdown = true;
            }
        }
        else
        {
            std::this_thread::sleep_for(durationMS - timeMS);
        }
    }
}
