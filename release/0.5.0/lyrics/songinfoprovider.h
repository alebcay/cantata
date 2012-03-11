/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SONGINFOPROVIDER_H
#define SONGINFOPROVIDER_H

#include <QtCore/QObject>
#include <QtCore/QUrl>

//#include "collapsibleinfopane.h"
//#include "core/song.h"

class Song;

class SongInfoProvider : public QObject {
  Q_OBJECT

public:
  SongInfoProvider();

  virtual void FetchInfo(int id, const Song& metadata) = 0;
//   virtual void Cancel(int id) {}

  virtual QString name() const;

  bool is_enabled() const { return enabled_; }
  void set_enabled(bool enabled) { enabled_ = enabled; }

signals:
  void ImageReady(int id, const QUrl& url);
  void InfoReady(int id, const QString& data);
  void Finished(int id);

private:
  bool enabled_;
};

#endif // SONGINFOPROVIDER_H
