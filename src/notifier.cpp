//
// Copyright (C) 2009 Jakub Horák <kubahorak@gmail.com>
// Copyright (C) 2010-2011 xaizek <xaizek@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the
// Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
//

#include <KApplication>
#include <KConfig>
#include <KConfigGroup>
#include <KNotification>

#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QPixmapCache>
#include <QRegExp>

#include "main.h"

#include "notifier.h"

Notifier::Notifier() : QObject()
{
    KConfig config("mpdknotifierrc");
    KConfigGroup generalGroup(&config, "General");
    QString mpdHost = generalGroup.readEntry("MPDHost", "localhost");
    QString mpdPort = generalGroup.readEntry("MPDPort", "6600");

    m_musicDir = generalGroup.readEntry("MusicDir", "/mnt/music/");
    if (!m_musicDir.endsWith("/")) {
        m_musicDir += "/";
    }

    m_format = generalGroup.readEntry("Format",
                                      "%Artist% - %Title%{\n"
                                      "%Album%} {(%Date%)}");
    m_commandsNames = generalGroup.readEntry("CommandsNames", "Open ncmpcpp")
                      .split(';');
    m_commands = generalGroup.readEntry("Commands", "terminal -x ncmpcpp")
                 .split(';');

    m_noCoverImg = generalGroup.readEntry("NoCoverImg",
            "/usr/share/apps/mpdknotifier/nocover.jpg");
    m_artFindCmd = generalGroup.readEntry("ArtFindCmd", "");
    m_artResizeCmd = generalGroup.readEntry("ArtResizeCmd", "");
    m_preferredWidth = generalGroup.readEntry("PreferredCoverWidth", "200")
                       .toInt();
    m_preferredHeight = generalGroup.readEntry("PreferredCoverHeight", "200")
                        .toInt();

    QString logFile = generalGroup.readEntry("LogFile",
                                             "~/.mpdknotifier/log.txt");
    initLogging(logFile);
    m_soc.connectToHost(mpdHost, mpdPort.toUInt());
    connect(&m_soc, SIGNAL(readyRead()), this, SLOT(slotNewData()));
    connect(&m_soc, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(displayError(QAbstractSocket::SocketError)));

    m_shownPopup = false;
    m_notification = 0;
}

void Notifier::initLogging(const QString& _logFile)
{
    // handle ~
    QString fullPath;
    if (_logFile.startsWith("~/")) {
        fullPath = QDir::homePath() + _logFile.right(_logFile.length() - 1);
    } else {
        fullPath = _logFile;
    }

    // make sure that all parent directories exist
    QString dir(fullPath);
    if (int i = dir.lastIndexOf('/')) {
        dir = dir.left(i);
    }
    QDir::current().mkpath(dir);

    m_logFile.setFileName(fullPath);
    m_logFile.open(QIODevice::Append | QIODevice::Text | QIODevice::Unbuffered);
}

Notifier::~Notifier()
{
    m_soc.close();
    m_logFile.close();
}

void Notifier::slotNewData()
{
    QString response = QString::fromUtf8(m_soc.readAll());

    // previous command succeed
    if (response.startsWith("OK")) {
        debug("'OK' response");
        // ask for notification on any changes in 'player' subsystem
        m_soc.write(QByteArray("idle player\n"));
    } else if (response.startsWith("changed")) {
        // there are some changes in subsystem
        debug("'changed' response");
        // ask for current state of playback
        m_soc.write(QByteArray("status\n"));
    } else if (response.startsWith("volume")) {
        debug("status response");
        QStringList l = response.split("\n");
        for (int i = 0; i < l.size(); ++i) {
            if (!l.at(i).startsWith("state: ")) {
                continue;
            } else if (l.at(i) == "state: play") {
                debug("new state: play");
                // ask for current song information
                m_soc.write(QByteArray("currentsong\n"));
            } else {
                debug("new state: pause or stop");
                // continue waiting
                m_soc.write(QByteArray("idle\n"));
            }
            break;
        }
    } else if (response.startsWith("file")) {
        debug("currentsong response");
        showSongInfo(response.split("\n"));
        m_soc.write(QByteArray("idle player\n"));
    }
}

void Notifier::showSongInfo(const QStringList& _songInfo)
{
    QString file, aart;
    file = _songInfo.at(0);
    file.remove(0, 6);

    // save for using as '%f' during format substitution
    m_lastFile = m_musicDir + file;

    file = m_musicDir + file.left(file.lastIndexOf("/") + 1);

    // save for using as '%d'
    m_lastDir = file;

    debug("dir: '" + file + "'");

    aart = findAlbumArt(file);
    if (aart == "" && m_artFindCmd != "") {
        debug("aart: '" + aart + "'");
        debug("advanced search for album art");
        system(substInCmd(m_artFindCmd).toLatin1());
        aart = findAlbumArt(file);
    }
    debug("aart: '" + aart + "'");

    // save for using as '%a'
    m_lastArt = aart;
    m_lastArtFilename = m_lastArt;
    if (int i = m_lastArtFilename.lastIndexOf('/')) {
        m_lastArtFilename = m_lastArtFilename.remove(0, i + 1);
    }
    popup(applyFormat(m_format, _songInfo), aart);
}

void Notifier::popup(const QString& _text, QString _imageFilename)
{
    if (m_shownPopup) {
        m_notification->close();
    }
    m_notification = new KNotification("message");

    debug("try pop-up with art: '" + _imageFilename + "'");
    debug("previous pop-up art: '" + m_lastImgFile + "'");
    if (_imageFilename != "" && _imageFilename != m_lastImgFile) {
        debug("try to load album art");
        if (loadImg(_imageFilename, m_pmap)) {
            m_lastImgFile = _imageFilename;
        } else {
            _imageFilename = "";
        }
    }

    m_notification->setActions(m_commandsNames);
    m_notification->setFlags(KNotification::CloseOnTimeout);
    m_notification->setText(_text);
    if (_imageFilename != "") {
        m_notification->setPixmap(m_pmap);
    } else if (m_noCoverImg != "") {
        if (m_noCoverPixmap.isNull()) {
            debug("loading no cover art");
            loadImg(m_noCoverImg, m_noCoverPixmap);
        }
        debug("using no cover art");
        m_notification->setPixmap(m_noCoverPixmap);
    }

    connect(m_notification, SIGNAL(activated(unsigned int)),
            this, SLOT(slotAction(unsigned int)));
    connect(m_notification, SIGNAL(closed()),
            this, SLOT(slotClosedNotification()));
    connect(m_notification, SIGNAL(ignored()),
            this, SLOT(slotClosedNotification()));
    m_notification->sendEvent();
    m_shownPopup = true;
}

void Notifier::slotClosedNotification()
{
    m_shownPopup = false;
}

void Notifier::slotAction(unsigned int _n)
{
    m_shownPopup = false;
    if (_n - 1 < (unsigned int)m_commands.size()) {
        QString cmd = substInCmd(m_commands[_n - 1]);
        debug("Executing command: '" + cmd + "'");
        system(cmd.toLocal8Bit());
    } else {
        debug(QString("Invalid cmdnum: %1 of %2")
              .arg(_n)
              .arg(m_commands.size()));
    }
}

void Notifier::displayError(QAbstractSocket::SocketError _socketError)
{
    switch (_socketError) {
        case QAbstractSocket::RemoteHostClosedError:
            break;
        case QAbstractSocket::HostNotFoundError:
            qCritical() << tr("The host wasn't found. Please check the "
                              "host name and port settings.");
            break;
        case QAbstractSocket::ConnectionRefusedError:
            qCritical() << tr("The connection was refused by the peer. "
                              "Make sure the MPD server is running, "
                              "and check that the host name and port "
                              "settings are correct.");
            break;

        default:
            qCritical() << tr("The following error occurred: %1.")
                           .arg(m_soc.errorString());
            break;
    }
}

bool Notifier::loadImg(const QString& _file, QPixmap& _pixmap)
{
    debug(QString("try to load image \"%1\"").arg(_file));
    if (!_pixmap.load(_file, "JPG")
            && !_pixmap.load(_file, "PNG")
            && !_pixmap.load(_file, "GIF")) {
        // TODO: someday figure out where is bug in Qt QPixmapCache with
        // generating key for implicit caching
        // TODO: maybe add an option to clear cache
        //QPixmapCache::clear();
        debug("image load failed");
        logEvent(tr("Image file is not JPG, PNG or GIF."));
        return (false);
    }
    if (shouldImageBeScaled(_pixmap)) {
        _pixmap = _pixmap.scaled(m_preferredWidth, m_preferredHeight);
    }
    return (true);
}

QString Notifier::applyFormat(QString _frmt, const QStringList& _l)
{
    int i = _frmt.lastIndexOf("%");
    int j = _frmt.lastIndexOf("%", i - 1);
    while (i >= 0 && j >= 0) {
        QString tag = _frmt.toStdString().substr(j + 1, i - (j + 1)).c_str();
        for (int k = 2; k < _l.size(); ++k) {
            if (_l.at(k).startsWith(tag + ": ")) {
                QString tmp = _l.at(k);
                _frmt.replace(j, tag.size() + 2,
                             tmp.remove(0, tag.size() + 2));
                break;
            }
        }
        if (j == 0) {
            break;
        }
        i = _frmt.lastIndexOf("%", j - 1);
        j = _frmt.lastIndexOf("%", i - 1);
    }

    QString tmp;
    do {
        tmp = _frmt;
        // remove conditional block with unexpanded tags
        _frmt = _frmt.replace(QRegExp("\\{([^{}]*%){2,}[^{}]*\\}"), "");
        // remove conditional block braces without unexpanded tags
        _frmt = _frmt.replace(QRegExp("\\{([^%}]*)\\}"), "\\1");
    } while (_frmt != tmp);
    debug("text message: " + _frmt);

    // substitute ampersand
    _frmt.replace("&", "&amp;");

    return (_frmt);
}

QString Notifier::findAlbumArt(const QString& _dirpath)
{
    // search for any images in given directory
    QStringList filters;
    filters << "*.jpg" << "*.png" << "*.gif";
    QDir dir(_dirpath);
    dir.setNameFilters(filters);
    QFileInfoList allpics = dir.entryInfoList();
    if (allpics.size() == 0) {
        return ("");
    }
    if (allpics.size() == 1) {
        return (allpics.first().absoluteFilePath());
    }

    // search using strict patterns
    filters.clear();
    filters << "[fF]ront.jpg" << "[fF]ront.png";
    filters << "[cC]over.jpg" << "[cC]over.png";
    dir.setNameFilters(filters);
    QFileInfoList filteredpics = dir.entryInfoList();
    if (filteredpics.size() > 0) {
        debug("found strict front cover");
        return (filteredpics.first().absoluteFilePath());
    }

    // search using weak patterns
    filters.clear();
    filters << "*[fF]ront*.jpg" << "*[fF]ront*.png";
    filters << "*[cC]over*.jpg" << "*[cC]over*.png";
    dir.setNameFilters(filters);
    filteredpics = dir.entryInfoList();
    if (filteredpics.size() > 0) {
        debug("found weak front cover");
        return (filteredpics.first().absoluteFilePath());
    }

    // use any of images we have initially found
    debug("found some picture");
    return (allpics.first().absoluteFilePath());
}

bool Notifier::shouldImageBeScaled(const QPixmap& _image)
{
    if (_image.width() <= m_preferredWidth
            && _image.height() <= m_preferredHeight) {
        return (false);
    }

    logEvent(tr("Album art is bigger than needed, its size is %1 p x %2 "
                "when %3 p x %4 p is preferred")
             .arg(_image.width())
             .arg(_image.height())
             .arg(m_preferredWidth)
             .arg(m_preferredHeight));

    QString cmd = substInCmd(m_artResizeCmd);
    debug("Command for scaling image: '" + cmd + "'");
    system(cmd.toLocal8Bit());
    return (true);
}

QString Notifier::substInCmd(QString _cmd)
{
    // going backwards
    int i = _cmd.lastIndexOf("%");
    while (i >= 0) {
        if (i + 1 < _cmd.length()) {
            switch (_cmd[i + 1].toAscii()) {
                case '%':
                    _cmd.replace(i, 2, "%");
                    break;
                case 'f':
                    _cmd.replace(i, 2, m_lastFile);
                    break;
                case 'd':
                    _cmd.replace(i, 2, m_lastDir);
                    break;
                case 'a':
                    _cmd.replace(i, 2, m_lastArt);
                    break;
                case 'c':
                    _cmd.replace(i, 2, m_lastArtFilename);
                    break;
            }
        }
        if (i > 0) {
            i = _cmd.lastIndexOf("%", i - 1);
        }
    }
    return (_cmd);
}

void Notifier::logEvent(const QString& _msg)
{
    m_logFile.write((QDateTime::currentDateTime().toString()
                     + "\n").toLocal8Bit());
    m_logFile.write((tr("Message:   %1\n").arg(_msg)).toLocal8Bit());
    m_logFile.write((tr("Directory: %1\n").arg(m_lastDir)).toLocal8Bit());
    m_logFile.write((tr("File:      %1\n").arg(m_lastFile)).toLocal8Bit());
    m_logFile.write((tr("Art file:  %1\n").arg(m_lastArt)).toLocal8Bit());
    m_logFile.write("\n");
}

void Notifier::debug(const QString& _msg)
{
    if (g_debugMode) {
        qDebug("%s", _msg.toAscii().constData());
    }
}
