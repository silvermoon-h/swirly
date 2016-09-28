/*
 * The Restful Matching-Engine.
 * Copyright (C) 2013, 2016 Swirly Cloud Limited.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if
 * not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef SWIRLYUI_MARKETVIEW_HPP
#define SWIRLYUI_MARKETVIEW_HPP

#include <QWidget>

class QModelIndex;

namespace swirly {
namespace ui {

class Market;
class MarketModel;

class MarketView : public QWidget {
  Q_OBJECT

 public:
  MarketView(MarketModel& model, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags{});
  ~MarketView() noexcept override;

 signals:
  void currentChanged(const Market& market);

  void doubleClicked(const Market& market);

 private slots:
  void slotCurrentChanged(const QModelIndex& current, const QModelIndex& previous);

  void slotDoubleClicked(const QModelIndex& index);

 private:
  MarketModel& model_;
};

} // ui
} // swirly

#endif // SWIRLYUI_MARKETVIEW_HPP
