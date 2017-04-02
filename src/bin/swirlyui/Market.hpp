/*
 * The Restful Matching-Engine.
 * Copyright (C) 2013, 2017 Swirly Cloud Limited.
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
#ifndef SWIRLYUI_MARKET_HPP
#define SWIRLYUI_MARKET_HPP

#include "Contr.hpp"
#include "Level.hpp"

namespace swirly {
namespace ui {
namespace market {

enum class Column : int { //
    CheckState, //
    Id, //
    Contr, //
    SettlDate, //
    State, //
    BidCount, //
    BidResd, //
    BidPrice, //
    LastLots, //
    LastPrice, //
    LastTime, //
    OfferPrice, //
    OfferResd, //
    OfferCount
};
constexpr int ColumnCount{unbox(Column::OfferCount) + 1};

} // market

class Market {
  public:
    using Levels = std::array<Level, MaxLevels>;

    Market(Id64 id, const Contr& contr, QDate settlDate, MarketState state, Lots lastLots,
           Ticks lastTicks, const QDateTime& lastTime)
        : id_{id},
          contr_{contr},
          settlDate_{settlDate},
          state_{state},
          lastLots_{lastLots},
          lastTicks_{lastTicks},
          lastTime_{lastTime}
    {
    }
    Market() = default;
    ~Market() noexcept = default;

    static Market fromJson(const Contr& contr, const QJsonObject& obj);

    Id64 id() const noexcept { return id_; }
    const Contr& contr() const noexcept { return contr_; }
    QDate settlDate() const noexcept { return settlDate_; }
    MarketState state() const noexcept { return state_; }
    Lots lastLots() const noexcept { return lastLots_; }
    Ticks lastTicks() const noexcept { return lastTicks_; }
    const QDateTime& lastTime() const noexcept { return lastTime_; }
    const Levels& bids() const noexcept { return bids_; }
    const Levels& offers() const noexcept { return offers_; }
    const Level& bestBid() const noexcept { return bids_.front(); }
    const Level& bestOffer() const noexcept { return offers_.front(); }

  private:
    Id64 id_{};
    Contr contr_{};
    QDate settlDate_{};
    MarketState state_{};
    Lots lastLots_{};
    Ticks lastTicks_{};
    QDateTime lastTime_{};
    Levels bids_{};
    Levels offers_{};
};

QDebug operator<<(QDebug debug, const Market& market);

inline bool isModified(const Market& prev, const Market& next) noexcept
{
    // Assume market-data has changed.
    return true;
}

} // ui
} // swirly

Q_DECLARE_METATYPE(swirly::ui::Market)

#endif // SWIRLYUI_MARKET_HPP
