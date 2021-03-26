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

#ifndef SK_CIO_NET_ADDRESS_HXX_INCLUDED
#define SK_CIO_NET_ADDRESS_HXX_INCLUDED

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <system_error>

#include <sk/cio/detail/config.hxx>

#if defined(SK_CIO_PLATFORM_WINDOWS)
#    include <sk/cio/win32/windows.hxx>
#elif defined(SK_CIO_PLATFORM_POSIX)
#    include <sys/types.h>
#    include <sys/socket.h>
#    include <netdb.h>
#endif

#include <sk/cio/expected.hxx>
#include <sk/cio/task.hxx>

namespace sk::cio::net {

    /*************************************************************************
     *
     * The resolver error category.
     *
     */
    enum struct resolver_error : int {
        no_error = 0,
    };

} // namespace sk::cio::net

namespace std {

    template <>
    struct is_error_code_enum<sk::cio::net::resolver_error> : true_type {
    };

} // namespace std

namespace sk::cio::net {

    namespace detail {

        struct resolver_errc_category : std::error_category {
            auto name() const noexcept -> char const * final;
            auto message(int c) const -> std::string final;
        };

    } // namespace detail

    auto resolver_errc_category() -> detail::resolver_errc_category const &;
    auto make_error_code(resolver_error e) -> std::error_code;

    /*************************************************************************
     *
     * address: represents the address of a network endpoint; for IP endpoints
     * this will be a hostname and port.
     *
     */
    struct address {
        sockaddr_storage native_address;

        // Some systems provide this in sockaddr, but others don't.
        // For consistency, store it ourselves.
        socklen_t native_address_length;

        int address_family() const
        {
            return native_address.ss_family;
        }

        // Construct an address from a sockaddr.
        address(sockaddr_storage const *ss, socklen_t len)
            : native_address_length(len)
        {
            if (len > sizeof(native_address))
                throw std::domain_error("address is too large");

            std::memcpy(&native_address, ss, len);
        }

        address(sockaddr const *ss, socklen_t len) : native_address_length(len)
        {
            if (len > sizeof(native_address))
                throw std::domain_error("address is too large");

            std::memcpy(&native_address, ss, len);
        }

        // Return the zero address for an address family.
        static auto zero_address(int af) -> expected<address, std::error_code>
        {
            switch (af) {
            case AF_INET: {
                sockaddr_in sin{};
                sin.sin_family = AF_INET;
                return address(reinterpret_cast<sockaddr *>(&sin), sizeof(sin));
            }

            case AF_INET6: {
                sockaddr_in6 sin6{};
                sin6.sin6_family = AF_INET6;
                return address(reinterpret_cast<sockaddr *>(&sin6),
                               sizeof(sin6));
            }

            default:
                return make_unexpected(
                    std::make_error_code(std::errc::address_family_not_supported));
            }
        }
    };

    // Create an address from an address and service string.
    //
    // This does not attempt to resolve either argument, so they should
    // be literal strings.
    //
    // If only host is specified, the host in the return address will be
    // the "any" address.
    auto make_address(std::string const &host, std::string const &service)
        -> expected<address, std::error_code>;

    std::ostream &operator<<(std::ostream &strm, address const &addr);

    /*************************************************************************
     *
     * async_resolve_address(): resolve a hostname to a list of addresses
     * using the operating system's resolver.
     *
     */
    auto async_resolve_address(std::string hostname, std::string port)
        -> task<expected<std::vector<address>, std::error_code>>;

} // namespace sk::cio::net

#endif // SK_CIO_NET_ADDRESS_HXX_INCLUDED
