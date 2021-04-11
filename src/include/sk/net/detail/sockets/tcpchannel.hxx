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

#ifndef SK_NET_DETAIL_SOCKETS_TCPCHANNEL_HXX_INCLUDED
#define SK_NET_DETAIL_SOCKETS_TCPCHANNEL_HXX_INCLUDED

#include <cstddef>

#include <sk/async_invoke.hxx>
#include <sk/channel/concepts.hxx>
#include <sk/detail/safeint.hxx>
#include <sk/expected.hxx>
#include <sk/net/address.hxx>
#include <sk/net/detail/sockets/sockets.hxx>
#include <sk/task.hxx>

namespace sk::net {
    /*************************************************************************
     *
     * tcp_endpoint: an IPv4 or IPv6 address and a port.
     *
     */

    class tcp_endpoint {
    public:
        using address_type = unspecified_address;
        using const_address_type = unspecified_address const;
        using port_type = std::uint16_t;

    private:
        address_type _address;
        port_type _port = 0;

    public:
        tcp_endpoint() noexcept = default;
        tcp_endpoint(address_type const &addr, port_type port) noexcept
            : _address(addr), _port(port)
        {
        }

        [[nodiscard]] auto address() const noexcept -> const_address_type &
        {
            return _address;
        }

        [[nodiscard]] auto address() noexcept -> address_type &
        {
            return _address;
        }

        [[nodiscard]] auto port() const noexcept -> port_type
        {
            return _port;
        }

        auto port(port_type p) noexcept -> port_type
        {
            return std::exchange(_port, p);
        }

        auto as_sockaddr_storage() const noexcept -> sockaddr_storage
        {
            sockaddr_storage ret{};

            std::visit(
                overload{[&](inet_family::address_type const &a) -> void {
                             auto *sin = reinterpret_cast<sockaddr_in *>(&ret);
                             sin->sin_family = AF_INET;
                             sin->sin_port = htons(_port);
                             std::memcpy(&sin->sin_addr,
                                         &a.bytes[0],
                                         sizeof(sin->sin_addr));
                         },
                         [&](inet6_family::address_type const &a) -> void {
                             auto *sin = reinterpret_cast<sockaddr_in6 *>(&ret);
                             sin->sin6_family = AF_INET6;
                             sin->sin6_port = htons(_port);
                             std::memcpy(&sin->sin6_addr,
                                         &a.bytes[0],
                                         sizeof(sin->sin6_addr));
                         },
                         [](auto const &) -> void {
                             // Should never reach here.
                             std::abort();
                         }},
                _address.value());

            return ret;
        }
    };

    inline bool operator==(tcp_endpoint const &a,
                           tcp_endpoint const &b) noexcept
    {
        return a.address() == b.address() && a.port() == b.port();
    }

    inline bool operator<(tcp_endpoint const &a, tcp_endpoint const &b) noexcept
    {
        if (a.address() < b.address())
            return true;

        if (a.address() == b.address())
            return a.port() < b.port();
        else
            return false;
    }

    inline auto str(tcp_endpoint const &ep) -> std::string
    {
        switch (tag(ep.address())) {
        case inet_family::tag:
            return *str(ep.address()) + ":" + std::to_string(ep.port());
        case inet6_family::tag:
            return "[" + *str(ep.address()) + "]:" + std::to_string(ep.port());
        default:
            std::abort();
        }
    }

    template <address_family af>
    auto make_tcp_endpoint(address<af> const &,
                           tcp_endpoint::port_type) noexcept
        -> expected<tcp_endpoint, std::error_code> = delete;

    template <>
    [[nodiscard]] inline auto
    make_tcp_endpoint(address<unspecified_family> const &addr,
                      tcp_endpoint::port_type port) noexcept
        -> expected<tcp_endpoint, std::error_code>
    {
        switch (tag(addr)) {
        case inet_family::tag:
        case inet6_family::tag:
            return tcp_endpoint(addr, port);

        default:
            return make_unexpected(
                std::make_error_code(std::errc::address_family_not_supported));
        }
    }

    template <>
    [[nodiscard]] inline auto
    make_tcp_endpoint(address<inet_family> const &addr,
                      tcp_endpoint::port_type port) noexcept
        -> expected<tcp_endpoint, std::error_code>
    {
        return make_tcp_endpoint<unspecified_family>(
            *address_cast<unspecified_address>(addr), port);
    }

    template <>
    [[nodiscard]] inline auto
    make_tcp_endpoint(address<inet6_family> const &addr,
                      tcp_endpoint::port_type port) noexcept
        -> expected<tcp_endpoint, std::error_code>
    {
        return make_tcp_endpoint(*address_cast<unspecified_address>(addr),
                                 port);
    }

    inline auto make_tcp_endpoint(std::string_view str,
                                  tcp_endpoint::port_type port) noexcept
        -> expected<tcp_endpoint, std::error_code>
    {
        auto addr = make_address(str);
        if (!addr)
            return make_unexpected(addr.error());

        return make_tcp_endpoint(*addr, port);
    }

    inline auto make_tcp_endpoint(sockaddr_in const *sin) noexcept
        -> expected<tcp_endpoint, std::error_code>
    {
        return make_tcp_endpoint(make_inet_address(sin->sin_addr.s_addr),
                                 ntohs(sin->sin_port));
    }

    inline auto make_tcp_endpoint(sockaddr_in6 const *sin) noexcept
        -> expected<tcp_endpoint, std::error_code>
    {
        return make_tcp_endpoint(make_inet6_address(sin->sin6_addr),
                                 ntohs(sin->sin6_port));
    }

    inline auto make_tcp_endpoint(sockaddr const *sa,
                                  socklen_t addrlen) noexcept
        -> expected<tcp_endpoint, std::error_code>
    {
        SK_CHECK(addrlen > 0, "invalid address length");

        switch (sa->sa_family) {
        case AF_INET:
            if (addrlen < static_cast<socklen_t>(sizeof(sockaddr_in)))
                return make_unexpected(
                    std::make_error_code(std::errc::invalid_argument));

            return make_tcp_endpoint(reinterpret_cast<sockaddr_in const *>(sa));

        case AF_INET6:
            if (addrlen < static_cast<socklen_t>(sizeof(sockaddr_in6)))
                return make_unexpected(
                    std::make_error_code(std::errc::invalid_argument));

            return make_tcp_endpoint(
                reinterpret_cast<sockaddr_in6 const *>(sa));

        default:
            return make_unexpected(
                std::make_error_code(std::errc::address_family_not_supported));
        }
    }

    namespace detail {

        struct addrinfo_tcp_endpoint_transform {
            using result_type = tcp_endpoint;

            auto operator()(addrinfo **ai) const noexcept
                -> std::optional<result_type>
            {
                while (*ai != nullptr) {
                    auto ep = make_tcp_endpoint(
                        (*ai)->ai_addr,
                        static_cast<socklen_t>((*ai)->ai_addrlen));
                    if (ep)
                        return *ep;

                    *ai = (*ai)->ai_next;
                }

                return {};
            }
        };

    } // namespace detail

    using tcp_endpoint_system_resolver =
        detail::basic_system_resolver<unspecified_family,
                                      detail::addrinfo_tcp_endpoint_transform,
                                      SOCK_STREAM,
                                      IPPROTO_TCP>;

    class tcpchannel : public detail::streamsocket<SOCK_STREAM, IPPROTO_TCP> {
        using socket_type = detail::streamsocket<SOCK_STREAM, IPPROTO_TCP>;

    public:
        tcpchannel() = default;

        explicit tcpchannel(sk::net::detail::native_socket_type &&fd)
            : socket_type(std::move(fd))
        {
        }

        tcpchannel(tcpchannel &&other) noexcept : socket_type(std::move(other))
        {
        }

        auto operator=(tcpchannel &&other) noexcept -> tcpchannel &
        {
            if (&other != this)
                socket_type::operator=(std::move(other));
            return *this;
        }

        ~tcpchannel() = default;

        tcpchannel(tcpchannel const &) = delete;

        auto operator=(tcpchannel const &) -> tcpchannel & = delete;

        [[nodiscard]] auto connect(tcp_endpoint const &addr)
            -> expected<void, std::error_code>
        {
            auto ss = addr.as_sockaddr_storage();
            socklen_t len;

            switch (ss.ss_family) {
            case AF_INET:
                len = sizeof(sockaddr_in);
                break;

            case AF_INET6:
                len = sizeof(sockaddr_in6);
                break;

            default:
                return make_unexpected(std::make_error_code(
                    std::errc::address_family_not_supported));
            }

            return socket_type::_connect(
                ss.ss_family, reinterpret_cast<sockaddr *>(&ss), len);
        }

        [[nodiscard]] auto async_connect(tcp_endpoint &addr)
            -> task<expected<void, std::error_code>>
        {
            auto ss = addr.as_sockaddr_storage();
            socklen_t len;

            switch (ss.ss_family) {
            case AF_INET:
                len = sizeof(sockaddr_in);
                break;

            case AF_INET6:
                len = sizeof(sockaddr_in6);
                break;

            default:
                co_return make_unexpected(std::make_error_code(
                    std::errc::address_family_not_supported));
            }

            co_return co_await socket_type::_async_connect(
                ss.ss_family, reinterpret_cast<sockaddr *>(&ss), len);
        }
    };

    static_assert(seqchannel<tcpchannel>);

} // namespace sk::net

#endif // SK_NET_DETAIL_SOCKETS_TCPCHANNEL_HXX_INCLUDED
