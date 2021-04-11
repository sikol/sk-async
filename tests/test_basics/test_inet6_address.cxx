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

TEST_CASE("inet6_address: make_inet6_address", "[inet6_address][address]")
{
    auto addr = make_address<inet6_family>("::1");
    REQUIRE(addr);
    REQUIRE(tag(*addr) == inet6_family::tag);

    auto s = str(*addr);
    REQUIRE(s);
    REQUIRE(*s == "::1");

    addr = make_address<inet6_family>("::");
    REQUIRE(addr);
    s = str(*addr);
    REQUIRE(s);
    REQUIRE(*s == "::");

    addr = make_address<inet6_family>("1::2::3");
    REQUIRE(!addr);
    addr = make_address<inet6_family>("127.0.0.1");
    REQUIRE(!addr);

    addr = make_address<inet6_family>("1::");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "1::");

    addr = make_address<inet6_family>("2000::1");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "2000::1");

    addr = make_address<inet6_family>("2001:db0:ffff::3:4");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "2001:db0:ffff::3:4");

    addr = make_address<inet6_family>("2001:db0:ffff::");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "2001:db0:ffff::");

    addr = make_address<inet6_family>("::fffe:1:2");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "::fffe:1:2");

    addr = make_address<inet6_family>("::ffff:127.0.0.1");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "::ffff:127.0.0.1");

    addr = make_address<inet6_family>("::ffff:34.89.21.4");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "::ffff:34.89.21.4");

    addr = make_address<inet6_family>("::10.254.67.131");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "::10.254.67.131");

    addr = make_address<inet6_family>("2001:db0::192.168.67.131");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "2001:db0::c0a8:4383");

    addr = make_address<inet6_family>("2001:db0:0:1234:5678:abcd:1f2e:3d4c");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "2001:db0:0:1234:5678:abcd:1f2e:3d4c");

    addr = make_address<inet6_family>("2001:db0:0:1234:5678:abcd:1f2e:3d4c:1");
    REQUIRE(!addr);

    addr = make_address<inet6_family>("2001:db0:0:1234:5678:abcd:1f2e:3d4c::");
    REQUIRE(!addr);

    addr = make_address<inet6_family>("::2001:db0:0:1234:5678:abcd:1f2e:3d4c");
    REQUIRE(!addr);
}

TEST_CASE("inet6_address: address_cast to unspecified_address",
          "[inet6_address][address]")
{
    auto inet = make_address<inet6_family>("::1");
    REQUIRE(inet);

    auto unspec = address_cast<unspecified_address>(*inet);
    REQUIRE(unspec);
    REQUIRE(tag(*unspec) == inet6_family::tag);

    auto s = str(*unspec);
    if (!s) {
        INFO(s.error().message());
        REQUIRE(s);
    }
    REQUIRE(*s == "::1");

    auto inet2 = address_cast<inet6_address>(*unspec);
    REQUIRE(tag(*inet2) == inet6_family::tag);

    s = str(*inet2);
    REQUIRE(s);
    REQUIRE(*s == "::1");
}

TEST_CASE("inet6_address: make_unspecified_zero_address",
          "[inet6_address][address]")
{
    auto unspec_zero = make_unspecified_zero_address(inet6_family::tag);
    REQUIRE(*str(*unspec_zero) == "::");
    REQUIRE(unspec_zero);

    REQUIRE(tag(*unspec_zero) == inet6_family::tag);

    auto inet_zero = address_cast<inet6_address>(*unspec_zero);
    REQUIRE(inet_zero);
    REQUIRE(tag(*inet_zero) == inet6_family::tag);
    REQUIRE(*str(*inet_zero) == "::");
}

TEST_CASE("inet6_address: make_address without port",
          "[inet6_address][address]")
{
    auto addr = make_address("::1");
    REQUIRE(addr);
    REQUIRE(*str(*addr) == "::1");

    auto iaddr = address_cast<inet6_address>(*addr);
    REQUIRE(iaddr);
    REQUIRE(*str(*iaddr) == "::1");
}

TEST_CASE("inet6_address: streaming output", "[inet6_address][address]")
{
    auto addr = make_address("::1");
    REQUIRE(addr);

    std::ostringstream strm;
    strm << *addr;
    REQUIRE(strm.str() == "::1");
}

TEST_CASE("inet6_address: resolve", "[inet6_address][address]")
{
    sk::net::system_resolver<inet6_family> res;
    std::vector<inet6_address> addrs;
    auto ret = wait(res.async_resolve(std::back_inserter(addrs), "localhost"));
    REQUIRE(ret);

    // >= because on some platforms localhost has multiple aliases.
    REQUIRE(addrs.size() >= 1);

    auto &first = addrs.front();

    REQUIRE(tag(first) == inet6_family::tag);
    REQUIRE(*str(first) == "::1");
}
