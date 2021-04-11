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

#ifndef SK_NET_DETAIL_SOCKETS_UNIXCHANNEL_HXX_INCLUDED
#define SK_NET_DETAIL_SOCKETS_UNIXCHANNEL_HXX_INCLUDED

#include <cstddef>
#include <filesystem>

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
     * unix_endpoint: an AF_UNIX path.
     *
     */

    class unix_endpoint {
    public:
        using address_type = unix_address;
        using const_address_type = unix_address const;

    private:
        address_type _address;

    public:
        explicit unix_endpoint(address_type const &addr) : _address(addr) {}

        [[nodiscard]] auto address() const noexcept -> const_address_type &
        {
            return _address;
        }

        [[nodiscard]] auto address() noexcept -> address_type &
        {
            return _address;
        }

        [[nodiscard]] auto as_sockaddr_un() const noexcept -> sockaddr_un
        {
            sockaddr_un sun{};

            auto &chars = _address.value().path;

            sun.sun_family = AF_UNIX;
            static_assert(unix_family::address_size <=
                          sizeof(sockaddr_un::sun_path));
            std::ranges::copy(chars, &sun.sun_path[0]);
            return sun;
        }

        [[nodiscard]] auto as_sockaddr_storage() const noexcept -> sockaddr_storage
        {
            sockaddr_storage ret{};
            auto sun = as_sockaddr_un();

            static_assert(sizeof(ret) >= sizeof(sun));
            std::memcpy(&ret, &sun, sizeof(sun));
            return ret;
        }
    };

    inline auto str(unix_endpoint const &ep) -> std::string
    {
        return *str(ep.address());
    }

    inline auto make_unix_endpoint(unix_address const &addr) noexcept
        -> expected<unix_endpoint, std::error_code>
    {
        return unix_endpoint(addr);
    }

    inline auto make_unix_endpoint(unspecified_address const &addr) noexcept
        -> expected<unix_endpoint, std::error_code>
    {
        auto ua = address_cast<unix_address>(addr);
        if (!ua)
            return make_unexpected(ua.error());
        return unix_endpoint(*ua);
    }

    inline auto make_unix_endpoint(std::filesystem::path const &path) noexcept
        -> expected<unix_endpoint, std::error_code>
    {
        auto addr = make_unix_address(path);
        if (!addr)
            return make_unexpected(addr.error());
        return make_unix_endpoint(*addr);
    }

    inline auto make_unix_endpoint(std::string_view str) noexcept
        -> expected<unix_endpoint, std::error_code>
    {
        auto addr = make_unix_address(str);
        if (!addr)
            return make_unexpected(addr.error());
        return make_unix_endpoint(*addr);
    }

    class unixchannel : public detail::streamsocket<SOCK_STREAM, 0> {
        using socket_type = detail::streamsocket<SOCK_STREAM, 0>;

    public:
        unixchannel() = default;

        explicit unixchannel(sk::net::detail::native_socket_type &&fd)
            : socket_type(std::move(fd))
        {
        }

        unixchannel(unixchannel &&other) noexcept
            : socket_type(std::move(other))
        {
        }

        auto operator=(unixchannel &&other) noexcept -> unixchannel &
        {
            if (&other != this)
                socket_type::operator=(std::move(other));
            return *this;
        }

        ~unixchannel() = default;

        unixchannel(unixchannel const &) = delete;

        auto operator=(unixchannel const &) -> unixchannel & = delete;

        [[nodiscard]] auto connect(unix_endpoint const &ep)
            -> expected<void, std::error_code>
        {
            auto sun = ep.as_sockaddr_un();
            return socket_type::_connect(
                AF_UNIX, reinterpret_cast<sockaddr *>(&sun), sizeof(sun));
        }

        [[nodiscard]] auto async_connect(unix_endpoint const &ep)
            -> task<expected<void, std::error_code>>
        {
            auto sun = ep.as_sockaddr_un();
            co_return co_await socket_type::_async_connect(
                AF_UNIX, reinterpret_cast<sockaddr *>(&sun), sizeof(sun));
        }
    };

    static_assert(seqchannel<unixchannel>);

} // namespace sk::net

#endif // SK_NET_DETAIL_SOCKETS_UNIXCHANNEL_HXX_INCLUDED
