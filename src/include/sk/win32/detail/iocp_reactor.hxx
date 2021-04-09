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

#ifndef SK_CIO_WIN32_IOCP_REACTOR_HXX_INCLUDED
#define SK_CIO_WIN32_IOCP_REACTOR_HXX_INCLUDED

#include <iostream>
#include <system_error>
#include <thread>

#include <sk/win32/error.hxx>
#include <sk/win32/handle.hxx>
#include <sk/win32/windows.hxx>
#include <sk/task.hxx>
#include <sk/executor.hxx>

namespace sk::win32::detail {

    struct iocp_coro_state : OVERLAPPED {
        iocp_coro_state() : OVERLAPPED({}) {}
        bool was_pending = false;
        BOOL success = 0;
        DWORD error = 0;
        DWORD bytes_transferred = 0;
        executor *executor;
        coroutine_handle<> coro_handle;
        std::mutex mutex;
    };

    struct iocp_reactor {

        iocp_reactor();

        // Not copyable.
        iocp_reactor(iocp_reactor const &) = delete;
        iocp_reactor &operator=(iocp_reactor const &) = delete;

        // Movable.
        iocp_reactor(iocp_reactor &&) noexcept = delete;
        iocp_reactor &operator=(iocp_reactor &&) noexcept = delete;

        unique_handle completion_port;

        // Associate a new handle with our i/o port.
        auto associate_handle(HANDLE) -> void;

        // Start this reactor.
        auto start() -> void;

        // Stop this reactor.
        auto stop() -> void;

        auto get_system_executor() -> mt_executor *;

    private:
        void completion_thread_fn(void);
        std::jthread completion_thread;
    };

    inline iocp_reactor::iocp_reactor()
    {
        auto hdl =
            ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        completion_port.assign(hdl);
        get_system_executor()->start_threads();
    }

    inline auto iocp_reactor::get_system_executor() -> mt_executor *
    {
        static mt_executor xer;
        return &xer;
    }

    inline auto iocp_reactor::completion_thread_fn() -> void
    {
        auto port_handle = completion_port.native_handle();

        for (;;) {
            DWORD bytes_transferred;
            ULONG_PTR completion_key;
            iocp_coro_state *overlapped = nullptr;

            auto ret = ::GetQueuedCompletionStatus(
                port_handle,
                &bytes_transferred,
                &completion_key,
                reinterpret_cast<OVERLAPPED **>(&overlapped),
                INFINITE);

            if (overlapped == nullptr)
                // Happens when our completion port is closed.
                return;

            {
                std::lock_guard lock(overlapped->mutex);

                if (overlapped->was_pending) {
                    overlapped->success = ret;
                    if (!overlapped->success)
                        overlapped->error = ::GetLastError();
                    else
                        overlapped->error = 0;
                    overlapped->bytes_transferred = bytes_transferred;
                }
            }

            auto &h = overlapped->coro_handle;
            sk::detail::check(
                overlapped->executor != nullptr,
                "iocp_reactor: trying to resume without executor");

            overlapped->executor->post([&] { h.resume(); });
        }
    }

    inline auto iocp_reactor::start() -> void
    {
        WSADATA wsadata;
        ::WSAStartup(MAKEWORD(2, 2), &wsadata);

        completion_thread =
            std::jthread(&iocp_reactor::completion_thread_fn, this);
    }

    inline auto iocp_reactor::stop() -> void
    {
        completion_port.close();
        completion_thread.join();
    }

    inline auto iocp_reactor::associate_handle(HANDLE h) -> void
    {
        ::CreateIoCompletionPort(h, completion_port.native_handle(), 0, 0);
    }

}; // namespace sk::win32::detail

#endif // SK_CIO_WIN32_IOCP_REACTOR_HXX_INCLUDED
