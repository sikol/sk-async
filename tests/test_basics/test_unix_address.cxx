/*
 * Copyright (c) 2019, 2020, 2021 SiKol Ltd.
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <catch.hpp>

#include <sk/net/address.hxx>

using namespace sk::net;

TEST_CASE("unix_address: make_unix_address") {
    auto addr = make_address<unix_family>("/tmp/x.sock");
    REQUIRE(addr);
    REQUIRE(tag(*addr) == unix_family::tag);

    auto s = str(*addr);
    REQUIRE(s);
    REQUIRE(*s == "/tmp/x.sock");
}

TEST_CASE("unix_address: address_cast to unspecified_address") {
    auto uaddr = make_address<unix_family>("/tmp/x.sock");
    REQUIRE(uaddr);

    auto unspec = address_cast<unspecified_address>(*uaddr);
    REQUIRE(unspec);
    REQUIRE(tag(*unspec) == unix_family::tag);

    auto s = str(*unspec);
    if (!s) {
        INFO(s.error().message());
        REQUIRE(s);
    }
    REQUIRE(*s == "/tmp/x.sock");

    auto unix2 = address_cast<unix_address>(*unspec);
    REQUIRE(tag(*unix2) == unix_family::tag);

    s = str(*unix2);
    REQUIRE(s);
    REQUIRE(*s == "/tmp/x.sock");
}

TEST_CASE("unix_address: streaming output") {
    auto addr = make_address<unix_family>("/tmp/x.sock");
    REQUIRE(addr);

    std::ostringstream strm;
    strm << *addr;
    REQUIRE(strm.str() == "/tmp/x.sock");

}
