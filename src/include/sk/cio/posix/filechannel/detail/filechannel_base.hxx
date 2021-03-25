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

#ifndef SK_CIO_POSIX_CHANNEL_FILECHANNEL_BASE_HXX_INCLUDED
#define SK_CIO_POSIX_CHANNEL_FILECHANNEL_BASE_HXX_INCLUDED

#include <sys/types.h>
#include <fcntl.h>

#include <filesystem>
#include <system_error>

#include <sk/buffer/buffer.hxx>
#include <sk/cio/async_invoke.hxx>
#include <sk/cio/channel/concepts.hxx>
#include <sk/cio/detail/safeint.hxx>
#include <sk/cio/error.hxx>
#include <sk/cio/filechannel/filechannel.hxx>
#include <sk/cio/posix/async_api.hxx>
#include <sk/cio/posix/error.hxx>
#include <sk/cio/posix/fd.hxx>
#include <sk/cio/reactor.hxx>
#include <sk/cio/task.hxx>
#include <sk/cio/types.hxx>

namespace sk::cio::posix::detail {

    /*************************************************************************
     *
     * filechannel_base: base class for file channels.
     *
     */

    struct filechannel_base {
        using value_type = std::byte;

        filechannel_base(filechannel_base const &) = delete;
        filechannel_base(filechannel_base &&) noexcept = default;
        filechannel_base &operator=(filechannel_base const &) = delete;
        filechannel_base &operator=(filechannel_base &&) noexcept = default;

        /*
         * Test if this channel has been opened.
         */
        auto is_open() const -> bool;

        [[nodiscard]] auto async_close()
            -> task<expected<void, std::error_code>>;

        [[nodiscard]] auto close() -> expected<void, std::error_code>;

    protected:
        [[nodiscard]] auto _async_open(std::filesystem::path const &,
                                       fileflags_t)
            -> task<expected<void, std::error_code>>;

        [[nodiscard]] auto _open(std::filesystem::path const &, fileflags_t)
            -> expected<void, std::error_code>;

        [[nodiscard]] auto _make_flags(fileflags_t flags, int &open_flags)
            -> bool;

        filechannel_base() = default;
        ~filechannel_base() = default;

        posix::unique_fd _fd;
    };

    /*************************************************************************
     * filechannel_base::_make_flags()
     */
    inline auto filechannel_base::_make_flags(fileflags_t flags,
                                              int &open_flags) -> bool
    {

        // Must specify either read or write.
        if ((flags & (fileflags::read | fileflags::write)) == 0)
            return false;

        // Read access only
        if ((flags & fileflags::read) && !(flags & fileflags::write)) {
            // These flags are not valid for reading.
            if (flags &
                (fileflags::trunc | fileflags::append | fileflags::create_new))
                return false;

            open_flags = O_RDONLY;
            return true;
        }

        // Write access or read-write access
        if (flags & fileflags::write) {
            // Must specify either create_new or open_existing (or both).
            if (!(flags & (fileflags::create_new | fileflags::open_existing)))
                return false;

            if (flags & fileflags::read)
                open_flags = O_RDWR;
            else
                open_flags = O_WRONLY;

            // Must create a new file.
            if ((flags & fileflags::create_new) &&
                !(flags & fileflags::open_existing))
                open_flags |= O_CREAT | O_EXCL;
            // Can create a new file or open an existing one.
            else if ((flags & fileflags::create_new) &&
                     (flags & fileflags::open_existing)) {
                if (flags & fileflags::trunc)
                    open_flags |= O_CREAT | O_TRUNC;
                else
                    open_flags |= O_CREAT;
                // Can only open an existing file.
            } else if (!(flags & fileflags::create_new) &&
                       (flags & fileflags::open_existing)) {
                if (flags & fileflags::trunc)
                    open_flags |= O_TRUNC;
                // else
                //    open_flags |= 0;
            }

            return true;
        }

        return false;
    }

    /*************************************************************************
     * filechannel_base::async_open()
     */

    inline auto filechannel_base::_async_open(std::filesystem::path const &path,
                                              fileflags_t flags)
        -> task<expected<void, std::error_code>>
    {
        if (is_open())
            co_return make_unexpected(cio::error::channel_already_open);

        int open_flags = 0;

        if (!_make_flags(flags, open_flags))
            co_return make_unexpected(cio::error::filechannel_invalid_flags);

        auto native_path = path.native();
        if (native_path.find('\0') != native_path.npos)
            co_return make_unexpected(make_error(ENOENT));

        auto fd = co_await async_fd_open(native_path.c_str(), open_flags, 0777);

        if (!fd)
            co_return make_unexpected(fd.error());

        _fd.assign(*fd);
        co_return {};
    }

    /*************************************************************************
     * filechannel_base::open()
     */

    inline auto filechannel_base::_open(std::filesystem::path const &path,
                                        fileflags_t flags)
        -> expected<void, std::error_code>
    {
        if (is_open())
            return make_unexpected(cio::error::channel_already_open);

        int open_flags = 0;

        if (!_make_flags(flags, open_flags))
            return make_unexpected(cio::error::filechannel_invalid_flags);

        auto native_path = path.native();
        if (native_path.find('\0') != native_path.npos)
            return make_unexpected(make_error(ENOENT));

        auto fd = ::open(native_path.c_str(), open_flags, 0777);

        if (fd == -1)
            return make_unexpected(get_errno());

        _fd.assign(fd);
        return {};
    }

    /*************************************************************************
     * filechannel_base::is_open()
     */
    inline auto filechannel_base::is_open() const -> bool
    {
        return static_cast<bool>(_fd);
    }

    /*************************************************************************
     * filechannel_base::close()
     */
    inline auto filechannel_base::close() -> expected<void, std::error_code>
    {
        if (!is_open())
            return make_unexpected(cio::error::channel_not_open);

        auto err = _fd.close();
        if (err)
            return make_unexpected(err);
        return {};
    }

    /*************************************************************************
     * filechannel_base::async_close()
     */
    inline auto filechannel_base::async_close()
        -> task<expected<void, std::error_code>>
    {
        auto err = co_await async_invoke([&] { return _fd.close(); });

        if (err)
            co_return make_unexpected(err);

        co_return {};
    }

} // namespace sk::cio::posix::detail

#endif // SK_CIO_WIN32_CHANNEL_DAFILECHANNEL_BASE_HXX_INCLUDED
