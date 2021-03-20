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

#ifndef SK_CIO_WIN32_CHANNEL_IFILECHANNEL_HXX_INCLUDED
#define SK_CIO_WIN32_CHANNEL_IFILECHANNEL_HXX_INCLUDED

#include <filesystem>
#include <system_error>

#include <sk/buffer/buffer.hxx>
#include <sk/cio/channel/concepts.hxx>
#include <sk/cio/error.hxx>
#include <sk/cio/task.hxx>
#include <sk/cio/types.hxx>
#include <sk/cio/win32/channel/filechannel_base.hxx>
#include <sk/cio/win32/error.hxx>
#include <sk/cio/win32/handle.hxx>
#include <sk/cio/win32/iocp_reactor.hxx>

namespace sk::cio::win32 {

    /*************************************************************************
     *
     * idafilechannel: a direct access channel that reads from a file.
     */

    // clang-format off
    template <typename CharT>
    struct idafilechannel final : filechannel_base<CharT> {

        /*
         * Create an idafilechannel which is closed.
         */
        idafilechannel() = default;

        /*
         * Create an idafilechannel from a native handle.
         */
        explicit idafilechannel(
            typename filechannel_base<CharT>::native_handle_type &&handle);

        idafilechannel(idafilechannel const &) = delete;
        idafilechannel(idafilechannel &&) noexcept = default;
        idafilechannel &operator=(idafilechannel const &) = delete;
        idafilechannel &operator=(idafilechannel &&) noexcept = default;
        ~idafilechannel();

        /*
         * Read data.
         */
        template <sk::writable_buffer Buffer>
        auto async_read_some_at(io_size_t, io_offset_t, Buffer &buffer)
            -> task<expected<io_size_t, std::error_code>>
            requires std::same_as<
                sk::buffer_value_t<Buffer>, 
                typename filechannel_base<CharT>::value_type>;

        template <sk::writable_buffer Buffer>
        auto read_some_at(io_size_t, io_offset_t, Buffer &buffer)
            -> expected<io_size_t, std::error_code>
            requires std::same_as<
                sk::buffer_value_t<Buffer>, 
                typename filechannel_base<CharT>::value_type>;
    };
    // clang-format on

    static_assert(idachannel<idafilechannel<char>>);

    /*************************************************************************
     * idafilechannel::idafilechannel()
     */
    template <typename CharT>
    idafilechannel<CharT>::idafilechannel(
        typename filechannel_base<CharT>::native_handle_type &&native_handle_)
        : filechannel_base<CharT>(native_handle_) {}

    /*************************************************************************
     * idafilechannel::~idafilechannel()
     */
    template <typename CharT>
    idafilechannel<CharT>::~idafilechannel() = default;

    /*************************************************************************
     * idafilechannel::async_read_some_at()
     */

    // clang-format off
    template <typename CharT>
    template <sk::writable_buffer Buffer>
    auto idafilechannel<CharT>::async_read_some_at(io_size_t nobjs,
                                                   io_offset_t loc,
                                                   Buffer &buffer)
        -> task<expected<io_size_t, std::error_code>> 
        requires std::same_as<
            sk::buffer_value_t<Buffer>, 
            typename filechannel_base<CharT>::value_type>
    {
        // clang-format on

        DWORD bytes_read = 0;

        auto ranges = buffer.writable_ranges();

        if (std::ranges::size(ranges) == 0)
            co_return 0u;

        auto first_range = *std::ranges::begin(ranges);
        auto buf_data = std::ranges::data(first_range);
        auto buf_size = std::ranges::size(first_range);

        // The maximum I/O size we can support.
        auto max_objs =
            static_cast<std::size_t>(std::numeric_limits<DWORD>::max()) /
            sizeof(CharT);

        // Can't read more than max_objs.
        if (nobjs > max_objs)
            nobjs = max_objs;

        // Can't read more than we have buffer space for.
        if (nobjs > buf_size)
            nobjs = buf_size;

        // The number of bytes we'll read.
        DWORD dwbytes = static_cast<DWORD>(nobjs) * sizeof(CharT);

        auto ret = co_await win32::AsyncReadFile(
            filechannel_base<CharT>::native_handle.handle_value, buf_data,
            dwbytes, &bytes_read, loc);

        if (ret)
            co_return make_unexpected(win32::win32_to_generic_error(ret));

        auto objs_read = bytes_read * sizeof(CharT);
        buffer.commit(objs_read);
        co_return objs_read;
    }

    /*************************************************************************
     * idafilechannel::read_some_at()
     */

    // clang-format off
    template <typename CharT>
    template <sk::writable_buffer Buffer>
    auto idafilechannel<CharT>::read_some_at(io_size_t nobjs,
                                             io_offset_t loc,
                                             Buffer &buffer)
        -> expected<io_size_t, std::error_code> 
        requires std::same_as<
            sk::buffer_value_t<Buffer>, 
            typename filechannel_base<CharT>::value_type>
    {
        // clang-format on

        DWORD bytes_read = 0;

        auto ranges = buffer.writable_ranges();

        if (std::ranges::size(ranges) == 0)
            return 0u;

        auto first_range = *std::ranges::begin(ranges);
        auto buf_data = std::ranges::data(first_range);
        auto buf_size = std::ranges::size(first_range);

        // The maximum I/O size we can support.
        auto max_objs =
            static_cast<std::size_t>(std::numeric_limits<DWORD>::max()) /
            sizeof(CharT);

        // Can't read more than max_objs.
        if (nobjs > max_objs)
            nobjs = max_objs;

        // Can't read more than we have buffer space for.
        if (nobjs > buf_size)
            nobjs = buf_size;

        // The number of bytes we'll read.
        DWORD dwbytes = static_cast<DWORD>(nobjs) * sizeof(CharT);

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

        auto ret =
            ::ReadFile(filechannel_base<CharT>::native_handle.handle_value,
                       buf_data, dwbytes, &bytes_read, &overlapped);

        if (!ret && (::GetLastError() == ERROR_IO_PENDING))
            ret = ::GetOverlappedResult(
                filechannel_base<CharT>::native_handle.handle_value,
                &overlapped, &bytes_read, TRUE);

        if (!ret) {
            return make_unexpected(
                win32::win32_to_generic_error(win32::get_last_error()));
        }

        auto objs_read = bytes_read * sizeof(CharT);
        buffer.commit(objs_read);
        return objs_read;
    }

} // namespace sk::cio::win32

#endif // SK_CIO_WIN32_CHANNEL_IFILECHANNEL_HXX_INCLUDED
