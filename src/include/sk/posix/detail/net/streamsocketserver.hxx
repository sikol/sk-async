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

#ifndef SK_CIO_POSIX_NET_STREAMSOCKETSERVER_HXX_INCLUDED
#define SK_CIO_POSIX_NET_STREAMSOCKETSERVER_HXX_INCLUDED

#include <sys/types.h>
#include <sys/socket.h>

#include <cstddef>
#include <system_error>

#include <sk/channel/concepts.hxx>
#include <sk/expected.hxx>
#include <sk/net/address.hxx>
#include <sk/posix/error.hxx>
#include <sk/posix/fd.hxx>
#include <sk/task.hxx>

namespace sk::posix::detail {

    template <typename server_type,
              seqchannel channel_type,
              int type,
              int protocol>
    class streamsocketserver {
    protected:
        unique_fd _fd;

        // address_family is only used on Win32
        explicit streamsocketserver(unique_fd &&, int /*address_family*/);
        streamsocketserver(streamsocketserver &&) noexcept = default;
        auto operator=(streamsocketserver &&) noexcept
            -> streamsocketserver & = default;
        ~streamsocketserver();

        [[nodiscard]] static auto
        _listen(int af, sockaddr const *addr, socklen_t)
            -> expected<server_type, std::error_code>;

    public:
        using value_type = std::byte;

        streamsocketserver(streamsocketserver const &) = delete;
        auto operator=(streamsocketserver const &)
            -> streamsocketserver & = delete;

        [[nodiscard]] auto is_open() const -> bool;

        [[nodiscard]] auto async_accept()
            -> task<expected<channel_type, std::error_code>>;

        [[nodiscard]] auto accept() -> expected<channel_type, std::error_code>;

        [[nodiscard]] auto async_close()
            -> task<expected<void, std::error_code>>;

        [[nodiscard]] auto close() -> expected<void, std::error_code>;
    };

    /*************************************************************************
     * streamsocketserver::streamsocketserver()
     */

    template <typename server_type,
              seqchannel channel_type,
              int type,
              int protocol>
    streamsocketserver<server_type, channel_type, type, protocol>::
        streamsocketserver(unique_fd &&sock, int /*address_family*/)
        : _fd(std::move(sock))
    {
    }

    /*************************************************************************
     * streamsocketserver::~streamsocketserver()
     */

    template <typename server_type,
              seqchannel channel_type,
              int type,
              int protocol>
    streamsocketserver<server_type, channel_type, type, protocol>::
        ~streamsocketserver()
    {
        if (_fd)
            reactor_handle::get_global_reactor().deassociate_fd(_fd.value());
    }

    /*************************************************************************
     * streamsocketserver::is_open()
     */

    template <typename server_type,
              seqchannel channel_type,
              int type,
              int protocol>
    auto
    streamsocketserver<server_type, channel_type, type, protocol>::is_open()
        const -> bool
    {
        return _fd;
    }

    /*************************************************************************
     * streamsocketserver::listen()
     */

    template <typename server_type,
              seqchannel channel_type,
              int type,
              int protocol>
    auto streamsocketserver<server_type, channel_type, type, protocol>::_listen(
        int af, sockaddr const *addr, socklen_t addrlen)
        -> expected<server_type, std::error_code>
    {
        int listener = ::socket(af, type, protocol);

        if (listener == -1)
            return make_unexpected(sk::posix::get_errno());

        unique_fd listener_(listener);

#ifdef SK_CIO_PLATFORM_HAS_AF_UNIX
        if (af != AF_UNIX) {
#endif
            int one = 1, ret;
            ret = ::setsockopt(listener,
                               SOL_SOCKET,
                               SO_REUSEADDR,
                               reinterpret_cast<char const *>(&one),
                               sizeof(one));
            if (ret)
                return make_unexpected(sk::posix::get_errno());
#ifdef SK_CIO_PLATFORM_HAS_AF_UNIX
        }
#endif

        auto ret = ::bind(listener, addr, addrlen);
        if (ret == -1)
            return make_unexpected(sk::posix::get_errno());

        ret = ::listen(listener, SOMAXCONN);
        if (ret == -1)
            return make_unexpected(sk::posix::get_errno());

        reactor_handle::get_global_reactor().associate_fd(listener);

        return server_type{std::move(listener_), af};
    }

    /*************************************************************************
     * streamsocketserver::close()
     */
    template <typename server_type,
              seqchannel channel_type,
              int type,
              int protocol>
    auto streamsocketserver<server_type, channel_type, type, protocol>::close()
        -> expected<void, std::error_code>
    {
        if (!is_open())
            return make_unexpected(error::channel_not_open);

        auto err = _fd.close();
        if (err)
            return make_unexpected(err);
        return {};
    }

    /*************************************************************************
     * streamsocketserver::async_close()
     */
    template <typename server_type,
              seqchannel channel_type,
              int type,
              int protocol>
    auto
    streamsocketserver<server_type, channel_type, type, protocol>::async_close()
        -> task<expected<void, std::error_code>>
    {
        auto err = co_await async_invoke([&] { return _fd.close(); });

        if (err)
            co_return make_unexpected(err);

        co_return {};
    }

    /*************************************************************************
     * streamsocketserver::async_accept()
     */
    template <typename server_type,
              seqchannel channel_type,
              int type,
              int protocol>
    auto streamsocketserver<server_type, channel_type, type, protocol>::
        async_accept() -> task<expected<channel_type, std::error_code>>
    {
        auto client =
            co_await sk::posix::async_fd_accept(*_fd, nullptr, nullptr);
        if (!client)
            co_return make_unexpected(client.error());

        unique_fd client_(*client);

        reactor_handle::get_global_reactor().associate_fd(*client);
        co_return channel_type{std::move(client_)};
    }

    /*************************************************************************
     * streamsocketserver::accept()
     */
    template <typename server_type,
              seqchannel channel_type,
              int type,
              int protocol>
    auto streamsocketserver<server_type, channel_type, type, protocol>::accept()
        -> expected<channel_type, std::error_code>
    {
        auto client = ::accept(*_fd, nullptr, nullptr);
        if (client < 0)
            return make_unexpected(get_errno());

        unique_fd client_(client);

        reactor_handle::get_global_reactor().associate_fd(client);
        return channel_type{std::move(client_)};
    }

} // namespace sk::posix::detail

#endif // SK_CIO_POSIX_NET_STREAMSOCKETSERVER_HXX_INCLUDED
