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

#include <sk/cio/win32/error.hxx>
#include <sk/cio/win32/iocp_reactor.hxx>

namespace sk::cio::win32 {

    iocp_reactor::iocp_reactor() {
        auto hdl =
            ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        completion_port.assign(hdl);
    }

    auto iocp_reactor::get_global_reactor() -> iocp_reactor & {
        static iocp_reactor global_reactor;
        return global_reactor;
    }

    auto iocp_reactor::reactor_thread_fn() -> void {
        for (;;) {
            DWORD bytes_transferred;
            ULONG_PTR completion_key;
            iocp_awaitable *overlapped = nullptr;

            auto ret = ::GetQueuedCompletionStatus(
                completion_port.handle_value, &bytes_transferred,
                &completion_key, reinterpret_cast<OVERLAPPED **>(&overlapped),
                INFINITE);

            if (overlapped == nullptr)
                // Happens when our completion port is closed.
                return;

            overlapped->success = ret;
            if (!overlapped->success)
                overlapped->error = GetLastError();
            else
                overlapped->error = 0;
            overlapped->bytes_transferred = bytes_transferred;
            overlapped->coro_handle.resume();
        }
    }

    auto iocp_reactor::start() -> void {
        reactor_thread = std::jthread([=, this] { reactor_thread_fn(); });
    }

    auto iocp_reactor::stop() -> void {
        completion_port.close();
        reactor_thread.join();
    }

    auto iocp_reactor::start_global_reactor() -> void {
        get_global_reactor().start();
    }

    auto iocp_reactor::stop_global_reactor() -> void {
        get_global_reactor().stop();
    }

    auto iocp_reactor::associate_handle(HANDLE h) -> void {
        ::CreateIoCompletionPort(h, completion_port.handle_value, 0, 0);
    }

    task<HANDLE> AsyncCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess,
                                  DWORD dwShareMode,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  DWORD dwCreationDisposition,
                                  DWORD dwFlagsAndAttributes,
                                  HANDLE hTemplateFile) {

        auto handle = ::CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                                    lpSecurityAttributes, dwCreationDisposition,
                                    dwFlagsAndAttributes, hTemplateFile);

        if (handle != INVALID_HANDLE_VALUE)
            iocp_reactor::get_global_reactor().associate_handle(handle);

        co_return handle;
    }

    task<HANDLE> AsyncCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess,
                                  DWORD dwShareMode,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  DWORD dwCreationDisposition,
                                  DWORD dwFlagsAndAttributes,
                                  HANDLE hTemplateFile) {
        auto handle = ::CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                                    lpSecurityAttributes, dwCreationDisposition,
                                    dwFlagsAndAttributes, hTemplateFile);

        if (handle != INVALID_HANDLE_VALUE)
            iocp_reactor::get_global_reactor().associate_handle(handle);

        co_return handle;
    }

    task<std::error_code> AsyncReadFile(HANDLE hFile, LPVOID lpBuffer,
                             DWORD nNumberOfBytesToRead,
                             LPDWORD lpNumberOfBytesRead, DWORD64 Offset) {

        iocp_awaitable overlapped;
        memset(&overlapped, 0, sizeof(OVERLAPPED));
        overlapped.Offset = static_cast<DWORD>(Offset & 0xFFFFFFFFUL);
        overlapped.OffsetHigh = static_cast<DWORD>(Offset >> 32);

        bool ret = ::ReadFile(hFile, lpBuffer, nNumberOfBytesToRead,
                              lpNumberOfBytesRead, &overlapped);

        if (ret == TRUE)
            co_return cio::error::no_error;

        auto err = GetLastError();

        if (err == ERROR_IO_PENDING) {
            co_await overlapped;

            if (!overlapped.success)
                co_return win32::make_win32_error(overlapped.error);

            *lpNumberOfBytesRead = overlapped.bytes_transferred;
            co_return cio::error::no_error;
        }

        co_return win32::make_win32_error(err);
    }

} // namespace sk::async::win32
