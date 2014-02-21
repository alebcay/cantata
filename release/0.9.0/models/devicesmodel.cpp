/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "musiclibrarymodel.h"
#include "devicesmodel.h"
#include "playqueuemodel.h"
#include "settings.h"
#include "itemview.h"
#include "mpdparseutils.h"
#include "umsdevice.h"
#include "httpserver.h"
#include "localize.h"
#include "icons.h"
#include "mountpoints.h"
#include <QtGui/QMenu>
#include <QtCore/QStringList>
#include <QtCore/QMimeData>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIcon>
#include <KDE/KGlobal>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/devicenotifier.h>
#include <solid/portablemediaplayer.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>
K_GLOBAL_STATIC(DevicesModel, instance)
#else
#include "solid-lite/device.h"
#include "solid-lite/deviceinterface.h"
#include "solid-lite/devicenotifier.h"
#include "solid-lite/portablemediaplayer.h"
#include "solid-lite/storageaccess.h"
#include "solid-lite/storagedrive.h"
#include "solid-lite/storagevolume.h"
#endif

#include <QtCore/QDebug>
#define DBUG qDebug()

DevicesModel * DevicesModel::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static DevicesModel *instance=0;
    if(!instance) {
        instance=new DevicesModel;
    }
    return instance;
    #endif
}

DevicesModel::DevicesModel(QObject *parent)
    : QAbstractItemModel(parent)
    , itemMenu(0)
    , enabled(false)
    , inhibitMenuUpdate(false)
{
    updateItemMenu();
}

DevicesModel::~DevicesModel()
{
    qDeleteAll(devices);
}

QModelIndex DevicesModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    if (parent.isValid()) {
        MusicLibraryItem *p=static_cast<MusicLibraryItem *>(parent.internalPointer());

        if (p) {
            return row<p->childCount() ? createIndex(row, column, p->childItem(row)) : QModelIndex();
        }
    } else {
        return row<devices.count() ? createIndex(row, column, devices.at(row)) : QModelIndex();
    }

    return QModelIndex();
}

QModelIndex DevicesModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    MusicLibraryItem *childItem = static_cast<MusicLibraryItem *>(index.internalPointer());
    MusicLibraryItem *parentItem = childItem->parentItem();

    if (parentItem) {
        return createIndex(parentItem->parentItem() ? parentItem->row() : devices.indexOf(static_cast<Device *>(parentItem)), 0, parentItem);
    } else {
        return QModelIndex();
    }
}

QVariant DevicesModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int DevicesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    return parent.isValid() ? static_cast<MusicLibraryItem *>(parent.internalPointer())->childCount() : devices.count();
}

int DevicesModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant DevicesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            QString iconName = static_cast<MusicLibraryItemRoot *>(item)->icon();
            return QIcon::fromTheme(iconName.isEmpty() ? QLatin1String("multimedia-player") : iconName);
        }
        case MusicLibraryItem::Type_Artist: {
            MusicLibraryItemArtist *artist = static_cast<MusicLibraryItemArtist *>(item);
            return artist->isVarious() ? Icons::variousArtistsIcon : Icons::artistIcon;
        }
        case MusicLibraryItem::Type_Album:
            if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
                return Icons::albumIcon;
            } else {
                return static_cast<MusicLibraryItemAlbum *>(item)->cover();
            }
        case MusicLibraryItem::Type_Song:   return QIcon::fromTheme("audio-x-generic");
        default: return QVariant();
        }
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Song==item->itemType()) {
            MusicLibraryItemSong *song = static_cast<MusicLibraryItemSong *>(item);
            if (static_cast<MusicLibraryItemAlbum *>(song->parentItem())->isSingleTracks()) {
                return song->song().artistSong();
            } else {
                return song->song().trackAndTitleStr(static_cast<MusicLibraryItemArtist *>(song->parentItem()->parentItem())->isVarious() &&
                                                     !Song::isVariousArtists(song->song().artist));
            }
        } else if(MusicLibraryItem::Type_Album==item->itemType() && MusicLibraryItemAlbum::showDate() &&
                  static_cast<MusicLibraryItemAlbum *>(item)->year()>0) {
            return QString::number(static_cast<MusicLibraryItemAlbum *>(item)->year())+QLatin1String(" - ")+item->data();
        }
        return item->data();
    case Qt::ToolTipRole:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root:
            return 0==item->childCount()
                ? item->data()
                : item->data()+"\n"+
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("1 Artist", "%1 Artists", item->childCount());
                    #else
                    QTP_ARTISTS_STR(item->childCount());
                    #endif
        case MusicLibraryItem::Type_Artist:
            return 0==item->childCount()
                ? item->data()
                : item->data()+"\n"+
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("1 Album", "%1 Albums", item->childCount());
                    #else
                    QTP_ALBUMS_STR(item->childCount());
                    #endif
        case MusicLibraryItem::Type_Album:
            return 0==item->childCount()
                ? item->data()
                : item->data()+"\n"+
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("1 Track (%2)", "%1 Tracks (%2)",item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
                    #else
                    QTP_TRACKS_DURATION_STR(item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
                    #endif
        case MusicLibraryItem::Type_Song:
            return data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time())+
                   QLatin1String("<br/><small><i>")+static_cast<MusicLibraryItemSong *>(item)->song().file+QLatin1String("</i></small>");
        default: return QVariant();
        }
    case ItemView::Role_ImageSize:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            return MusicLibraryItemAlbum::iconSize();
        }
        break;
    case ItemView::Role_SubText:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            Device *dev=static_cast<Device *>(item);

            if (!dev->statusMessage().isEmpty()) {
                return dev->statusMessage();
            }
            if (!dev->isConnected()) {
                return i18n("Not Connected");
            }
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Artist", "%1 Artists", item->childCount());
            #else
            return QTP_ARTISTS_STR(item->childCount());
            #endif
            break;
        }
        case MusicLibraryItem::Type_Artist:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Album", "%1 Albums", item->childCount());
            #else
            return QTP_ALBUMS_STR(item->childCount());
            #endif
            break;
        case MusicLibraryItem::Type_Song:
            return Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time());
        case MusicLibraryItem::Type_Album:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Track (%2)", "%1 Tracks (%2)", item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
            #else
            return QTP_TRACKS_DURATION_STR(item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
            #endif
        default: return QVariant();
        }
    case ItemView::Role_Image:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            QVariant v;
            v.setValue<QPixmap>(static_cast<MusicLibraryItemAlbum *>(item)->cover());
            return v;
        }
        return QVariant();
    case ItemView::Role_Capacity:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            return static_cast<Device *>(item)->usedCapacity();
        }
        return QVariant();
    case ItemView::Role_CapacityText:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            return static_cast<Device *>(item)->capacityString();
        }
        return QVariant();
    case ItemView::Role_ToggleIcon:
        if (MusicLibraryItem::Type_Root==item->itemType() && static_cast<Device *>(item)->supportsDisconnect()) {
            return static_cast<Device *>(item)->isConnected() ? Icons::disconnectIcon : Icons::connectIcon;
        }
        return QVariant();
    default:
        return QVariant();
    }
    return QVariant();
}

void DevicesModel::clear()
{
    inhibitMenuUpdate=true;
    QSet<QString> remoteUdis;
    QSet<QString> udis;
    foreach (Device *dev, devices) {
        if (Device::RemoteFs==dev->devType()) {
            remoteUdis.insert(dev->udi());
        } else {
            udis.insert(dev->udi());
        }
    }

    foreach (const QString &u, udis) {
        deviceRemoved(u);
    }
    foreach (const QString &u, remoteUdis) {
        removeRemoteDevice(u);
    }

    devices.clear();
    volumes.clear();
    inhibitMenuUpdate=false;
    updateItemMenu();
}

void DevicesModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }

    enabled=e;

    inhibitMenuUpdate=true;
    if (enabled) {
        connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(const QString &)), this, SLOT(deviceAdded(const QString &)));
        connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(const QString &)), this, SLOT(deviceRemoved(const QString &)));
//        connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
//                this, SLOT(setCover(const Song &, const QImage &, const QString &)));
        loadLocal();
        connect(MountPoints::self(), SIGNAL(updated()), this, SLOT(mountsChanged()));
        #ifdef ENABLE_REMOTE_DEVICES
        loadRemote();
        #endif
    } else {
        disconnect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(const QString &)), this, SLOT(deviceAdded(const QString &)));
        disconnect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(const QString &)), this, SLOT(deviceRemoved(const QString &)));
//        disconnect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
//                   this, SLOT(setCover(const Song &, const QImage &, const QString &)));
        disconnect(MountPoints::self(), SIGNAL(updated()), this, SLOT(mountsChanged()));
        clear();
    }
    inhibitMenuUpdate=false;
    updateItemMenu();
}

Device * DevicesModel::device(const QString &udi)
{
    int idx=indexOf(udi);
    return idx<0 ? 0 : devices.at(idx);
}

void DevicesModel::setCover(const Song &song, const QImage &img)
{
    if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
        return;
    }

    if (img.isNull()) {
        return;
    }

    Device *dev=qobject_cast<Device *>(sender());
    if (!dev) {
        return;
    }
    int i=devices.indexOf(dev);
    if (i<0) {
        return;
    }
    MusicLibraryItemArtist *artistItem = static_cast<MusicLibraryItemRoot *>(dev)->artist(song, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
        if (albumItem) {
            if (static_cast<const MusicLibraryItemAlbum *>(albumItem)->setCover(img)) {
                QModelIndex idx=index(artistItem->childItems().indexOf(albumItem), 0, index(static_cast<MusicLibraryItemContainer *>(dev)->childItems().indexOf(artistItem), 0, index(i, 0, QModelIndex())));
                emit dataChanged(idx, idx);
            }
        }
    }
}

void DevicesModel::deviceUpdating(const QString &udi, bool state)
{
    int idx=indexOf(udi);
    if (idx>=0) {
        Device *dev=devices.at(idx);

        if (state) {
            QModelIndex modelIndex=createIndex(idx, 0, dev);
            emit dataChanged(modelIndex, modelIndex);
        } else {
            if (dev->haveUpdate()) {
                dev->applyUpdate();
            }
            QModelIndex modelIndex=createIndex(idx, 0, dev);
            emit dataChanged(modelIndex, modelIndex);
            QSet<QString> genres;
            foreach (Device *dev, devices) {
                genres+=dev->genres();
            }
            emit updateGenres(genres);
        }
    }
}

Qt::ItemFlags DevicesModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
    }
    return Qt::ItemIsEnabled;
}

QStringList DevicesModel::filenames(const QModelIndexList &indexes, bool playableOnly, bool fullPath) const
{
    QList<Song> songList=songs(indexes, playableOnly, fullPath);
    QStringList fnames;
    foreach (const Song &s, songList) {
        fnames.append(s.file);
    }
    return fnames;
}

static Song fixPath(Song s, const QString &path)
{
    if (!path.isEmpty()) {
        s.file=path+s.file;
    }
    return s;
}

QList<Song> DevicesModel::songs(const QModelIndexList &indexes, bool playableOnly, bool fullPath) const
{
    QMap<MusicLibraryItem *, QList<Song> > devSongs;

    foreach(QModelIndex index, indexes) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());
        MusicLibraryItem *parent=item;

        while (parent->parentItem()) {
            parent=parent->parentItem();
        }

        if (!parent) {
            continue;
        }
        if (playableOnly && !static_cast<Device *>(parent)->canPlaySongs()) {
            continue;
        }

        QString base=fullPath ? static_cast<Device *>(parent)->path() : QString();

        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            // First, sort all artists as they would appear in UI...
            QList<MusicLibraryItem *> artists=static_cast<const MusicLibraryItemContainer *>(item)->childItems();
            qSort(artists.begin(), artists.end(), MusicLibraryItemArtist::lessThan);
            foreach (MusicLibraryItem *a, artists) {
                const MusicLibraryItemContainer *artist=static_cast<const MusicLibraryItemContainer *>(a);
                // Now sort all albums as they would appear in UI...
                QList<MusicLibraryItem *> artistAlbums=static_cast<const MusicLibraryItemContainer *>(artist)->childItems();
                qSort(artistAlbums.begin(), artistAlbums.end(), MusicLibraryItemAlbum::lessThan);
                foreach (MusicLibraryItem *i, artistAlbums) {
                    const MusicLibraryItemContainer *album=static_cast<const MusicLibraryItemContainer *>(i);
                    foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                        if (MusicLibraryItem::Type_Song==song->itemType() && !devSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                            devSongs[parent] << fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), base);
                        }
                    }
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Artist: {
            // First, sort all albums as they would appear in UI...
            QList<MusicLibraryItem *> artistAlbums=static_cast<const MusicLibraryItemContainer *>(item)->childItems();
            qSort(artistAlbums.begin(), artistAlbums.end(), MusicLibraryItemAlbum::lessThan);

            foreach (MusicLibraryItem *i, artistAlbums) {
                const MusicLibraryItemContainer *album=static_cast<const MusicLibraryItemContainer *>(i);
                foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                    if (MusicLibraryItem::Type_Song==song->itemType() && !devSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                        devSongs[parent] << fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), base);
                    }
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Album:
            foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
                if (MusicLibraryItem::Type_Song==song->itemType() && !devSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                    devSongs[parent] << fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), base);
                }
            }
            break;
        case MusicLibraryItem::Type_Song:
            if (!devSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(item)->song())) {
                devSongs[parent] << fixPath(static_cast<const MusicLibraryItemSong*>(item)->song(), base);
            }
            break;
        default:
            break;
        }
    }

    QList<Song> songs;
    QMap<MusicLibraryItem *, QList<Song> >::Iterator it(devSongs.begin());
    QMap<MusicLibraryItem *, QList<Song> >::Iterator end(devSongs.end());

    for (; it!=end; ++it) {
        songs.append(it.value());
    }

    return songs;
}

void DevicesModel::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres)
{
    foreach (Device *dev, devices) {
        dev->getDetails(artists, albumArtists, albums, genres);
    }
}

void DevicesModel::emitAddToDevice()
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        emit addToDevice(act->data().toString());
    }
}

void DevicesModel::deviceAdded(const QString &udi)
{
    if (indexOf(udi)>=0) {
        return;
    }

    Solid::Device device(udi);
    DBUG << "Solid device added udi:" << device.udi() << "product:" << device.product() << "vendor:" << device.vendor();
    Solid::StorageAccess *ssa = device.as<Solid::StorageAccess>();

    if (ssa) {
        if ((!device.parent().as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.parent().as<Solid::StorageDrive>()->bus()) &&
            (!device.as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.as<Solid::StorageDrive>()->bus())) {
            DBUG << "Found Solid::StorageAccess that is not usb, skipping";
            return;
        }
        DBUG << "volume is generic storage";
        if (!volumes.contains(device.udi())) {
            connect(ssa, SIGNAL(accessibilityChanged(bool, const QString&)), this, SLOT(accessibilityChanged(bool, const QString&)));
            volumes.insert(device.udi());
        }
    } else if (device.is<Solid::StorageDrive>()) {
        DBUG << "device is a Storage drive, still need a volume";
    } else if (device.is<Solid::PortableMediaPlayer>()) {
        DBUG << "device is a PMP";
    } else {
        DBUG << "device not handled";
        return;
    }
    addLocalDevice(device.udi());
}

void DevicesModel::addLocalDevice(const QString &udi)
{
    Device *dev=Device::create(this, udi);
    if (dev) {
        beginInsertRows(QModelIndex(), devices.count(), devices.count());
        devices.append(dev);
        endInsertRows();
        connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
        connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
        connect(dev, SIGNAL(cover(const Song &, const QImage &)), SLOT(setCover(const Song &, const QImage &)));
        updateItemMenu();
    }
}

void DevicesModel::deviceRemoved(const QString &udi)
{
    int idx=indexOf(udi);
    DBUG << "Solid device removed udi = " << udi << idx;
    if (idx>=0) {
        if (volumes.contains(udi)) {
            Solid::Device device(udi);
            Solid::StorageAccess *ssa = device.as<Solid::StorageAccess>();
            if (ssa) {
                disconnect(ssa, SIGNAL(accessibilityChanged(bool, const QString&)), this, SLOT(accessibilityChanged(bool, const QString&)));
            }
            volumes.remove(udi);
        }

        beginRemoveRows(QModelIndex(), idx, idx);
        devices.takeAt(idx)->deleteLater();
        endRemoveRows();
        updateItemMenu();
    }
}

void DevicesModel::accessibilityChanged(bool accessible, const QString &udi)
{
    Q_UNUSED(accessible)
    int idx=indexOf(udi);
    DBUG << "Solid device accesibility changed udi = " << udi << idx << accessible;
    if (idx>=0) {
        Device *dev=devices.at(idx);
        if (dev) {
            dev->connectionStateChanged();
            QModelIndex modelIndex=createIndex(idx, 0, dev);
            emit dataChanged(modelIndex, modelIndex);
        }
    }
}
void DevicesModel::addRemoteDevice(const QString &coverFileName, const DeviceOptions &opts, RemoteFsDevice::Details details)
{
    #ifdef ENABLE_REMOTE_DEVICES
    Device *dev=RemoteFsDevice::create(this, coverFileName, opts, details);

    if (dev) {
        beginInsertRows(QModelIndex(), devices.count(), devices.count());
        devices.append(dev);
        endInsertRows();
        connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
        connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
        connect(dev, SIGNAL(cover(const Song &, const QImage &)), SLOT(setCover(const Song &, const QImage &)));
        if (Device::RemoteFs==dev->devType()) {
            connect(static_cast<RemoteFsDevice *>(dev), SIGNAL(udiChanged()), SLOT(remoteDeviceUdiChanged()));
        }
        updateItemMenu();
    }
    #else
    Q_UNUSED(coverFileName)
    Q_UNUSED(opts)
    Q_UNUSED(details)
    #endif
}

void DevicesModel::removeRemoteDevice(const QString &udi)
{
    #ifdef ENABLE_REMOTE_DEVICES
    int idx=indexOf(udi);
    if (idx<0) {
        return;
    }

    Device *dev=devices.at(idx);

    if (dev && Device::RemoteFs==dev->devType()) {
        beginRemoveRows(QModelIndex(), idx, idx);
        // Remove device from list, but do NOT delete - it may be scanning!!!!
        devices.takeAt(idx);
        endRemoveRows();
        updateItemMenu();
        // Remove will stop device, and delete it
        RemoteFsDevice::remove(dev);
    }
    #else
    Q_UNUSED(udi)
    #endif
}

void DevicesModel::remoteDeviceUdiChanged()
{
    #ifdef ENABLE_REMOTE_DEVICES
    updateItemMenu();
    #endif
}

void DevicesModel::mountsChanged()
{
    #ifdef ENABLE_REMOTE_DEVICES
    foreach (Device *dev, devices) {
        if (Device::RemoteFs==dev->devType()) {
            if (0==dev->childCount()) {
                ((RemoteFsDevice *)dev)->load();
            } else if (!dev->isConnected()) {
                ((RemoteFsDevice *)dev)->clear();
            }
        }
    }
    #endif

    // For some reason if a device without a partition (e.g. /dev/sdc) is mounted whilst cantata is running, then we receive no deviceAdded signal
    // So, as a work-around, each time a device is mounted - check for all local devices. :-)
    // BUG:127
    loadLocal();
}

void DevicesModel::loadLocal()
{
    // Build set of currently known MTP/UMS devices...
    QSet<QString> existingUdis;
    foreach (const Device *dev, devices) {
        if (Device::Mtp==dev->devType() || Device::Ums==dev->devType()) {
            existingUdis.insert(dev->udi());
        }
    }

    QList<Solid::Device> deviceList = Solid::Device::listFromType(Solid::DeviceInterface::PortableMediaPlayer);
    foreach (const Solid::Device &device, deviceList) {
        if (existingUdis.contains(device.udi())) {
            existingUdis.remove(device.udi());
            continue;
        }
        if (device.as<Solid::StorageDrive>()) {
            DBUG << "Solid PMP that is also a StorageDrive, skipping, udi:" << device.udi() << "product:" << device.product() << "vendor:" << device.vendor();
            continue;
        }
        DBUG << "Solid::PortableMediaPlayer with udi:" << device.udi() << "product:" << device.product() << "vendor:" << device.vendor();
        addLocalDevice(device.udi());
    }
    deviceList = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    foreach (const Solid::Device &device, deviceList)
    {
        if (existingUdis.contains(device.udi())) {
            existingUdis.remove(device.udi());
            continue;
        }
        DBUG << "Solid::StorageAccess with udi:" << device.udi() << "product:" << device.product() << "vendor:" << device.vendor();
        const Solid::StorageAccess *ssa = device.as<Solid::StorageAccess>();

        if (ssa) {
            if ((!device.parent().as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.parent().as<Solid::StorageDrive>()->bus()) &&
                (!device.as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.as<Solid::StorageDrive>()->bus())) {
                DBUG << "Solid::StorageAccess that is not usb, skipping";
                continue;
            }
            if (!volumes.contains(device.udi())) {
                connect(ssa, SIGNAL(accessibilityChanged(bool, const QString&)), this, SLOT(accessibilityChanged(bool, const QString&)));
                volumes.insert(device.udi());
            }
            addLocalDevice(device.udi());
        }
    }

    // Remove any previous MTP/UMS devices that were not listed above.
    // This is to fix BUG:127
    foreach (const QString &udi, existingUdis) {
        deviceRemoved(udi);
    }
}

#ifdef ENABLE_REMOTE_DEVICES
void DevicesModel::loadRemote()
{
    QList<Device *> rem=RemoteFsDevice::loadAll(this);
    if (rem.count()) {
        beginInsertRows(QModelIndex(), devices.count(), devices.count()+(rem.count()-1));
        foreach (Device *dev, rem) {
            devices.append(dev);
            connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
            connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
            connect(dev, SIGNAL(cover(const Song &, const QImage &)), SLOT(setCover(const Song &, const QImage &)));
            if (Device::RemoteFs==dev->devType()) {
                connect(static_cast<RemoteFsDevice *>(dev), SIGNAL(udiChanged()), SLOT(remoteDeviceUdiChanged()));
            }
        }
        endInsertRows();
        updateItemMenu();
    }
}

void DevicesModel::unmountRemote()
{
    foreach (Device *dev, devices) {
        if (Device::RemoteFs==dev->devType()) {
            static_cast<RemoteFsDevice *>(dev)->unmount();
        }
    }
}
#endif

void DevicesModel::toggleGrouping()
{
    beginResetModel();
    foreach (Device *dev, devices) {
        dev->toggleGrouping();
    }
    endResetModel();
}

int DevicesModel::indexOf(const QString &udi)
{
    int i=0;
    foreach (Device *dev, devices) {
        if (dev->udi()==udi) {
            return i;
        }
        i++;
    }
    return -1;
}

static bool lessThan(const QString &left, const QString &right)
{
    return left.localeAwareCompare(right)<0;
}

void DevicesModel::updateItemMenu()
{
    if (inhibitMenuUpdate) {
        return;
    }

    if (!itemMenu) {
        itemMenu = new QMenu(0);
    }

    itemMenu->clear();

    if (devices.isEmpty()) {
        itemMenu->addAction(i18n("No Devices Attached"))->setEnabled(false);
    } else {
        QMap<QString, const Device *> items;

        foreach (const Device *d, devices) {
            items.insert(d->data(), d);
        }

        QStringList keys=items.keys();
        qSort(keys.begin(), keys.end(), lessThan);

        foreach (const QString &k, keys) {
            const Device *d=items[k];
            itemMenu->addAction(QIcon::fromTheme(d->icon()), k, this, SLOT(emitAddToDevice()))->setData(d->udi());
        }
    }
}

QMimeData * DevicesModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData=0;
    QStringList paths;
    if (HttpServer::self()->isAlive()) {
        QList<Song> songList=songs(indexes, true, true);
        foreach (const Song &s, songList) {
            paths.append(HttpServer::self()->encodeUrl(s));
        }
    } else {
        paths=filenames(indexes, true, true);
    }

    if (!paths.isEmpty()) {
        mimeData=new QMimeData();
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}