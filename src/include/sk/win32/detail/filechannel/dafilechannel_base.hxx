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

#ifndef SK_WIN32_DETAIL_FILECHANNEL_DAFILECHANNEL_BASE_HXX_INCLUDED
#define SK_WIN32_DETAIL_FILECHANNEL_DAFILECHANNEL_BASE_HXX_INCLUDED

#include <system_error>

#include <sk/async_invoke.hxx>
#include <sk/channel/types.hxx>
#include <sk/detail/safeint.hxx>
#include <sk/reactor.hxx>
#include <sk/task.hxx>
#include <sk/win32/async_api.hxx>
#include <sk/win32/detail/filechannel/filechannel_base.hxx>
#include <sk/win32/error.hxx>
#include <sk/win32/handle.hxx>

namespace sk::win32::detail {

    /*************************************************************************
     *
     * dafilechannel_base: base class for direct access file channels.
     *
     */
    struct dafilechannel_base : filechannel_base {
        dafilechannel_base(dafilechannel_base const &) = delete;
        dafilechannel_base(dafilechannel_base &&) noexcept = default;
        dafilechannel_base &operator=(dafilechannel_base const &) = delete;
        dafilechannel_base &operator=(dafilechannel_base &&) noexcept = default;

    protected:
        /*
         * Read data.
         */
        [[nodiscard]] auto
        _async_read_some_at(io_offset_t loc, std::byte *buffer, io_size_t nobjs)
            -> task<expected<io_size_t, std::error_code>>;

        [[nodiscard]] auto
        _read_some_at(io_offset_t loc, std::byte *buffer, io_size_t nobjs)
            -> expected<io_size_t, std::error_code>;

        /*
         * Write data.
         */
        [[nodiscard]] auto
        _async_write_some_at(io_offset_t, std::byte const *, io_size_t)
            -> task<expected<io_size_t, std::error_code>>;

        [[nodiscard]] auto
        _write_some_at(io_offset_t, std::byte const *, io_size_t)
            -> expected<io_size_t, std::error_code>;

        dafilechannel_base() = default;
        ~dafilechannel_base() = default;
    };

    /*************************************************************************
     * filechannel_base::_async_read_some_at()
     */

    inline auto dafilechannel_base::_async_read_some_at(io_offset_t loc,
                                                        std::byte *buffer,
                                                        io_size_t nobjs)
        -> task<expected<io_size_t, std::error_code>>
    {
        sk::detail::check(is_open(), "attempt to read on a closed channel");

        auto dwbytes = sk::detail::int_cast<DWORD>(nobjs);

        DWORD bytes_read = 0;
        auto ret = co_await win32::AsyncReadFile(
            _native_handle.native_handle(), buffer, dwbytes, &bytes_read, loc);

        if (!ret)
            co_return make_unexpected(
                win32::win32_to_generic_error(ret.error()));

        co_return bytes_read;
    }

    /*************************************************************************
     * dafilechannel_base::_read_some_at()
     */

    inline auto dafilechannel_base::_read_some_at(io_offset_t loc,
                                                  std::byte *buffer,
                                                  io_size_t nobjs)
        -> expected<io_size_t, std::error_code>
    {
        sk::detail::check(is_open(), "attempt to read on a closed channel");

        auto dwbytes = sk::detail::int_cast<DWORD>(nobjs);

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
        auto ret = ::ReadFile(_native_handle.native_handle(),
                              buffer,
                              dwbytes,
                              &bytes_read,
                              &overlapped);

        if (!ret && (::GetLastError() == ERROR_IO_PENDING))
            ret = ::GetOverlappedResult(
                _native_handle.native_handle(), &overlapped, &bytes_read, TRUE);

        if (!ret)
            return make_unexpected(
                win32::win32_to_generic_error(win32::get_last_error()));

        return bytes_read;
    }

    /*************************************************************************
     * dafilechannel_base::async_write_some_at()
     */

    inline auto dafilechannel_base::_async_write_some_at(
        io_offset_t loc, std::byte const *buffer, io_size_t nobjs)
        -> task<expected<io_size_t, std::error_code>>
    {

        sk::detail::check(is_open(), "attempt to write on a closed channel");

        auto dwbytes = sk::detail::int_cast<DWORD>(nobjs);
        DWORD bytes_written = 0;

        auto ret =
            co_await win32::AsyncWriteFile(_native_handle.native_handle(),
                                           buffer,
                                           dwbytes,
                                           &bytes_written,
                                           loc);

        if (ret)
            co_return make_unexpected(
                win32::win32_to_generic_error(ret.error()));

        co_return bytes_written;
    }

    /*************************************************************************
     * dafilechannel_base::write_some_at()
     */

    inline auto dafilechannel_base::_write_some_at(io_offset_t loc,
                                                   std::byte const *buffer,
                                                   io_size_t nobjs)
        -> expected<io_size_t, std::error_code>
    {

        sk::detail::check(is_open(), "attempt to write on a closed channel");

        auto dwbytes = sk::detail::int_cast<DWORD>(nobjs);

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
        auto ret = ::WriteFile(_native_handle.native_handle(),
                               buffer,
                               dwbytes,
                               &bytes_written,
                               &overlapped);

        if (!ret && (::GetLastError() == ERROR_IO_PENDING))
            ret = ::GetOverlappedResult(_native_handle.native_handle(),
                                        &overlapped,
                                        &bytes_written,
                                        TRUE);

        if (!ret)
            return make_unexpected(
                win32::win32_to_generic_error(win32::get_last_error()));

        return bytes_written;
    }

} // namespace sk::win32::detail

#endif // SK_WIN32_DETAIL_FILECHANNEL_DAFILECHANNEL_BASE_HXX_INCLUDED
