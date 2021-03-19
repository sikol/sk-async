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

#ifndef SK_ASYNC_STREAM_IFILESTREAM_HXX_INCLUDED
#define SK_ASYNC_STREAM_IFILESTREAM_HXX_INCLUDED

#include <filesystem>
#include <system_error>

#include <sk/async/task.hxx>
#include <sk/async/win32/error.hxx>
#include <sk/async/win32/handle.hxx>
#include <sk/async/win32/iocp_reactor.hxx>
#include <sk/async/stream/errors.hxx>
#include <sk/buffer/buffer.hxx>

namespace sk::async {

    /*
     * ifilestream: an async stream that reads bytes from a file.
     */
    template <typename CharT> struct ifilestream final {
        using native_handle_type = win32::unique_handle;

        native_handle_type native_handle{INVALID_HANDLE_VALUE};

        explicit ifilestream(ifilestream const &) = delete;
        ifilestream(ifilestream &&) noexcept = default;
        ifilestream &operator=(ifilestream const &) = delete;
        ifilestream &operator=(ifilestream &&) noexcept = default;
        ~ifilestream();

        /*
         * Create an ifilestream which is closed.
         */
        ifilestream() = default;

        /*
         * Create an ifilestream from a native handle.
         */
        explicit ifilestream(native_handle_type &&handle);

        /*
         * Open a file.
         */
        auto async_open(std::filesystem::path const &) -> task<std::error_code>;

        /*
         * Test if this filestream has been opened.
         */
        auto is_open() const -> bool;

        /*
         * Close the file.  The result of closing a file with outstanding async
         * operations is undefined.
         */
        auto async_close() -> task<std::error_code>;

        /*
         * Read data from the current file position.
         */
        template <sk::writable_buffer_of<CharT> BufferT>
        auto async_read(BufferT &buffer) -> expected<std::size_t, std::error_code>;

    private:
        std::uint64_t file_position = 0;
    };

    template <typename CharT>
    ifilestream<CharT>::ifilestream(native_handle_type &&native_handle_)
        : native_handle(std::move(native_handle_)) {}

    template <typename CharT> ifilestream<CharT>::~ifilestream() = default;

    template <typename CharT> bool ifilestream<CharT>::is_open() const {
        return native_handle != INVALID_HANDLE_VALUE;
    }

    template <typename CharT>
    task<std::error_code>
    ifilestream<CharT>::async_open(std::filesystem::path const &path) {
        std::wstring wpath(path.native());

/*
 * Because CreateFileW() may block (e.g. when accessing a remote server)
 * and there's no overlapped version, we have to run the open in a
 * separate thread.
 */
#if 0
        auto result = co_await async::async_execute(
            [&]() -> yarrow::expected<win32::shared_handle> {
                return win32::create_file(
                    wpath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                    win32::null_handle);
            });
#endif

        auto handle = co_await win32::AsyncCreateFileW(
            wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

        // Return error if any.
        if (handle == INVALID_HANDLE_VALUE)
            co_return win32::win32_to_generic_error(win32::get_last_error());

        native_handle.assign(handle);
        co_return error::no_error;
    }

    template <typename CharT>
    task<std::error_code> ifilestream<CharT>::async_close() {
        native_handle.close();
        co_return error::no_error;
    }

    template <typename CharT>
    template <sk::writable_buffer_of<CharT> BufferT>
    expected<std::size_t, std::error_code>
    ifilestream<CharT>::async_read(BufferT &buffer) {

        DWORD bytes_read = 0;

        auto ranges = buffer.writable_ranges();
        if (std::ranges::size(ranges) == 0)
            co_return 0u;

        auto first_range = *std::ranges::begin(ranges);
        auto data = std::ranges::data(first_range);
        auto size = std::ranges::size(first_range);

        DWORD dwsize;

        if (size >
            static_cast<decltype(size)>(std::numeric_limits<DWORD>::max()))
            dwsize = std::numeric_limits<DWORD>::max();
        else
            dwsize = static_cast<DWORD>(size);

        auto ret =
            co_await win32::AsyncReadFile(native_handle.handle_value, data,
                                          dwsize, &bytes_read, file_position);

        if (!ret) {
            buffer.commit(bytes_read);
            file_position += bytes_read;
            co_return bytes_read;
        }

        co_return make_unexpected(win32::win32_to_generic_error(ret));
    }

} // namespace sk::async

#endif // SK_ASYNC_STREAM_IFILESTREAM_HXX_INCLUDED
