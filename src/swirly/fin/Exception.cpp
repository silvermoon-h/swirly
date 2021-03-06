/*
 * The Restful Matching-Engine.
 * Copyright (C) 2013, 2018 Swirly Cloud Limited.
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
#include "Exception.hpp"

namespace swirly {
inline namespace fin {
using namespace std;

BadRequestException::~BadRequestException() = default;

AlreadyExistsException::~AlreadyExistsException() = default;

RefAlreadyExistsException::~RefAlreadyExistsException() = default;

InvalidException::~InvalidException() = default;

InvalidLotsException::~InvalidLotsException() = default;

InvalidTicksException::~InvalidTicksException() = default;

ProtocolException::~ProtocolException() = default;

TooLateException::~TooLateException() = default;

UnauthorizedException::~UnauthorizedException() = default;

ForbiddenException::~ForbiddenException() = default;

NotFoundException::~NotFoundException() = default;

AccntNotFoundException::~AccntNotFoundException() = default;

MarketNotFoundException::~MarketNotFoundException() = default;

OrderNotFoundException::~OrderNotFoundException() = default;

MethodNotAllowedException::~MethodNotAllowedException() = default;

InternalException::~InternalException() = default;

DatabaseException::~DatabaseException() = default;

ServiceUnavailableException::~ServiceUnavailableException() = default;

MarketClosedException::~MarketClosedException() = default;

} // namespace fin
} // namespace swirly
