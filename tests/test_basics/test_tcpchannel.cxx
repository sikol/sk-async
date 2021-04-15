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

#include <stop_token>

#include <catch.hpp>

#include <sk/net/address.hxx>
#include <sk/net/tcpchannel.hxx>
#include <sk/reactor.hxx>
#include <sk/wait.hxx>

TEST_CASE("tcp_endpoint_resolver", "[resolver][tcpchannel]")
{
    sk::net::tcp_endpoint_system_resolver res;

    auto eps = wait(res.async_resolve("localhost", "http"));
    REQUIRE(eps);

    std::set<sk::net::tcp_endpoint> addrs;
    std::ranges::copy(*eps, std::inserter(addrs, addrs.end()));

    REQUIRE(addrs.size() == 2);
    auto &first = *addrs.begin();
    auto &second = *(++addrs.begin());

    if (str(first) == "127.0.0.1:80")
        REQUIRE(*str(second) == "[::1]:80");
    else if (str(first) == "[::1]:80")
        REQUIRE(*str(second) == "127.0.0.1:80");
}

namespace {

    auto cancel_task(sk::net::tcpserverchannel &chnl,
                     std::stop_token token,
                     bool &ok,
                     sk::event &evt) -> sk::task<void>
    {
        auto ret = co_await chnl.async_accept(token);
        REQUIRE(!ret);
        REQUIRE(ret.error() == sk::error::cancelled);

        ok = true;
        evt.signal();
    }

} // anonymous namespace

TEST_CASE("tcpserverchannel cancellation", "[tcpchannel]")
{
    std::stop_source ss;
    bool ok = false;
    sk::event evt;

    auto ep = sk::net::make_tcp_endpoint("::1", 5358);
    REQUIRE(ep);

    auto server = sk::net::tcpserverchannel::listen(*ep);
    REQUIRE(server);

    auto reactor = sk::weak_reactor_handle::get();
    auto *xer = reactor->get_system_executor();
    wait(co_detach(cancel_task(*server, ss.get_token(), ok, evt), xer));

    ss.request_stop();

    evt.wait();
    REQUIRE(ok);

    wait(server->async_close());
}

TEST_CASE("tcpserverchannel immediate cancellation", "[tcpchannel]")
{
    std::stop_source ss;
    bool ok = false;
    sk::event evt;

    ss.request_stop();

    auto ep = sk::net::make_tcp_endpoint("::1", 5358);
    REQUIRE(ep);

    auto server = sk::net::tcpserverchannel::listen(*ep);
    REQUIRE(server);

    auto reactor = sk::weak_reactor_handle::get();
    auto *xer = reactor->get_system_executor();
    wait(co_detach(cancel_task(*server, ss.get_token(), ok, evt), xer));

    evt.wait();
    REQUIRE(ok);

    wait(server->async_close());
}
