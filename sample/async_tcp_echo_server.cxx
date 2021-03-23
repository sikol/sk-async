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

#include <cstdio>
#include <iostream>
#include <ranges>

#include <fmt/core.h>

#include <sk/cio/dtask.hxx>
#include <sk/cio/detach_task.hxx>
#include <sk/cio/net/address.hxx>
#include <sk/cio/net/tcpserverchannel.hxx>
#include <sk/cio/net/tcpchannel.hxx>
#include <sk/cio/reactor.hxx>
#include <sk/buffer/fixed_buffer.hxx>

using namespace sk::cio;

dtask handle_client(net::tcpchannel client) {
    std::cerr << "handle_client() : start\n";
    for (;;) {
        std::cerr << "handle_client() : in loop\n";
        sk::fixed_buffer<std::byte, 1024> buf;

        std::cerr << "handle_client() : wait to read\n";
        auto ret = co_await client.async_read_some(unlimited, buf);
        if (!ret) {
            fmt::print(stderr, "read err: {}\n", ret.error().message());
            co_return;
        }

        std::cerr << "handle_client() : read done\n";
        for (auto &&range : buf.readable_ranges())
            std::cout.write(
                reinterpret_cast<char const *>(std::ranges::data(range)),
                std::ranges::size(range));
        std::cerr << "handle_client() : end loop\n";
    }

    fmt::print(stderr, "handle_client() : return\n");
    co_return;
}

task<void> run(std::string const &addr, std::string const &port) {
    auto netaddr = net::make_address(addr, port);
    if (!netaddr) {
        fmt::print(stderr, "{}:{}: {}\n", addr, port,
                   netaddr.error().message());
        co_return;
    }

    auto server = net::tcpserverchannel::listen(*netaddr);

    for (;;) {
        auto client = co_await server->async_accept();
        if (!client) {
            fmt::print(stderr, "async_accept(): {}\n",
                       client.error().message());
            co_return;
        }

        co_await handle_client(std::move(*client));

        //detach_task(client_task);
    }
}


int main(int argc, char **argv) {
    using namespace std::chrono_literals;

    if (argc != 3) {
        fmt::print(stderr, "usage: {} <address> <port>", argv[0]);
        return 1;
    }

    sk::cio::reactor_handle reactor;
    run(argv[1], argv[2]).wait();
    return 0;
}