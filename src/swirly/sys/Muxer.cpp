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
#include "Muxer.hpp"

#include <algorithm>
#include <cassert>

namespace swirly {

void PollPolicy::insert(Impl* md, int sid, int fd, EventMask mask)
{
    auto& pfds = md->pfds;

    const pollfd pfd{fd, static_cast<short>(mask), 0};
    const auto it = std::lower_bound(pfds.begin(), pfds.end(), pfd, Cmp{});
    if (it != pfds.end() && it->fd == fd) {
        throw std::system_error{sys::makeError(EEXIST), "fd already registered"};
    }
    auto& sids = md->sids;
    const auto jt = sids.begin() + distance(pfds.begin(), it);

    pfds.insert(it, pfd);
    sids.insert(jt, sid);
}

void PollPolicy::update(Impl* md, int fd, EventMask mask)
{
    auto& pfds = md->pfds;

    const pollfd pfd{fd, 0, 0};
    const auto it = std::lower_bound(pfds.begin(), pfds.end(), pfd, Cmp{});
    if (it == pfds.end() || it->fd != fd) {
        throw std::system_error{sys::makeError(ENOENT), "fd is not registered"};
    }
    it->events = mask;
}

void PollPolicy::erase(Impl* md, int fd)
{
    auto& pfds = md->pfds;

    const pollfd pfd{fd, 0, 0};
    const auto it = std::lower_bound(pfds.begin(), pfds.end(), pfd, Cmp{});
    if (it == pfds.end() || it->fd != fd) {
        throw std::system_error{sys::makeError(ENOENT), "fd is not registered"};
    }
    auto& sids = md->sids;
    const auto jt = sids.begin() + distance(pfds.begin(), it);

    pfds.erase(it);
    sids.erase(jt);
}

int PollPolicy::wait(Impl* md, Event* buf, std::size_t size, int timeout)
{
    assert(buf && size > 0);
    auto& pfds = md->pfds;
    auto& sids = md->sids;

    int n{sys::poll(&pfds[0], pfds.size(), timeout)};
    n = std::min<int>(n, size);

    const auto* end = buf + n;
    for (std::size_t i{0}; buf != end && i < pfds.size(); ++i) {
        const auto& pfd = pfds[i];
        const auto sid = sids[i];
        if (pfd.revents) {
            buf->events = pfd.revents;
            buf->data.u64 = static_cast<std::uint64_t>(sid) << 32 | pfd.fd;
            ++buf;
        }
    }
    return n;
}

} // namespace swirly