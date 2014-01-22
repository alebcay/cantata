/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PLAYQUEUEMODEL_H
#define PLAYQUEUEMODEL_H

#include <QtCore/QAbstractTableModel>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QSet>
#include "song.h"
#include "mpdstatus.h"
#include "config.h"

class StreamFetcher;

class PlayQueueModel : public QAbstractTableModel
{
    Q_OBJECT

public:

    enum Columns
    {
        COL_STATUS,
        COL_TRACK,
        COL_DISC,
        COL_TITLE,
        COL_ARTIST,
        COL_ALBUM,
        COL_LENGTH,
        COL_YEAR,
        COL_GENRE,

        COL_COUNT
    };

    static const QLatin1String constMoveMimeType;
    static const QLatin1String constFileNameMimeType;
    static const QLatin1String constUriMimeType;

    static void encode(QMimeData &mimeData, const QString &mime, const QStringList &values);
    static QStringList decode(const QMimeData &mimeData, const QString &mime);
    static QString headerText(int col);

    PlayQueueModel(QObject *parent = 0);
    ~PlayQueueModel();
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const;
    QVariant data(const QModelIndex &, int) const;
    void updateCurrentSong(quint32 id);
    qint32 getIdByRow(qint32 row) const;
    qint32 getPosByRow(qint32 row) const;
    qint32 getRowById(qint32 id) const;
    Song getSongByRow(const qint32 row) const;
    Qt::DropActions supportedDropActions() const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    QSet<qint32> getSongIdSet();
    void clear();
    qint32 currentSong() const { return currentSongId; }
    qint32 currentSongRow() const { return getRowById(currentSongId); }
    void setState(MPDStatus::State st);
    bool isGrouped() const { return grouped; }
    void setGrouped(bool g);
    void setDropAdjust(quint32 a) { dropAdjust=a; }
    QSet<quint16> updatePlaylist(const QList<Song> &songList, QSet<qint32> controlledIds);

public Q_SLOTS:
    void addItems(const QStringList &items, int row);
    void addItems(const QStringList &items) { addItems(items, -1); }
    void addFiles(const QStringList &filenames, int row);

Q_SIGNALS:
    void filesAddedInPlaylist(const QStringList filenames, const quint32 row, const quint32 size);
    void moveInPlaylist(const QList<quint32> &items, const quint32 row, const quint32 size);
    void statsUpdated(int artists, int albums, int songs, quint32 time);

private Q_SLOTS:
    void playListStats();

private:
    QList<Song> songs;
    QSet<qint32> ids;
    qint32 currentSongId;
    StreamFetcher *fetcher;
    MPDStatus::State mpdState;
    bool grouped;
    quint32 dropAdjust;
};

#endif