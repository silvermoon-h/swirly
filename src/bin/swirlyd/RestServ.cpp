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
#include "RestServ.hpp"

#include <swirly/fir/Rest.hpp>

#include <swirly/elm/Exception.hpp>

#include <swirly/ash/Finally.hpp>
#include <swirly/ash/Log.hpp>
#include <swirly/ash/Time.hpp>

#include <chrono>

using namespace std;

namespace swirly {
namespace mg {
namespace {

class ScopedIdens {
 public:
  ScopedIdens(string_view sv, vector<Iden>& ids) noexcept : ids_{ids}
  {
    Tokeniser<','> toks{sv};
    while (!toks.empty()) {
      ids.push_back(static_cast<Iden>(stou64(toks.top())));
      toks.pop();
    }
  }
  ~ScopedIdens() noexcept { ids_.clear(); }

 private:
  vector<Iden>& ids_;
};

class ScopedMnems {
 public:
  ScopedMnems(string_view sv, vector<Mnem>& mnems) noexcept : mnems_{mnems}
  {
    Tokeniser<','> toks{sv};
    while (!toks.empty()) {
      mnems.push_back(toks.top());
      toks.pop();
    }
  }
  ~ScopedMnems() noexcept { mnems_.clear(); }

 private:
  vector<Mnem>& mnems_;
};

string_view getDnAttribute(string_view dn, string_view attr) noexcept
{
  assert(!dn.empty());
  if (dn[0] == '/') {
    // Trim leading slash.
    dn.remove_prefix(1);
  }
  Tokeniser<'/'> toks{dn};
  while (!toks.empty()) {
    string_view key, val;
    tie(key, val) = splitPair(toks.top(), '=');
    if (key == attr) {
      return val;
    }
    toks.pop();
  }
  return {};
}

string_view getETrader(HttpMessage data, const char* httpAuth)
{
  // User's Distinguished Name (DN).
  const string_view dn{data.header(httpAuth)};
  if (dn.empty()) {
    throw UnauthorizedException{"client dn is required"_sv};
  }
  // Expect emailAddress attribute in client's SSL certificate.
  const string_view email{getDnAttribute(dn, "emailAddress"_sv)};
  if (email.empty()) {
    throw UnauthorizedException{"client email is required"_sv};
  }
  return email;
}

Millis getTime(HttpMessage data, const char* httpTime = nullptr) noexcept
{
  string_view val;
  if (httpTime) {
    val = data.header(httpTime);
  }
  return val.empty() ? getTimeOfDay() : box<Millis>(stou64(val));
}
} // anonymous

RestServ::~RestServ() noexcept = default;

bool RestServ::reset(HttpMessage data) noexcept
{
  bool cache{false};
  state_ = 0;

  auto uri = data.uri();
  // Remove leading slash.
  if (uri.front() == '/') {
    uri.remove_prefix(1);
  }
  uri_.reset(uri);

  const auto method = data.method();
  if (method == "GET"_sv) {
    cache = !uri.empty() && uri_.top() == "rec"_sv;
    state_ |= MethodGet;
  } else if (method == "POST"_sv) {
    state_ |= MethodPost;
  } else if (method == "PUT"_sv) {
    state_ |= MethodPut;
  } else if (method == "DELETE"_sv) {
    state_ |= MethodDelete;
  }

  request_.reset();
  return cache;
}

void RestServ::httpRequest(mg_connection& nc, HttpMessage data)
{
  using namespace chrono;

  TimeRecorder tr{profile_};
  auto finally = makeFinally([this]() {
    if (this->profile_.size() % 10 == 0) {
      this->profile_.report();
    }
  });
  const auto cache = reset(data);
  const auto now = getTime(data, httpTime_);
  // See mg_send().
  nc.last_io_time = unbox(now) / 1000;

  StreamBuf buf{nc.send_mbuf};
  out_.rdbuf(&buf);
  if (!isSet(MethodDelete)) {
    out_.reset(200, "OK", cache);
  } else {
    out_.reset(204, "No Content");
  }
  try {
    const auto body = data.body();
    if (!body.empty()) {
      if (!request_.parse(data.body())) {
        throw BadRequestException{"request body is incomplete"_sv};
      }
    }
    restRequest(data, now);
    if (!isSet(MatchUri)) {
      throw NotFoundException{errMsg() << "resource '" << data.uri() << "' does not exist"};
    }
    if (!isSet(MatchMethod)) {
      throw MethodNotAllowedException{errMsg() << "method '" << data.method()
                                               << "' is not allowed"};
    }
  } catch (const ServException& e) {
    out_.reset(e.httpStatus(), e.httpReason());
    out_ << e;
  } catch (const exception& e) {
    const int status{500};
    const char* const reason{"Internal Server Error"};
    out_.reset(status, reason);
    ServException::toJson(status, reason, e.what(), out_);
  }
  out_.setContentLength();
}

void RestServ::restRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {
    return;
  }

  const auto tok = uri_.top();
  uri_.pop();

  if (tok == "rec"_sv) {
    // /rec
    recRequest(data, now);
  } else if (tok == "sess"_sv) {
    // /sess
    sessRequest(data, now);
  } else if (tok == "view"_sv) {
    // /view
    viewRequest(data, now);
  }
}

void RestServ::recRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {

    // /rec
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /rec
      state_ |= MatchMethod;
      const int bs{EntitySet::Asset | EntitySet::Contr | EntitySet::Market};
      rest_.getRec(bs, now, out_);
    }
    return;
  }

  const auto tok = uri_.top();
  uri_.pop();

  const auto es = EntitySet::parse(tok);
  if (es.many()) {

    if (uri_.empty()) {

      // /rec/entity,entity...
      state_ |= MatchUri;

      if (isSet(MethodGet)) {
        // GET /rec/entity,entity...
        state_ |= MatchMethod;
        rest_.getRec(es, now, out_);
      }
    }
    return;
  }

  switch (es.get()) {
  case EntitySet::Asset:
    assetRequest(data, now);
    break;
  case EntitySet::Contr:
    contrRequest(data, now);
    break;
  case EntitySet::Market:
    marketRequest(data, now);
    break;
  case EntitySet::Trader:
    traderRequest(data, now);
    break;
  }
}

void RestServ::assetRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {

    // /rec/asset
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /rec/asset
      state_ |= MatchMethod;
      rest_.getAsset(now, out_);
    }
    return;
  }

  const auto mnem = uri_.top();
  uri_.pop();

  if (uri_.empty()) {

    // /rec/asset/MNEM
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /rec/asset/MNEM
      state_ |= MatchMethod;
      rest_.getAsset(mnem, now, out_);
    }
    return;
  }
}

void RestServ::contrRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {

    // /rec/contr
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /rec/contr
      state_ |= MatchMethod;
      rest_.getContr(now, out_);
    }
    return;
  }

  const auto mnem = uri_.top();
  uri_.pop();

  if (uri_.empty()) {

    // /rec/contr/MNEM
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /rec/contr/MNEM
      state_ |= MatchMethod;
      rest_.getContr(mnem, now, out_);
    }
    return;
  }
}

void RestServ::marketRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {

    // /rec/market
    state_ |= MatchUri;

    switch (state_ & MethodMask) {
    case MethodGet:
      // GET /rec/market
      state_ |= MatchMethod;
      rest_.getMarket(now, out_);
      break;
    case MethodPost:
      // POST /rec/market
      state_ |= MatchMethod;

      constexpr auto reqFields = RestRequest::Mnem | RestRequest::Display | RestRequest::Contr;
      constexpr auto optFields
        = RestRequest::SettlDate | RestRequest::ExpiryDate | RestRequest::State;
      if (!request_.valid(reqFields, optFields)) {
        throw InvalidException{"request fields are invalid"_sv};
      }
      rest_.postMarket(request_.mnem(), request_.display(), request_.contr(), request_.settlDate(),
                       request_.expiryDate(), request_.state(), now, out_);
      break;
    }
    return;
  }

  const auto mnem = uri_.top();
  uri_.pop();

  if (uri_.empty()) {

    // /rec/market/MNEM
    state_ |= MatchUri;

    switch (state_ & MethodMask) {
    case MethodGet:
      // GET /rec/market/MNEM
      state_ |= MatchMethod;
      rest_.getMarket(mnem, now, out_);
      break;
    case MethodPut:
      // PUT /rec/market/MNEM
      state_ |= MatchMethod;

      constexpr auto reqFields = 0x0;
      constexpr auto optFields = RestRequest::Display | RestRequest::State;
      if (!request_.valid(reqFields, optFields)) {
        throw InvalidException{"request fields are invalid"_sv};
      }
      optional<string_view> display;
      if ((request_.fields() & RestRequest::Display)) {
        display = request_.display();
      }
      optional<MarketState> state;
      if ((request_.fields() & RestRequest::State)) {
        state = request_.state();
      }
      rest_.putMarket(mnem, display, state, now, out_);
      break;
    }
    return;
  }
}

void RestServ::traderRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {

    // /rec/trader
    state_ |= MatchUri;

    switch (state_ & MethodMask) {
    case MethodGet:
      // GET /rec/trader
      state_ |= MatchMethod;
      rest_.getTrader(now, out_);
      break;
    case MethodPost:
      // POST /rec/market
      state_ |= MatchMethod;

      // FIXME: Incomplete. See BackRecServlet.java
      constexpr auto reqFields = RestRequest::Mnem | RestRequest::Display;
      constexpr auto optFields = RestRequest::Email;
      if (!request_.valid(reqFields, optFields)) {
        throw InvalidException{"request fields are invalid"_sv};
      }
      rest_.postTrader(request_.mnem(), request_.display(), request_.email(), now, out_);
      break;
    }
    return;
  }

  const auto mnem = uri_.top();
  uri_.pop();

  if (uri_.empty()) {

    // /rec/trader/MNEM
    state_ |= MatchUri;

    switch (state_ & MethodMask) {
    case MethodGet:
      // GET /rec/trader/MNEM
      state_ |= MatchMethod;
      rest_.getTrader(mnem, now, out_);
      break;
    case MethodPut:
      // PUT /rec/trader/MNEM
      state_ |= MatchMethod;

      // FIXME: Incomplete. See BackRecServlet.java
      constexpr auto reqFields = RestRequest::Display;
      constexpr auto optFields = RestRequest::Email;
      if (!request_.valid(reqFields, optFields)) {
        throw InvalidException{"request fields are invalid"_sv};
      }
      rest_.putTrader(mnem, request_.display(), now, out_);
      break;
    }
    return;
  }
}

void RestServ::sessRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {

    // /sess
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /sess
      state_ |= MatchMethod;
      const int bs{EntitySet::Order | EntitySet::Trade | EntitySet::Posn | EntitySet::View};
      rest_.getSess(getETrader(data, httpAuth_), bs, now, out_);
    }
    return;
  }

  const auto tok = uri_.top();
  uri_.pop();

  const auto es = EntitySet::parse(tok);
  if (es.many()) {

    if (uri_.empty()) {

      // /sess/entity,entity...
      state_ |= MatchUri;

      if (isSet(MethodGet)) {
        // GET /sess/entity,entity...
        state_ |= MatchMethod;
        rest_.getSess(getETrader(data, httpAuth_), es, now, out_);
      }
    }
    return;
  }

  switch (es.get()) {
  case EntitySet::Order:
    orderRequest(data, now);
    break;
  case EntitySet::Trade:
    tradeRequest(data, now);
    break;
  case EntitySet::Posn:
    posnRequest(data, now);
    break;
  case EntitySet::View:
    viewRequest(data, now);
    break;
  }
}

void RestServ::orderRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {

    // /sess/order
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /sess/order
      state_ |= MatchMethod;
      rest_.getOrder(getETrader(data, httpAuth_), now, out_);
    }
    return;
  }

  const auto market = uri_.top();
  uri_.pop();

  if (uri_.empty()) {

    // /sess/order/MARKET
    state_ |= MatchUri;

    switch (state_ & MethodMask) {
    case MethodGet:
      // GET /sess/order/MARKET
      state_ |= MatchMethod;
      rest_.getOrder(market, now, out_);
      break;
    case MethodPost:
      // POST /sess/order/MARKET
      state_ |= MatchMethod;
      {
        constexpr auto reqFields = RestRequest::Side | RestRequest::Lots | RestRequest::Ticks;
        constexpr auto optFields = RestRequest::Ref | RestRequest::MinLots;
        if (!request_.valid(reqFields, optFields)) {
          throw InvalidException{"request fields are invalid"_sv};
        }
        rest_.postOrder(getETrader(data, httpAuth_), market, request_.ref(), request_.side(),
                        request_.lots(), request_.ticks(), request_.minLots(), now, out_);
      }
      break;
    }
    return;
  }

  ScopedIdens ids{uri_.top(), ids_};
  uri_.pop();

  if (uri_.empty()) {

    // /sess/order/MARKET/ID,ID...
    state_ |= MatchUri;

    switch (state_ & MethodMask) {
    case MethodGet:
      // GET /sess/order/MARKET/ID
      state_ |= MatchMethod;
      rest_.getOrder(getETrader(data, httpAuth_), market, ids_[0], now, out_);
      break;
    case MethodPut:
      // PUT /sess/order/MARKET/ID,ID...
      state_ |= MatchMethod;
      {
        constexpr auto reqFields = RestRequest::Lots;
        if (request_.fields() != reqFields) {
          throw InvalidException{"request fields are invalid"_sv};
        }
        rest_.putOrder(getETrader(data, httpAuth_), market, ids_, request_.lots(), now, out_);
      }
      break;
    case MethodDelete:
      // DELETE /sess/order/MARKET/ID,ID...
      state_ |= MatchMethod;
      rest_.deleteOrder(getETrader(data, httpAuth_), market, ids_, now);
      break;
    }
    return;
  }
}

void RestServ::tradeRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {

    // /sess/trade
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /sess/trade
      state_ |= MatchMethod;
      rest_.getTrade(getETrader(data, httpAuth_), now, out_);
    }
    return;
  }

  const auto market = uri_.top();
  uri_.pop();

  if (uri_.empty()) {

    // /sess/trade/MARKET
    state_ |= MatchUri;

    switch (state_ & MethodMask) {
    case MethodGet:
      // GET /sess/trade/MARKET
      state_ |= MatchMethod;
      rest_.getTrade(market, now, out_);
      break;
    case MethodPost:
      // POST /sess/trade/MARKET
      state_ |= MatchMethod;
      {
        constexpr auto reqFields = RestRequest::Trader | RestRequest::Side | RestRequest::Lots;
        constexpr auto optFields
          = RestRequest::Ref | RestRequest::Ticks | RestRequest::LiqInd | RestRequest::Cpty;
        if (!request_.valid(reqFields, optFields)) {
          throw InvalidException{"request fields are invalid"_sv};
        }
        rest_.postTrade(request_.trader(), market, request_.ref(), request_.side(), request_.lots(),
                        request_.ticks(), request_.liqInd(), request_.cpty(), now, out_);
      }
      break;
    }
    return;
  }

  ScopedIdens ids{uri_.top(), ids_};
  uri_.pop();

  if (uri_.empty()) {

    // /sess/trade/MARKET/ID,ID...
    state_ |= MatchUri;

    switch (state_ & MethodMask) {
    case MethodGet:
      // GET /sess/trade/MARKET/ID
      state_ |= MatchMethod;
      rest_.getTrade(getETrader(data, httpAuth_), market, ids_[0], now, out_);
      break;
    case MethodDelete:
      // DELETE /sess/trade/MARKET/ID,ID...
      state_ |= MatchMethod;
      rest_.deleteTrade(getETrader(data, httpAuth_), market, ids_, now);
      break;
    }
    return;
  }
}

void RestServ::posnRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {

    // /sess/posn
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /sess/posn
      state_ |= MatchMethod;
      rest_.getPosn(getETrader(data, httpAuth_), now, out_);
    }
    return;
  }

  const auto contr = uri_.top();
  uri_.pop();

  if (uri_.empty()) {

    // /sess/posn/CONTR
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /sess/posn/CONTR
      state_ |= MatchMethod;
      rest_.getPosn(contr, now, out_);
    }
    return;
  }

  const auto settlDate = box<IsoDate>(stou64(uri_.top()));
  uri_.pop();

  if (uri_.empty()) {

    // /sess/posn/CONTR/SETTL_DATE
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /sess/posn/CONTR/SETTL_DATE
      state_ |= MatchMethod;
      rest_.getPosn(getETrader(data, httpAuth_), contr, settlDate, now, out_);
    }
    return;
  }
}

void RestServ::viewRequest(HttpMessage data, Millis now)
{
  if (uri_.empty()) {

    // /view
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /view
      state_ |= MatchMethod;
      rest_.getView(now, out_);
    }
    return;
  }

  ScopedMnems mnems{uri_.top(), mnems_};
  uri_.pop();

  if (uri_.empty()) {

    // /view/MARKET,MARKET...
    state_ |= MatchUri;

    if (isSet(MethodGet)) {
      // GET /view/MARKET,MARKET...
      state_ |= MatchMethod;
      rest_.getView(mnems_, now, out_);
    }
    return;
  }
}

} // mg
} // swirly
