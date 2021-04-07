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
#include <sk/wait.hxx>

using namespace sk::net;
using sk::expected;

TEST_CASE("inet_address: make_inet_address")
{
    auto addr = make_address<inet_family>("127.0.0.1");
    REQUIRE(addr);
    REQUIRE(tag(*addr) == inet_family::tag);

    auto s = str(*addr);
    REQUIRE(s);
    REQUIRE(*s == "127.0.0.1");

    addr = make_address<inet_family>("0.0.0.0");
    REQUIRE(addr);
    s = str(*addr);
    REQUIRE(s);
    REQUIRE(*s == "0.0.0.0");

    addr = make_address<inet_family>("1.2.3.4.5");
    REQUIRE(!addr);
    addr = make_address<inet_family>("::1");
    REQUIRE(!addr);
}

TEST_CASE("inet_address: address_cast to unspecified_address")
{
    auto inet = make_address<inet_family>("127.0.0.1");
    REQUIRE(inet);

    auto unspec = address_cast<unspecified_address>(*inet);
    REQUIRE(unspec);
    REQUIRE(tag(*unspec) == inet_family::tag);

    auto s = str(*unspec);
    if (!s) {
        INFO(s.error().message());
        REQUIRE(s);
    }
    REQUIRE(*s == "127.0.0.1");

    auto inet2 = address_cast<inet_address>(*unspec);
    REQUIRE(tag(*inet2) == inet_family::tag);

    s = str(*inet2);
    REQUIRE(s);
    REQUIRE(*s == "127.0.0.1");
}

TEST_CASE("inet_address: make_unspecified_zero_address")
{
    auto unspec_zero = make_unspecified_zero_address(inet_family::tag);
    REQUIRE(*str(*unspec_zero) == "0.0.0.0");
    REQUIRE(unspec_zero);

    REQUIRE(tag(*unspec_zero) == inet_family::tag);

    auto inet_zero = address_cast<inet_address>(*unspec_zero);
    REQUIRE(inet_zero);
    REQUIRE(tag(*inet_zero) == inet_family::tag);
    REQUIRE(*str(*inet_zero) == "0.0.0.0");
}

TEST_CASE("inet_address: make_address without port")
{
    auto addr = make_address("127.0.0.1");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "127.0.0.1");

    auto iaddr = address_cast<inet_address>(*addr);
    REQUIRE(iaddr);
    REQUIRE(*str(*iaddr) == "127.0.0.1");
}

TEST_CASE("inet_address: streaming output")
{
    auto addr = make_address("127.0.0.1");
    REQUIRE(addr);

    std::ostringstream strm;
    strm << *addr;
    REQUIRE(strm.str() == "127.0.0.1");
}

TEST_CASE("inet_address: resolve")
{
    sk::net::system_resolver<inet_family> res;
    std::vector<inet_address> addrs;
    auto ret = wait(res.async_resolve(std::back_inserter(addrs), "localhost"));
    REQUIRE(ret);
    REQUIRE(addrs.size() == 1);

    auto &first = addrs.front();

    REQUIRE(tag(first) == inet_family::tag);
    REQUIRE(*str(first) == "127.0.0.1");
}

TEST_CASE("inet_address: compare") {
    auto addr1 = make_inet_address("127.0.0.1");
    auto addr2 = make_inet_address("127.0.0.1");
    auto addr3 = make_inet_address("127.0.0.2");

    REQUIRE(*addr1 == *addr2);
    REQUIRE(*addr1 != *addr3);
}
