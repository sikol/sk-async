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

#ifndef SK_CIO_WIN32_ERROR_HXX_INCLUDED
#define SK_CIO_WIN32_ERROR_HXX_INCLUDED

#include <system_error>

#include <sk/channel/error.hxx>
#include <sk/win32/windows.hxx>

/*
 * Error handling support for Win32.
 */

namespace sk::win32 {

    /*
     * Win32 error category - this corresponds to FORMAT_MESSAGE_FROM_SYSTEM.
     */

    enum struct error : DWORD {
        success = ERROR_SUCCESS, // 0
    };

} // namespace sk::win32

namespace std {
    template <>
    struct is_error_code_enum<sk::win32::error> : true_type {
    };
}; // namespace std

namespace sk::win32 {

    namespace detail {

        struct win32_errc_category : std::error_category {
            auto win32_errc_category::name() const noexcept -> char const *
            {
                return "win32";
            }

            auto win32_errc_category::message(int c) const -> std::string
            {
                LPSTR msgbuf;

                auto len = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                            FORMAT_MESSAGE_FROM_SYSTEM |
                                            FORMAT_MESSAGE_IGNORE_INSERTS,
                                            nullptr,
                                            c,
                                            0,
                                            reinterpret_cast<LPSTR>(&msgbuf),
                                            0,
                                            nullptr);

                if (len == 0)
                    return "[FormatMessageA() failed]";

                // FormatMessage() sometimes puts newline character on the end
                // of the string.  Why?  Who knows.
                while (msgbuf[len - 1] == '\r' || msgbuf[len - 1] == '\n')
                    len--;

                std::string ret(msgbuf, len);
                ::LocalFree(msgbuf);

                return ret;
            }
        };

    } // namespace detail

    inline auto win32_errc_category() -> detail::win32_errc_category const &
    {
        static detail::win32_errc_category c;
        return c;
    }

    inline auto make_error_code(error e) -> std::error_code
    {
        switch (static_cast<DWORD>(e)) {
        case ERROR_HANDLE_EOF:
            return sk::error::end_of_file;
        default:
            return {static_cast<int>(e), win32_errc_category()};
        }
    }

    inline auto make_win32_error(int e) -> std::error_code
    {
        return make_error_code(static_cast<error>(e));
    }

    inline auto make_win32_error(DWORD e) -> std::error_code
    {
        return make_error_code(static_cast<error>(e));
    }

    inline auto make_win32_error(LSTATUS e) -> std::error_code
    {
        return make_error_code(static_cast<error>(e));
    }

    // Not safe to call across a coroutine suspend point, because the error
    // value is thread-specific.
    inline auto get_last_error() -> std::error_code
    {
        return make_win32_error(::GetLastError());
    }

    inline auto get_last_winsock_error() -> std::error_code
    {
        return make_win32_error(::WSAGetLastError());
    }

    // Convert a Win32 error into a std::generic_category error if
    // there's an appropriate conversion.  This is used by the portable
    // I/O parts of the library so that the user only has to test for
    // a single error code.
    inline auto win32_to_generic_error(std::error_code ec) -> std::error_code
    {
        // If it's not a Win32 error to begin with, return it as-is.
        if (&ec.category() != &win32_errc_category())
            return ec;

        switch (ec.value()) {
        case ERROR_HANDLE_EOF:
            return sk::error::end_of_file;

        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return std::make_error_code(std::errc::no_such_file_or_directory);

        case ERROR_TOO_MANY_OPEN_FILES:
            return std::make_error_code(
                std::errc::too_many_files_open_in_system);

        case ERROR_ACCESS_DENIED:
            return std::make_error_code(std::errc::permission_denied);

        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return std::make_error_code(std::errc::not_enough_memory);

        default:
            return ec;
        }
    }

} // namespace sk::win32

#endif // SK_CIO_WIN32_ERROR_HXX_INCLUDED
