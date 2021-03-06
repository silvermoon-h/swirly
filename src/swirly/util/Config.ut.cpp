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
#include "Config.hpp"

#include <boost/test/unit_test.hpp>

#include <map>

using namespace std;
using namespace swirly;

BOOST_AUTO_TEST_SUITE(ConfigSuite)

BOOST_AUTO_TEST_CASE(ParsePairsCase)
{
    const string text{R"(
# comment
  # indented comment
ab
cd=
ef=gh
=ij

kl = mn
 op = qr 
st = = uv =

)"};

    istringstream is{text};
    map<string, string> conf;
    parse_pairs(is, [&conf](const auto& key, const auto& val) { conf.emplace(key, val); });
    BOOST_TEST(conf.size() == 7U);
    BOOST_TEST(conf["ab"] == "");
    BOOST_TEST(conf["cd"] == "");
    BOOST_TEST(conf["ef"] == "gh");
    BOOST_TEST(conf[""] == "ij");
    BOOST_TEST(conf["kl"] == "mn");
    BOOST_TEST(conf["op"] == "qr");
    BOOST_TEST(conf["st"] == "= uv =");
}

BOOST_AUTO_TEST_SUITE_END()
