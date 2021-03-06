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
#include "IpAddress.hpp"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace swirly;

BOOST_AUTO_TEST_SUITE(IpAddressSuite)

BOOST_AUTO_TEST_CASE(ParseEndpointV4Case)
{
    const auto ep = parse_endpoint<Tcp>("192.168.1.2:443");

    BOOST_TEST(ep.address().to_string() == "192.168.1.2");
    BOOST_TEST(ep.port() == 443);

    stringstream ss;
    ss << ep;
    BOOST_TEST(ss.str() == "192.168.1.2:443");
}

BOOST_AUTO_TEST_CASE(ParseEndpointV6Case)
{
    const auto ep = parse_endpoint<Udp>("[2001:db8:85a3:8d3:1319:8a2e:370:7348]:443");

    BOOST_TEST(ep.address().to_string() == "2001:db8:85a3:8d3:1319:8a2e:370:7348");
    BOOST_TEST(ep.port() == 443);

    stringstream ss;
    ss << ep;
    BOOST_TEST(ss.str() == "[2001:db8:85a3:8d3:1319:8a2e:370:7348]:443");
}

BOOST_AUTO_TEST_SUITE_END()
