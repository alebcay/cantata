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

#ifndef STREAMSPAGE_H
#define STREAMSPAGE_H

#include "ui_streamspage.h"
#include "mainwindow.h"
#include "streamsmodel.h"
#include "streamsproxymodel.h"

class StreamsPage : public QWidget, public Ui::StreamsPage
{
    Q_OBJECT

public:
    StreamsPage(MainWindow *p);
    virtual ~StreamsPage();

    void setEnabled(bool e);
    bool isEnabled() const { return enabled; }
    void refresh();
    void save();
    void addSelectionToPlaylist(bool replace, quint8 priorty=0);
    void setView(bool tree) { view->setMode(tree ? ItemView::Mode_Tree : ItemView::Mode_List); }
    void focusSearch() { view->focusSearch(); }
    void goBack() { view->backActivated(); }

Q_SIGNALS:
    void add(const QStringList &streams, bool replace, quint8 priorty);

public Q_SLOTS:
    void removeItems();
    void controlActions();

private Q_SLOTS:
    void importXml();
    void exportXml();
    void add();
    void edit();
    void searchItems();
    void itemDoubleClicked(const QModelIndex &index);
    void updateGenres(const QSet<QString> &g);

private:
    void addItemsToPlayQueue(const QModelIndexList &indexes, bool replace, quint8 priorty=0);
    QStringList getCategories();
    QStringList getGenres();

private:
    bool enabled;
    Action *importAction;
    Action *exportAction;
    Action *addAction;
    Action *editAction;
    StreamsModel model;
    StreamsProxyModel proxy;
    QSet<QString> genres;
    MainWindow *mw;
};

#endif