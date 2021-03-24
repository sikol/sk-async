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

#ifndef SK_CIO_WIN32_CHANNEL_OSEQFILECHANNEL_BASE_HXX_INCLUDED
#define SK_CIO_WIN32_CHANNEL_OSEQFILECHANNEL_BASE_HXX_INCLUDED

#include <filesystem>
#include <system_error>

#include <sk/buffer/buffer.hxx>
#include <sk/cio/channel/concepts.hxx>
#include <sk/cio/channel/filechannel.hxx>
#include <sk/cio/error.hxx>
#include <sk/cio/task.hxx>
#include <sk/cio/types.hxx>
#include <sk/cio/win32/channel/detail/filechannel_base.hxx>
#include <sk/cio/win32/error.hxx>
#include <sk/cio/win32/handle.hxx>
#include <sk/cio/win32/iocp_reactor.hxx>

namespace sk::cio::win32::detail {

    /*************************************************************************
     *
     * oseqfilechannel_base: a direct access channel that writes to a file.
     */

    // clang-format off
    template <typename T>
    struct oseqfilechannel_base {
        oseqfilechannel_base(oseqfilechannel_base const &) = delete;
        oseqfilechannel_base(oseqfilechannel_base &&) noexcept = default;
        oseqfilechannel_base &operator=(oseqfilechannel_base const &) = delete;
        oseqfilechannel_base &operator=(oseqfilechannel_base &&) noexcept = default;

        /*
         * Write data.
         */
        [[nodiscard]]
        auto async_write_some(std::byte const *buffer, io_size_t)
            -> task<expected<io_size_t, std::error_code>>;

        [[nodiscard]]
        auto write_some(std::byte const *buffer, io_size_t)
            -> expected<io_size_t, std::error_code>;

    protected:
        oseqfilechannel_base() = default;
        ~oseqfilechannel_base() = default;

    private:
        io_offset_t _write_position;
    };

    // clang-format on

    /*************************************************************************
     * oseqfilechannel_base::async_write_some()
     */

    template <typename T>
    auto oseqfilechannel_base<T>::async_write_some(std::byte const *buffer,
                                                   io_size_t nobjs)
        -> task<expected<io_size_t, std::error_code>> {

        SK_CHECK(static_cast<T *>(this)->is_open(),
                 "attempt to write on a closed channel");
        SK_CHECK(nobjs > 0, "attempt to write empty buffer");

        if (!cio::detail::can_add(_write_position, nobjs))
            return make_unexpected(std::errc::value_too_large);

        DWORD bytes_written = 0;
        auto dwbytes = cio::detail::int_cast<DWORD>(nobjs);

        auto ret = co_await win32::AsyncWriteFile(
            static_cast<T *>(this)->native_handle.handle_value, buffer,
            dwbytes, &bytes_written, _write_position);

        if (ret)
            co_return make_unexpected(win32::win32_to_generic_error(ret));

        if (_write_position != at_end)
            _write_position += bytes_written;
        co_return bytes_written;
    }

    /*************************************************************************
     * oseqfilechannel_base::write_some()
     */

    template <typename T>
    auto oseqfilechannel_base<T>::write_some(std::byte const *buffer, io_size_t nobjs)
        -> expected<io_size_t, std::error_code> 
    {

        SK_CHECK(static_cast<T *>(this)->is_open(),
                 "attempt to write on a closed channel");
        SK_CHECK(nobjs > 0, "attempt to write empty buffer");

        if (!cio::detail::can_add(_write_position, nobjs))
            return make_unexpected(std::errc::value_too_large);

        DWORD bytes_written = 0;
        auto dwbytes = cio::detail::int_cast<DWORD>(nobjs);

        OVERLAPPED overlapped;
        std::memset(&overlapped, 0, sizeof(overlapped));
        overlapped.Offset = static_cast<DWORD>(_write_position & 0xFFFFFFFFUL);
        overlapped.OffsetHigh = static_cast<DWORD>(_write_position >> 32);

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

        auto ret =
            ::WriteFile(static_cast<T *>(this)->native_handle.handle_value,
                        buffer, dwbytes, &bytes_written, &overlapped);

        if (!ret && (::GetLastError() == ERROR_IO_PENDING))
            ret = ::GetOverlappedResult(
                static_cast<T *>(this)->native_handle.handle_value, &overlapped,
                &bytes_written, TRUE);

        if (!ret)
            return make_unexpected(
                win32::win32_to_generic_error(win32::get_last_error()));

        if (_write_position != at_end)
            _write_position += bytes_written;
        return bytes_written;
    }

} // namespace sk::cio::win32::detail

#endif // SK_CIO_WIN32_CHANNEL_OSEQFILECHANNEL_BASE_HXX_INCLUDED
