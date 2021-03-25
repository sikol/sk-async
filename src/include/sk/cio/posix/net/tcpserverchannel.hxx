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

#ifndef SK_CIO_POSIX_NET_TCPSERVERCHANNEL_HXX_INCLUDED
#define SK_CIO_POSIX_NET_TCPSERVERCHANNEL_HXX_INCLUDED

#include <sys/types.h>
#include <sys/socket.h>

#include <cstddef>
#include <system_error>

#include <sk/cio/channel/concepts.hxx>
#include <sk/cio/expected.hxx>
#include <sk/cio/net/address.hxx>
#include <sk/cio/task.hxx>
#include <sk/cio/posix/fd.hxx>
#include <sk/cio/posix/net/tcpchannel.hxx>
#include <sk/cio/posix/error.hxx>

namespace sk::cio::posix::net {

    struct tcpserverchannel {
        using value_type = std::byte;

        tcpserverchannel(unique_fd &&);
        tcpserverchannel(tcpserverchannel const &) = delete;
        tcpserverchannel(tcpserverchannel &&) noexcept = default;
        tcpserverchannel &operator=(tcpserverchannel const &) = delete;
        tcpserverchannel &operator=(tcpserverchannel &&) noexcept = default;
        ~tcpserverchannel() = default;

        /*
         * Test if this channel has been opened.
         */
        auto is_open() const -> bool;

        /*
         * Create a TCP server channel listening on a local address.
         */
        [[nodiscard]] static auto listen(cio::net::address const &addr)
            -> expected<tcpserverchannel, std::error_code>;

        /*
         * Accept a connection from a remote host.
         */
        [[nodiscard]] auto async_accept()
            -> task<expected<tcpchannel, std::error_code>>;

        [[nodiscard]] auto accept() -> expected<tcpchannel, std::error_code>;

        /*
         * Close the socket.
         */
        [[nodiscard]] auto async_close()
            -> task<expected<void, std::error_code>>;

        [[nodiscard]] auto close() -> expected<void, std::error_code>;

    private:
        unique_fd _fd;
    };

    /*************************************************************************
     * tcpchannel::tcpchannel()
     */

    inline tcpserverchannel::tcpserverchannel(unique_fd &&sock)
        : _fd(std::move(sock))
    {
    }

    /*************************************************************************
     * tcpserverchannel::is_open()
     */

    inline auto tcpserverchannel::is_open() const -> bool
    {
        return _fd;
    }

    /*************************************************************************
     * tcpserverchannel::bind()
     */

    inline auto tcpserverchannel::listen(cio::net::address const &addr)
        -> expected<tcpserverchannel, std::error_code>
    {
        int listener;
        listener = ::socket(addr.address_family(), SOCK_STREAM, IPPROTO_TCP);

        if (listener == -1)
            return make_unexpected(get_errno());

        unique_fd listener_(listener);

        auto ret =
            ::bind(listener,
                   reinterpret_cast<sockaddr const *>(&addr.native_address),
                   addr.native_address_length);
        if (ret == -1)
            return make_unexpected(get_errno());

        ret = ::listen(listener, SOMAXCONN);
        if (ret == -1)
            return make_unexpected(get_errno());

        reactor_handle::get_global_reactor().associate_fd(listener);

        return tcpserverchannel{std::move(listener_)};
    }

    /*************************************************************************
     * tcpserverchannel::close()
     */
    inline auto tcpserverchannel::close() -> expected<void, std::error_code>
    {

        if (!is_open())
            return make_unexpected(cio::error::channel_not_open);

        auto err = _fd.close();
        if (err)
            return make_unexpected(err);
        return {};
    }

    /*************************************************************************
     * tcpserverchannel::async_close()
     */
    inline auto tcpserverchannel::async_close()
        -> task<expected<void, std::error_code>>
    {

        auto err =
            co_await async_invoke([&] { return _fd.close(); });

        if (err)
            co_return make_unexpected(err);

        co_return {};
    }

    /*************************************************************************
     * tcpserverchannel::async_accept()
     */
    inline auto tcpserverchannel::async_accept()
        -> task<expected<tcpchannel, std::error_code>>
    {
        auto client = co_await async_fd_accept(_fd.fd(), nullptr, 0);
        if (!client)
            co_return make_unexpected(client.error());

        unique_fd client_(*client);

        reactor_handle::get_global_reactor().associate_fd(*client);
        co_return tcpchannel{std::move(client_)};
    }

} // namespace sk::cio::posix::net

#endif // SK_CIO_POSIX_NET_TCPSERVERCHANNEL_HXX_INCLUDED
