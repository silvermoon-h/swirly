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

#include <boost/test/unit_test.hpp>

using namespace swirly;

BOOST_AUTO_TEST_SUITE(ExceptionSuite)

BOOST_AUTO_TEST_CASE(ExceptionCase)
{
    const auto ec = std::make_error_code(std::errc::invalid_argument);
    const Exception e{ec, "foo"};
    BOOST_TEST(e.what() == std::string{"foo: "} + std::strerror(EINVAL));
    BOOST_TEST(e.code() == ec);
}

BOOST_AUTO_TEST_SUITE_END()
