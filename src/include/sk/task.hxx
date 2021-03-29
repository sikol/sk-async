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

#ifndef SK_CIO_TASK_HXX_INCLUDED
#define SK_CIO_TASK_HXX_INCLUDED

#include <chrono>
#include <concepts>
#include <future>
#include <iostream>
#include <optional>
#include <thread>
#include <utility>

#include <sk/detail/coroutine.hxx>

namespace sk {
    /*************************************************************************
     *
     * promise_base
     */

    template <typename T>
    struct promise_base {
        void return_value(T const &value) noexcept(
            std::is_nothrow_copy_constructible_v<T>)
        {
            result = value;
        }

        void return_value(T &&value) noexcept(
            std::is_nothrow_move_constructible_v<T>)
        {
            result = std::move(value);
        }

        auto await_resume() -> T
        {
            return std::move(result);
        }
        T result{};
    };

    template <>
    struct promise_base<void> {
        void return_void() noexcept {}
        auto await_resume() -> void {}
    };

    /*************************************************************************
     *
     * task
     */

    template <typename T>
    struct task {
        struct promise_type : promise_base<T> {
#ifndef SK_HAS_CPP20_COROUTINES
            std::atomic<bool> ready = false;
#endif
            coroutine_handle<> previous;
            std::exception_ptr exception{nullptr};

            auto get_return_object()
            {
                return task(
                    coroutine_handle<promise_type>::from_promise(*this));
            }

            auto initial_suspend() -> suspend_always
            {
                return {};
            }

            struct final_awaiter {
                auto await_ready() noexcept -> bool
                {
                    return false;
                }

                void await_resume() noexcept {}

#ifdef SK_HAS_STD_COROUTINES
                // NOLINTNEXTLINE(bugprone-exception-escape)
                coroutine_handle<>
                await_suspend(coroutine_handle<promise_type> h) noexcept
                {
                    auto &prev = h.promise().previous;
                    if (prev)
                        return prev;

                    return std::noop_coroutine();
                }
#else
                // NOLINTNEXTLINE(bugprone-exception-escape)
                void await_suspend(coroutine_handle<promise_type> h) noexcept
                {
                    auto &promise = h.promise();

                    if (!promise.previous)
                        return;

                    if (promise.ready.exchange(true, std::memory_order_acq_rel))
                        promise.previous.resume();
                }
#endif
            };

            auto final_suspend() noexcept -> final_awaiter
            {
                return {};
            }

            void unhandled_exception()
            {
                exception = std::current_exception();
            }
        };

        coroutine_handle<promise_type> coro_handle;

        explicit task(coroutine_handle<promise_type> coro_handle_) noexcept
            : coro_handle(coro_handle_)
        {
        }

        task(task const &) = delete;
        auto operator=(task const &) -> task & = delete;
        auto operator=(task &&other) -> task & = delete;

        task(task &&other) noexcept
            : coro_handle(std::exchange(other.coro_handle, {}))
        {
        }

        ~task()
        {
            if (coro_handle) {
                try {
                    coro_handle.destroy();
                } catch (...) {
                    std::terminate();
                }
            }
        }

        auto await_ready() noexcept -> bool
        {
            return false;
        }

#ifdef SK_HAS_STD_COROUTINES
        // NOLINTNEXTLINE(bugprone-exception-escape)
        auto await_suspend(coroutine_handle<> h) noexcept
        {
            coro_handle.promise().previous = h;
            return coro_handle;
        }
#else
        // NOLINTNEXTLINE(bugprone-exception-escape)
        auto await_suspend(coroutine_handle<> h) noexcept -> bool
        {
            auto &promise = coro_handle.promise();
            promise.previous = h;
            coro_handle.resume();
            return !promise.ready.exchange(true, std::memory_order_acq_rel);
        }
#endif

        auto await_resume() -> T
        {
            auto &promise = coro_handle.promise();
            if (promise.exception)
                std::rethrow_exception(promise.exception);
            return coro_handle.promise().await_resume();
        }

        void start()
        {
            coro_handle.resume();
        }
    };

} // namespace sk

#endif // SK_CIO_TASK_HXX_INCLUDED
