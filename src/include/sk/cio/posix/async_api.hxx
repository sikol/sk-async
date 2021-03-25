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

#ifndef SK_CIO_POSIX_ASYNC_API_HXX_INCLUDED
#define SK_CIO_POSIX_ASYNC_API_HXX_INCLUDED

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include <system_error>

#include <sk/cio/expected.hxx>
#include <sk/cio/task.hxx>

namespace sk::cio::posix {

    /*************************************************************************
     *
     * POSIX async API.
     *
     */

    auto async_fd_open(char const *path, int flags, int mode = 0777)
        -> task<expected<int, std::error_code>>;

    auto async_fd_recv(int fd, void *buf, std::size_t n, int flags)
        -> task<expected<ssize_t, std::error_code>>;

    auto async_fd_send(int fd, void const *buf, std::size_t n, int flags)
        -> task<expected<ssize_t, std::error_code>>;

    auto async_fd_connect(int, sockaddr const *, socklen_t)
        -> task<expected<void, std::error_code>>;

    auto async_fd_accept(int, sockaddr *addr, socklen_t *)
        -> task<expected<int, std::error_code>>;

} // namespace sk::cio::posix

#endif // SK_CIO_POSIX_ASYNC_API_HXX_INCLUDED