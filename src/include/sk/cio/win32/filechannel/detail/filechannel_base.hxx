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

#ifndef SK_CIO_WIN32_CHANNEL_FILECHANNEL_BASE_HXX_INCLUDED
#define SK_CIO_WIN32_CHANNEL_FILECHANNEL_BASE_HXX_INCLUDED

#include <filesystem>
#include <system_error>

#include <sk/buffer/buffer.hxx>
#include <sk/cio/detail/safeint.hxx>
#include <sk/cio/async_invoke.hxx>
#include <sk/cio/channel/concepts.hxx>
#include <sk/cio/error.hxx>
#include <sk/cio/filechannel/filechannel.hxx>
#include <sk/cio/reactor.hxx>
#include <sk/cio/task.hxx>
#include <sk/cio/types.hxx>
#include <sk/cio/win32/async_api.hxx>
#include <sk/cio/win32/error.hxx>
#include <sk/cio/win32/handle.hxx>

namespace sk::cio::win32::detail {

    /*************************************************************************
     *
     * filechannel_base: base class for file channels.
     *
     */
    struct filechannel_base
    {
        using value_type = std::byte;
        using native_handle_type = win32::unique_handle;

        filechannel_base(filechannel_base const &) = delete;
        filechannel_base(filechannel_base &&) noexcept = default;
        filechannel_base &operator=(filechannel_base const &) = delete;
        filechannel_base &operator=(filechannel_base &&) noexcept = default;

        /*
         * Test if this channel has been opened.
         */
        auto is_open() const -> bool;

        [[nodiscard]]
        auto async_close()
             -> task<expected<void, std::error_code>>;

        [[nodiscard]]
        auto close()
             -> expected<void, std::error_code>;

    protected:

        [[nodiscard]]
        auto _async_open(std::filesystem::path const &,
                         fileflags_t)
            -> task<expected<void, std::error_code>>;

        [[nodiscard]]
        auto _open(std::filesystem::path const &,
                   fileflags_t)
            -> expected<void, std::error_code>;

        [[nodiscard]]
        auto _make_flags(fileflags_t flags,
                         DWORD &dwDesiredAccess,
                         DWORD &dwCreationDisposition,
                         DWORD &dwShareMode)
             -> bool;

        /*
         * Read data.
         */
        [[nodiscard]]
        auto _async_read_some_at(io_offset_t loc,
                                 std::byte *buffer,
                                 io_size_t nobjs)
        -> task<expected<io_size_t, std::error_code>>;

        [[nodiscard]]
        auto _read_some_at(io_offset_t loc,
                           std::byte *buffer,
                           io_size_t nobjs)
        -> expected<io_size_t, std::error_code>;

        /*
         * Write data.
         */
        [[nodiscard]]
        auto _async_write_some_at(io_offset_t,
                                  std::byte const *,
                                  io_size_t)
            -> task<expected<io_size_t, std::error_code>>;

        [[nodiscard]]
        auto _write_some_at(io_offset_t,
                            std::byte const *,
                            io_size_t)
            -> expected<io_size_t, std::error_code>;

        filechannel_base() = default;
        ~filechannel_base() = default;

        native_handle_type _native_handle;
    };

    /*************************************************************************
     * filechannel_base::_make_flags()
     */
    // clang-format off
    inline auto filechannel_base::_make_flags(
            fileflags_t flags, DWORD &dwDesiredAccess,
            DWORD &dwCreationDisposition, DWORD &dwShareMode)
        -> bool {
        // clang-format on

        // Must specify either read or write.
        if ((flags & (fileflags::read | fileflags::write)) == 0)
            return false;

        // Read access only
        if ((flags & fileflags::read) && !(flags & fileflags::write)) {
            // These flags are not valid for reading.
            if (flags &
                (fileflags::trunc | fileflags::append | fileflags::create_new))
                return false;

            dwDesiredAccess = GENERIC_READ;
            dwCreationDisposition = OPEN_EXISTING;
            dwShareMode = FILE_SHARE_READ;
            return true;
        }

        // Write access or read-write access
        if (flags & fileflags::write) {
            // Must specify either create_new or open_existing (or both).
            if (!(flags & (fileflags::create_new | fileflags::open_existing)))
                return false;

            // Per
            // https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
            // When opening a file for write, specify both GENERIC_READ and
            // GENERIC_WRITE even if we don't intend to read from the file.
            dwDesiredAccess |= GENERIC_READ | GENERIC_WRITE;

            // if (flags & fileflags::read)
            //    dwDesiredAccess |= GENERIC_READ;

            // Must create a new file.
            if ((flags & fileflags::create_new) &&
                !(flags & fileflags::open_existing))
                dwCreationDisposition = CREATE_NEW;
            // Can create a new file or open an existing one.
            else if ((flags & fileflags::create_new) &&
                     (flags & fileflags::open_existing)) {
                if (flags & fileflags::trunc)
                    dwCreationDisposition = CREATE_ALWAYS;
                else
                    dwCreationDisposition = OPEN_ALWAYS;
                // Can only open an existing file.
            } else if (!(flags & fileflags::create_new) &&
                       (flags & fileflags::open_existing)) {
                if (flags & fileflags::trunc)
                    dwCreationDisposition = TRUNCATE_EXISTING;
                else
                    dwCreationDisposition = OPEN_EXISTING;
            }

            return true;
        }

        return false;
    }

    /*************************************************************************
     * filechannel_base::async_open()
     */
    // clang-format off
    inline auto filechannel_base::_async_open(
            std::filesystem::path const &path,
            fileflags_t flags)
        -> task<expected<void, std::error_code>> {
        // clang-format on

        if (is_open())
            co_return make_unexpected(cio::error::channel_already_open);

        DWORD dwDesiredAccess = 0;
        DWORD dwShareMode = 0;
        DWORD dwCreationDisposition = 0;

        if (!_make_flags(flags, dwDesiredAccess, dwCreationDisposition,
                         dwShareMode))
            co_return make_unexpected(cio::error::filechannel_invalid_flags);

        std::wstring wpath(path.native());

        auto handle = co_await win32::AsyncCreateFileW(
            wpath.c_str(), dwDesiredAccess, dwShareMode, nullptr,
            dwCreationDisposition, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
            NULL);

        if (!handle)
            co_return make_unexpected(
                win32::win32_to_generic_error(handle.error()));

        _native_handle.assign(*handle);
        co_return {};
    }

    /*************************************************************************
     * filechannel_base::open()
     */
    // clang-format off
    inline auto filechannel_base::_open(
            std::filesystem::path const &path,
            fileflags_t flags)
        -> expected<void, std::error_code> {
        // clang-format on

        if (is_open())
            return make_unexpected(cio::error::channel_already_open);

        DWORD dwDesiredAccess = 0;
        DWORD dwShareMode = 0;
        DWORD dwCreationDisposition = 0;

        if (!_make_flags(flags, dwDesiredAccess, dwCreationDisposition,
                         dwShareMode))
            return make_unexpected(cio::error::filechannel_invalid_flags);

        std::wstring wpath(path.native());

        auto handle =
            ::CreateFileW(wpath.c_str(), dwDesiredAccess, dwShareMode, nullptr,
                          dwCreationDisposition,
                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

        // Return error if any.
        if (handle == INVALID_HANDLE_VALUE)
            return make_unexpected(
                win32::win32_to_generic_error(win32::get_last_error()));

        // Because we didn't use AsyncCreateFileW(), we have to manually
        // associate the handle with the completion port.
        reactor_handle::get_global_reactor().associate_handle(handle);

        _native_handle.assign(handle);
        return {};
    }

    /*************************************************************************
     * filechannel_base::is_open()
     */
    // clang-format off
    inline auto filechannel_base::is_open() const
        -> bool {
        // clang-format on

        return static_cast<bool>(_native_handle);
    }

    /*************************************************************************
     * filechannel_base::close()
     */
    // clang-format off
    inline auto filechannel_base::close()
        -> expected<void, std::error_code> {
        // clang-format on

        if (!is_open())
            return make_unexpected(cio::error::channel_not_open);

        auto err = _native_handle.close();
        if (err)
            return make_unexpected(err);
        return {};
    }

    /*************************************************************************
     * filechannel_base::async_close()
     */
    // clang-format off
    inline auto filechannel_base::async_close()
        -> task<expected<void, std::error_code>> {
        // clang-format on

        auto err =
            co_await async_invoke([&] { return _native_handle.close(); });

        if (err)
            co_return make_unexpected(err);

        co_return {};
    }

    /*************************************************************************
     * filechannel_base::_async_read_some_at()
     */

    inline auto filechannel_base::_async_read_some_at(io_offset_t loc,
                                                      std::byte *buffer,
                                                      io_size_t nobjs)
        -> task<expected<io_size_t, std::error_code>> {
        SK_CHECK(is_open(), "attempt to read on a closed channel");

        auto dwbytes = cio::detail::int_cast<DWORD>(nobjs);

        DWORD bytes_read = 0;
        auto ret = co_await win32::AsyncReadFile(
            _native_handle.native_handle(), buffer, dwbytes, &bytes_read, loc);

        if (!ret)
            co_return make_unexpected(
                win32::win32_to_generic_error(ret.error()));

        co_return bytes_read;
    }

    /*************************************************************************
     * filechannel_base::_read_some_at()
     */

    inline auto filechannel_base::_read_some_at(io_offset_t loc,
                                                std::byte *buffer,
                                                io_size_t nobjs)
        -> expected<io_size_t, std::error_code> {
        SK_CHECK(is_open(), "attempt to read on a closed channel");

        auto dwbytes = cio::detail::int_cast<DWORD>(nobjs);

        OVERLAPPED overlapped;
        std::memset(&overlapped, 0, sizeof(overlapped));
        overlapped.Offset = static_cast<DWORD>(loc & 0xFFFFFFFFUL);
        overlapped.OffsetHigh = static_cast<DWORD>(loc >> 32);

        // Create an event for the OVERLAPPED.  We don't actually use the event
        // but it has to be present.
        auto event_handle = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event_handle == nullptr)
            return make_unexpected(
                win32::win32_to_generic_error(win32::get_last_error()));

        unique_handle evt(event_handle);

        // Set the low bit on the event handle to prevent the completion packet
        // being queued to the iocp_reactor, which won't know what to do with
        // it.
        overlapped.hEvent = reinterpret_cast<HANDLE>(
            reinterpret_cast<std::uintptr_t>(event_handle) | 0x1);

        DWORD bytes_read = 0;
        auto ret = ::ReadFile(_native_handle.native_handle(), buffer, dwbytes,
                              &bytes_read, &overlapped);

        if (!ret && (::GetLastError() == ERROR_IO_PENDING))
            ret = ::GetOverlappedResult(_native_handle.native_handle(),
                                        &overlapped, &bytes_read, TRUE);

        if (!ret)
            return make_unexpected(
                win32::win32_to_generic_error(win32::get_last_error()));

        return bytes_read;
    }

    /*************************************************************************
     * odafilechannel::async_write_some_at()
     */

    inline auto filechannel_base::_async_write_some_at(io_offset_t loc,
                                                       std::byte const *buffer,
                                                       io_size_t nobjs)
        -> task<expected<io_size_t, std::error_code>> {

        SK_CHECK(is_open(), "attempt to write on a closed channel");

        auto dwbytes = cio::detail::int_cast<DWORD>(nobjs);
        DWORD bytes_written = 0;

        auto ret = co_await win32::AsyncWriteFile(
            _native_handle.native_handle(), buffer, dwbytes, &bytes_written, loc);

        if (ret)
            co_return make_unexpected(win32::win32_to_generic_error(ret.error()));

        co_return bytes_written;
    }

    /*************************************************************************
     * idafilechannel::write_some_at()
     */

    inline auto filechannel_base::_write_some_at(io_offset_t loc,
                                                 std::byte const *buffer,
                                                 io_size_t nobjs)
        -> expected<io_size_t, std::error_code> {

        SK_CHECK(is_open(), "attempt to write on a closed channel");

        auto dwbytes = cio::detail::int_cast<DWORD>(nobjs);

        OVERLAPPED overlapped;
        std::memset(&overlapped, 0, sizeof(overlapped));
        overlapped.Offset = static_cast<DWORD>(loc & 0xFFFFFFFFUL);
        overlapped.OffsetHigh = static_cast<DWORD>(loc >> 32);

        // Create an event for the OVERLAPPED.  We don't actually use the event
        // but it has to be present.
        auto event_handle = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event_handle == nullptr)
            return make_unexpected(
                win32::win32_to_generic_error(win32::get_last_error()));

        unique_handle evt(event_handle);

        // Set the low bit on the event handle to prevent the completion packet
        // being queued to the iocp_reactor, which won't know what to do with
        // it.
        overlapped.hEvent = reinterpret_cast<HANDLE>(
            reinterpret_cast<std::uintptr_t>(event_handle) | 0x1);

        DWORD bytes_written = 0;
        auto ret = ::WriteFile(_native_handle.native_handle(), buffer, dwbytes,
                               &bytes_written, &overlapped);

        if (!ret && (::GetLastError() == ERROR_IO_PENDING))
            ret = ::GetOverlappedResult(_native_handle.native_handle(),
                                        &overlapped, &bytes_written, TRUE);

        if (!ret)
            return make_unexpected(
                win32::win32_to_generic_error(win32::get_last_error()));

        return bytes_written;
    }

} // namespace sk::cio::win32::detail

#endif // SK_CIO_WIN32_CHANNEL_DAFILECHANNEL_BASE_HXX_INCLUDED
