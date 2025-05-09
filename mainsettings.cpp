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
#include <QRegExp>
#include <QPainter>
#include <QProcess>
#include <QKeyEvent>
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
    QWidget(parent), ui(new Ui::MainSettings)
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

    ui->systemInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->systemInfo->setText(QString("<center>FFMpeg info: avdevice-%1, avformat-%2</center>").arg(AV_STRINGIFY(LIBAVDEVICE_VERSION)).arg(AV_STRINGIFY(LIBAVFORMAT_VERSION)));

    for(auto type : { FFMPEG::H264Preset::UltraFast, FFMPEG::H264Preset::SuperFast, FFMPEG::H264Preset::VeryFast, FFMPEG::H264Preset::Faster, 
            FFMPEG::H264Preset::Fast, FFMPEG::H264Preset::Medium, FFMPEG::H264Preset::Slow, FFMPEG::H264Preset::Slower, FFMPEG::H264Preset::VerySlow })
    {
        ui->comboBoxH264Preset->addItem(FFMPEG::H264Preset::name(type), type);
    }
    ui->comboBoxH264Preset->setCurrentIndex(ui->comboBoxH264Preset->findData(FFMPEG::H264Preset::Medium));
    ui->pushButtonStart->setDisabled(true);
    ui->checkBoxShowCursor->setChecked(true);

    ui->checkBoxUseComposite->setChecked(true);
    ui->checkBoxRemoveWinDecor->setChecked(true);

    ui->lineEditRegion->setDisabled(true);
    ui->lineEditRegion->setValidator(new QRegExpValidator(QRegExp("(\\d{1,4})x(\\d{1,4})\\+(\\d{1,4})\\+(\\d{1,4})")));

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

    if(! xcb->getXfixesExtension())
    {
        ui->checkBoxShowCursor->setChecked(false);
        ui->checkBoxShowCursor->setDisabled(true);
        ui->checkBoxShowCursor->setToolTip("xcb-xfixes not found");
    }
    else
    {
        ui->checkBoxShowCursor->setToolTip("xcb-xfixes used");
    }

    if(! xcb->getCompositeExtension())
    {
        ui->checkBoxUseComposite->setChecked(false);
        ui->checkBoxUseComposite->setDisabled(true);
        ui->checkBoxUseComposite->setToolTip("xcb-composite not found");
    }
    else
    {
        ui->checkBoxUseComposite->setToolTip("xcb-composite used");
    }

    connect(actionSettings, SIGNAL(triggered()), this, SLOT(show()));
    connect(actionStart, SIGNAL(triggered()), this, SLOT(startRecord()));
    connect(actionStop, SIGNAL(triggered()), this, SLOT(stopRecord()));
    connect(actionExit, SIGNAL(triggered()), this, SLOT(exitProgram()));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
    connect(this, SIGNAL(updatePreviewNotify(quint32)), this, SLOT(updatePreviewLabel(quint32)));

/*
    connect(ui->checkBoxUseComposite, & QCheckBox::stateChanged,
        [=](int state)
        {
        });
*/
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

void MainSettings::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Escape)
    {
        if(isVisible()) hide();
    }
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
    else
    {
        configSave();
    }
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
    ds << ui->lineEditVideoBitrate->text().toInt();
    ds << ui->lineEditOutputFile->text();

    ds << ui->checkBoxShowCursor->isChecked();

    // 20220525
    ds << ui->checkBoxFocused->isChecked();
    ds << ui->lineEditAudioBitrate->text().toInt();
    ds << ui->comboBoxAudioPlugin->currentIndex();

    // 20250316
    ds << ui->checkBoxRemoveWinDecor->isChecked();
    ds << ui->checkBoxUseComposite->isChecked();
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

    if(0 >= version || version > VERSION)
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
    ui->lineEditVideoBitrate->setText(QString::number(h264BitRate));

    QString outputPath;
    ds >> outputPath;
    ui->lineEditOutputFile->setText(outputPath);

    bool showCursor;
    ds >> showCursor;
    ui->checkBoxShowCursor->setChecked(showCursor);

    if(20220524 < version)
    {
        bool renderFocused;
        ds >> renderFocused;

        ui->checkBoxFocused->setChecked(renderFocused);

        int audioBitrate;
        ds >> audioBitrate;
        ui->lineEditAudioBitrate->setText(QString::number(audioBitrate));

        int audioPlugin;
        ds >> audioPlugin;
        ui->comboBoxAudioPlugin->setCurrentIndex(audioPlugin);
    }

    if(20250315 < version)
    {
        bool removeDecor;
        ds >> removeDecor;
        ui->checkBoxRemoveWinDecor->setChecked(removeDecor);

        bool useComposite;
        ds >> useComposite;
        ui->checkBoxUseComposite->setChecked(useComposite);
    }
}

void MainSettings::updatePreviewLabel(quint32 win)
{
    if(win != XCB_WINDOW_NONE)
    {
        QString errstr;

        auto winsz = xcb->getWindowSize(win);
        auto reply = xcb->getWindowRegion(win, QRect(QPoint(0, 0), winsz), & errstr);

        if(reply)
        {
            int bytePerPixel = xcb->bppFromDepth(reply->pixmapDepth()) >> 3;
            int bytesPerLine = bytePerPixel * winsz.width();

            if(reply->pixmapSize() > (uint32_t) (winsz.width() * winsz.height() * bytePerPixel))
                    bytesPerLine += reply->pixmapSize() / (winsz.height() * bytePerPixel) - winsz.width();

            auto width = ui->groupBoxPreview->width();
            auto image = QImage(reply->pixmapData(), winsz.width(), winsz.height(), bytesPerLine, QImage::Format_RGBX8888).scaled(width, width, Qt::KeepAspectRatio);

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            image = image.rgbSwapped();
#endif

            ui->labelPreview->setScaledContents(true);
            ui->labelPreview->setPixmap(QPixmap::fromImage(image));

            ui->lineEditRegion->setDisabled(false);
            ui->lineEditRegion->setText(QString("%1x%2+%3+%4").arg(winsz.width()).arg(winsz.height()).arg(0).arg(0));

            windowId = win;
            actionStart->setEnabled(true);
            ui->pushButtonStart->setEnabled(true);
        }
        else
        {
            ui->labelPreview->setText(errstr);
        }
    }
}

void MainSettings::selectWindows(void)
{
    QMap<QString, xcb_window_t> windows;
    QString rootScreen("<root screen>");

    windows.insert(rootScreen, xcb->getScreenRoot());

    for(auto win : xcb->getWindowList())
    {
        auto parent = xcb->getWindowParent(win);
        auto list = xcb->getPropertyStringList(win, XCB_ATOM_WM_CLASS);
        auto name = xcb->getWindowName(win);
        QString key = list.size() ? QString("0x%1 %2.%3 (%4)").arg(win, 0, 16).arg(list.front()).arg(list.back()).arg(name) : name;

        windows.insert(key, ui->checkBoxRemoveWinDecor->isChecked() ? win : parent);
    }

    if(windows.size())
    {
        bool ok = false;
        auto sel = QInputDialog::getItem(this, "Select", "Capture window", windows.keys(), 0, false, & ok);

        if(ok && sel.size())
        {
            if(sel == rootScreen)
            {
                ui->checkBoxFocused->setChecked(false);
                ui->checkBoxFocused->setDisabled(true);
            }
            else
            {
                ui->checkBoxFocused->setDisabled(false);
            }

            ui->lineEditWindowDescription->setText(sel);
            emit updatePreviewNotify(windows[sel]);
        }
    }
}

void MainSettings::pushButton(void)
{
    bool started = ! ui->tabWidget->isEnabled();

    if(started)
        stopRecord();
    else
        startRecord();
}

bool MainSettings::startRecord(void)
{
    if(windowId == XCB_WINDOW_NONE)
    {
        if(isHidden())
            show();
    }
    else
    {
        if(isVisible())
            hide();

        bool error = false;
        QRect prefRegion;

        if(auto val = static_cast<const QRegExpValidator*>(ui->lineEditRegion->validator()))
        {
            const QRegExp & rx = val->regExp();
            if(0 == rx.indexIn(ui->lineEditRegion->text()))
            {
                prefRegion.setX(rx.cap(3).toInt());
                prefRegion.setY(rx.cap(4).toInt());
                prefRegion.setWidth(rx.cap(1).toInt());
                prefRegion.setHeight(rx.cap(2).toInt());

            }
            else
            {
                qWarning() << "incorrect region pattern:" << ui->lineEditRegion->text();
            }
        }

        auto composite = ui->checkBoxUseComposite->isChecked() ?
                xcb->getCompositeExtension() : nullptr;

        if(composite)
        {
            if(composite->redirectWindow(xcb->connection(), windowId))
            {
                if(! composite->redirectSubWindows(xcb->connection(), windowId))
                {
                    qWarning() << "composite redirect window failed";
                }

                compositeId = composite->nameWindowPixmap(xcb->connection(), windowId);
            }
            else
            {
                qWarning() << "composite redirect window failed";
            }
        }

        // check preffered region
        auto winsz = xcb->getWindowSize(windowId);
        auto realRegion = QRect(QPoint(0, 0), winsz);
        if(! realRegion.contains(prefRegion))
        {
            qWarning() << "region reset";
            ui->lineEditRegion->setText(QString("%1x%2+%3+%4").arg(winsz.width()).arg(winsz.height()).arg(0).arg(0));
            prefRegion.setX(0);
            prefRegion.setY(0);
            prefRegion.setWidth(winsz.width());
            prefRegion.setHeight(winsz.height());
        }

        auto h264Preset = static_cast<FFMPEG::H264Preset::type>(ui->comboBoxH264Preset->currentData().toInt());
        int videoBitrate = ui->lineEditVideoBitrate->text().toInt();
        if(videoBitrate < 0) videoBitrate = 1024;
        int audioBitrate = ui->lineEditAudioBitrate->text().toInt();
        if(audioBitrate < 0) audioBitrate = 64;

        auto fileFormat = ui->lineEditOutputFile->text();
        bool renderCursor = ui->checkBoxShowCursor->isChecked();
        bool startFocused = ui->checkBoxFocused->isChecked();

        AudioPlugin audioPlugin = AudioPlugin::None;
        if(ui->comboBoxAudioPlugin->currentText() == "default sink")
            audioPlugin = AudioPlugin::PulseAudioSink;
        else
        if(ui->comboBoxAudioPlugin->currentText() == "default source")
            audioPlugin = AudioPlugin::PulseAudioSource;

        if(startFocused)
            trayIcon->setIcon(QPixmap(QString(":/icons/streamb")));

        try
        {
            encoder.reset(new FFmpegEncoderPool(h264Preset, videoBitrate, windowId, compositeId, prefRegion, xcb, fileFormat.toStdString(), renderCursor, startFocused, audioPlugin, audioBitrate, this));
        }
        catch(const FFMPEG::runtimeException & err)
        {
            auto str = QString("%1 failed, code: %2, error: %3").arg(err.func).arg(err.code).arg(FFMPEG::errorString(err.code));
            qWarning() << str;
#ifdef BOOST_STACKTRACE_USE
            qWarning() << "stacktrace: " << err.trace.c_str();
#endif
            error = true;
            trayIcon->setToolTip(str);
        }
        catch(const std::runtime_error & err)
        {
            qWarning() << err.what();
            error = true;
            trayIcon->setToolTip(err.what());
        }

        if(! error)
        {
            connect(encoder.get(), SIGNAL(startedNotify(quint32)), this, SLOT(startedRecord(quint32)));
            connect(encoder.get(), SIGNAL(shutdownNotify()), this, SLOT(exitProgram()));
            connect(encoder.get(), SIGNAL(errorNotify(QString)), this, SLOT(stopRecord(QString)));
            connect(encoder.get(), SIGNAL(restartNotify()), this, SLOT(restartRecord()));
            encoder->start();
            return true;
        }

    }

    return false;
}

void MainSettings::startedRecord(quint32 wid)
{
    trayIcon->setIcon(QPixmap(QString(":/icons/streamg")));
    trayIcon->setToolTip(QString("capture window id: %1").arg(wid));

    ui->pushButtonStart->setText("Stop");
    ui->tabWidget->setDisabled(true);

    actionStart->setEnabled(false);
    actionStop->setEnabled(true);
}

void MainSettings::restartRecord(void)
{
    stopRecord();
    startRecord();
}

void MainSettings::stopRecord(QString error)
{
    if(compositeId != XCB_PIXMAP_NONE)
    {
        if(auto composite = xcb->getCompositeExtension())
        {
            composite->unredirectSubWindows(xcb->connection(), windowId);
            composite->unredirectWindow(xcb->connection(), windowId);
        }

        xcb_free_pixmap(xcb->connection(), compositeId);
        compositeId = XCB_PIXMAP_NONE;
    }

    windowId = XCB_WINDOW_NONE;

    ui->lineEditWindowDescription->clear();
    ui->labelPreview->clear();
    ui->lineEditRegion->clear();

    //ui->lineEditRegion->setDisabled(true);
    //actionStart->setDisabled(true);
    //ui->pushButtonStart->setDisabled(true);

    stopRecord();

    trayIcon->setToolTip(QString("error: %1").arg(error));
}

void MainSettings::stopRecord(void)
{
    encoder.reset();

    ui->pushButtonStart->setText("Start");
    ui->tabWidget->setDisabled(false);

    actionStart->setEnabled(true);
    actionStop->setEnabled(false);

    auto version = QString("%1 version: %2").arg(QCoreApplication::applicationName()).arg(QCoreApplication::applicationVersion());

    trayIcon->setIcon(QPixmap(QString(":/icons/streamr")));
    trayIcon->setToolTip(version);
}

/* FFmpegEncoderPool */
FFmpegEncoderPool::FFmpegEncoderPool(const FFMPEG::H264Preset::type & preset, int vbitrate, xcb_window_t win, xcb_window_t composite, const QRect & region,
    std::shared_ptr<XcbConnection> ptr, const std::string & format, bool cursor, bool focused, const AudioPlugin & audioPlugin, int audioBitrate, QObject* obj)
    : QThread(obj), FFMPEG::H264Encoder(preset, vbitrate, audioPlugin, audioBitrate), windowId(win), compositeId(composite), windowRegion(region), xcb(ptr), shutdown(false), showCursor(cursor), startFocused(focused)
{
    time_t raw;
    std::time(& raw);

    size_t len = 4096;
    outputPath.reset(new char[len]);

    struct tm* timeinfo = std::localtime(&raw);
    std::strftime(outputPath.get(), len - 1, format.c_str(), timeinfo);
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
    auto durationMS = std::chrono::milliseconds(1000 / video.fps);
    auto now = std::chrono::steady_clock::now();
    auto point = now;

    if(startFocused)
    {
        if(windowId != xcb->getActiveWindow())
        {
            qWarning() << "wait active window id: " << windowId;
        }

        while(true)
        {
            if(windowId == xcb->getActiveWindow())
                break;

            now = std::chrono::steady_clock::now();
            auto timeSec = std::chrono::duration_cast<std::chrono::seconds>(now - point);

            if(timeSec >= std::chrono::seconds(10))
            {
                emit errorNotify("active window timeout");
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    try
    {
        FFMPEG::H264Encoder::startRecord(outputPath.get(), windowRegion.width(), windowRegion.height());
    }
    catch(const FFMPEG::runtimeException & err)
    {
        auto str = QString("%1 failed, code: %2, error: %3").arg(err.func).arg(err.code).arg(FFMPEG::errorString(err.code));
        qWarning() << str;
#ifdef BOOST_STACKTRACE_USE
        qWarning() << "stacktrace: " << err.trace.c_str();
#endif
        emit errorNotify(str);
    }
    catch(const std::runtime_error & err)
    {
        qWarning() << err.what();
        emit errorNotify(err.what());
        return;
    }

    emit startedNotify(windowId);

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

        if(windowId != xcb->getScreenRoot())
        {
            // window closed
            if(! xcb->getWindowList().contains(windowId))
            {
                emit shutdownNotify();
                break;
            }

            // not active, paused
            if(startFocused && windowId != xcb->getActiveWindow())
                continue;
        }

        now = std::chrono::steady_clock::now();
        auto timeMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - point);

        if(timeMS >= durationMS)
        {
            point = now;

            if(windowId != xcb->getScreenRoot())
            {
                // check window size changed
                auto currentRegion = QRect(QPoint(0, 0), xcb->getWindowSize(windowId));
                if(! currentRegion.contains(windowRegion))
                {
                    qWarning() << "window size changed";
                    emit restartNotify();
                    break;
                }
            }

            auto reply = xcb->getWindowRegion(compositeId ? compositeId : windowId, windowRegion);
            if(! reply)
            {
                emit errorNotify("xcb getWindowRegion failed");
                break;
            }

            if(! reply->pixmapData() || 0 == reply->pixmapSize())
            {
                emit errorNotify("empty image data");
                break;
            }

            int bytesPerLine = reply->pixmapSize() / windowRegion.height();
            auto xfixes = xcb->getXfixesExtension();

            // sync cursor
            if(showCursor && xfixes)
            {
                QImage windowImage(reply->pixmapData(), windowRegion.width(), windowRegion.height(), bytesPerLine, QImage::Format_RGBX8888);

                if(auto cursorReply = xfixes->getCursorImageReply(xcb->connection()))
                {
                    auto absRegion = QRect(xcb->getWindowPosition(windowId) + windowRegion.topLeft(), windowRegion.size());

                    if(absRegion.contains(QRect(cursorReply->x, cursorReply->y, cursorReply->width, cursorReply->height)))
                    {
                        uint32_t* ptr = xfixes->getCursorImageData(cursorReply);
                        size_t len = xfixes->getCursorImageLength(cursorReply);

                        if(ptr && 0 < len)
                        {
                            auto winFrame = windowId != xcb->getScreenRoot() ? xcb->getWindowFrame(windowId) : WinFrameSize{0,0,0,0};
                            QImage cursorImage((uint8_t*) ptr, cursorReply->width, cursorReply->height, QImage::Format_RGBA8888);
                            QPoint cursorPosition(cursorReply->x + winFrame.left, cursorReply->y + winFrame.top);
                            QPainter painter(& windowImage);
                            painter.drawImage(cursorPosition - absRegion.topLeft(), cursorImage);
                        }
                    }
                }
            }

            try
            {
                encodeFrame(reply->pixmapData(), bytesPerLine, windowRegion.height());
            }
            catch(const FFMPEG::runtimeException & err)
            {
                auto str = QString("%1 failed, code: %2, error: %3").arg(err.func).arg(err.code).arg(FFMPEG::errorString(err.code));
                qWarning() << str;
#ifdef BOOST_STACKTRACE_USE
                qWarning() << "stacktrace: " << err.trace.c_str();
#endif
                emit errorNotify(str);
                shutdown = true;
            }
            catch(const std::runtime_error & err)
            {
                qWarning() << err.what();
                emit errorNotify(err.what());
                shutdown = true;
            }
        }
        else
        {
            std::this_thread::sleep_for(durationMS - timeMS);
        }
    }
}
