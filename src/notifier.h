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

#ifndef __NOTIFIER_HPP__
#define __NOTIFIER_HPP__

#include <QFile>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QTcpSocket>

class KNotification;
class QTcpSocket;

class Notifier : public QObject {
    Q_OBJECT

public:
    Notifier();
    ~Notifier();
    void popup(const QString& _text, QString _imageFilename);

private slots:
    void slotAction(unsigned int _n);
    void slotNewData();
    void slotClosedNotification();
    void displayError(QAbstractSocket::SocketError _socketError);

private:
    QString m_password;
    // some state variables
    KNotification* m_notification;
    QTcpSocket m_soc;
    bool m_shownPopup;
    QPixmap m_pmap;
    QPixmap m_noCoverPixmap;
    QString m_lastImgFile;
    // preferences
    QString m_musicDir;
    QString m_format;
    QStringList m_commandsNames;
    QStringList m_commands;
    QString m_noCoverImg;
    QString m_artFindCmd;
    QString m_artResizeCmd;
    int m_preferredWidth, m_preferredHeight;
    QFile m_logFile;
    // values saved for substitution in commands
    QString m_lastFile;
    QString m_lastDir;
    QString m_lastArt;
    QString m_lastArtFilename;

    void clearPassword(QString& _password) const;
    void parseHostString(const QString& _hostString, QString& _host,
                         QString& _password) const;
    void showSongInfo(const QStringList& _songInfo);
    void initLogging(const QString& _logFile);
    bool loadImg(const QString& _file, QPixmap& _pixmap);
    QString applyFormat(QString _frmt, const QStringList& _l);
    QString findAlbumArt(const QString& _dirpath);
    bool shouldImageBeScaled(const QPixmap& _image);
    QString substInCmd(QString _cmd);
    void logEvent(const QString& _msg);
    void debug(const QString& _msg);
};

#endif // __NOTIFIER_HPP__
