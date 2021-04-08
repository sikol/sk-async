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

#ifndef SK_CIO_POSIX_EPOLL_REACTOR_HXX_INCLUDED
#define SK_CIO_POSIX_EPOLL_REACTOR_HXX_INCLUDED

#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <cstring>
#include <iostream>
#include <system_error>
#include <thread>
#include <cassert>

#include <sk/async_invoke.hxx>
#include <sk/expected.hxx>
#include <sk/posix/error.hxx>
#include <sk/posix/fd.hxx>
#include <sk/task.hxx>
#include <sk/workq.hxx>

namespace sk::posix::detail {

    struct epoll_coro_state {
        bool was_pending;
        int ret;
        int error;
        coroutine_handle<> coro_handle;
        std::mutex mutex;
    };

    struct fd_state final {
        explicit fd_state(int fd_)
            : fd(fd_), read_waiter(nullptr), write_waiter(nullptr)
        {
            std::memset(&event, 0, sizeof(event));
            event.data.fd = fd;
            event.events = EPOLLET | EPOLLONESHOT;
        }

        int fd;
        epoll_event event{};
        epoll_coro_state *read_waiter;
        epoll_coro_state *write_waiter;
    };

    struct epoll_reactor final {

        explicit epoll_reactor(workq *);
        ~epoll_reactor() = default;

        // Not copyable.
        epoll_reactor(epoll_reactor const &) = delete;
        auto operator=(epoll_reactor const &) -> epoll_reactor & = delete;

        // Not movable.
        epoll_reactor(epoll_reactor &&) noexcept = delete;
        auto operator=(epoll_reactor &&) noexcept -> epoll_reactor & = delete;

        unique_fd epoll_fd;

        // Associate a new fd with our epoll.
        auto associate_fd(int) -> void;
        auto deassociate_fd(int) -> void;

        // Register interest in an fd
        auto register_read_interest(int fd, epoll_coro_state *state) -> void;
        auto register_write_interest(int fd, epoll_coro_state *state) -> void;

        // Start this reactor.
        auto start() -> void;

        // Stop this reactor.
        auto stop() -> void;

        // Post work to the reactor's thread pool.
        auto post(std::function<void()> fn) -> void;

        // POSIX async API
        [[nodiscard]] auto
        async_fd_open(char const *path, int flags, int mode = 0777)
            -> task<expected<int, std::error_code>>;

        [[nodiscard]] auto async_fd_close(int fd)
            -> task<expected<int, std::error_code>>;

        [[nodiscard]] auto async_fd_read(int fd, void *buf, std::size_t n)
            -> task<expected<ssize_t, std::error_code>>;

        [[nodiscard]] auto
        async_fd_pread(int fd, void *buf, std::size_t n, off_t offs)
            -> task<expected<ssize_t, std::error_code>>;

        [[nodiscard]] auto
        async_fd_recv(int fd, void *buf, std::size_t n, int flags)
            -> task<expected<ssize_t, std::error_code>>;

        [[nodiscard]] auto
        async_fd_send(int fd, void const *buf, std::size_t n, int flags)
            -> task<expected<ssize_t, std::error_code>>;

        [[nodiscard]] auto
        async_fd_write(int fd, void const *buf, std::size_t n)
            -> task<expected<ssize_t, std::error_code>>;

        [[nodiscard]] auto
        async_fd_pwrite(int fd, void const *buf, std::size_t n, off_t offs)
            -> task<expected<ssize_t, std::error_code>>;

        [[nodiscard]] auto async_fd_connect(int, sockaddr const *, socklen_t)
            -> task<expected<void, std::error_code>>;

        [[nodiscard]] auto async_fd_accept(int, sockaddr *addr, socklen_t *)
            -> task<expected<int, std::error_code>>;

    private:
        workq &_workq;

        std::mutex _state_mtx;
        std::vector<std::unique_ptr<fd_state>> _state;

        void epoll_thread_fn();
        std::thread _epoll_thread;
        std::array<int, 2> _shutdown_pipe;
    };

    inline epoll_reactor::epoll_reactor(workq *q) : _workq(*q)
    {
        ::pipe(_shutdown_pipe.data());

        auto epoll_fd_ = ::epoll_create(64);
        epoll_fd.assign(epoll_fd_);

        epoll_event shutdown_ev{};
        shutdown_ev.data.fd = _shutdown_pipe[0];
        shutdown_ev.events = EPOLLET | EPOLLONESHOT | EPOLLIN;
        ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, _shutdown_pipe[0], &shutdown_ev);
    }

    inline auto epoll_reactor::epoll_thread_fn() -> void
    {
        auto ep = *epoll_fd;

        epoll_event events[16];
        int nevents;

        while ((nevents = epoll_wait(
                    ep, events, sizeof(events) / sizeof(*events), -1)) != -1) {
            std::lock_guard state_lock(_state_mtx);

            for (int i = 0; i < nevents; ++i) {
                auto &event = events[i];
                auto fd = event.data.fd;

                if (fd == _shutdown_pipe[0])
                    return;

                auto &state = _state[fd];

                if (event.events & EPOLLIN) {
                    if (state->read_waiter) {
                        std::lock_guard h_lock(state->read_waiter->mutex);
                        auto &h = state->read_waiter->coro_handle;
                        state->read_waiter = nullptr;
                        state->event.events &= ~EPOLLIN;
                        _workq.post([&] { h.resume(); });
                    }
                }

                if (event.events & EPOLLOUT) {
                    if (state->write_waiter) {
                        std::lock_guard h_lock(state->write_waiter->mutex);
                        auto h = state->write_waiter->coro_handle;
                        state->write_waiter = nullptr;
                        state->event.events &= ~EPOLLOUT;
                        _workq.post([&] { h.resume(); });
                    }
                }
            }
        }
    }

    inline auto epoll_reactor::start() -> void
    {
        _epoll_thread = std::thread(&epoll_reactor::epoll_thread_fn, this);
        _workq.start_threads();
    }

    inline auto epoll_reactor::stop() -> void
    {
        ::write(_shutdown_pipe[1], "", 1);
        _workq.stop();
        epoll_fd.close();
        _epoll_thread.join();
    }

    inline auto epoll_reactor::associate_fd(int fd) -> void
    {
        std::lock_guard lock(_state_mtx);

        sk::detail::check(fd >= 0, "attempt to associate a negative fd");

        if (_state.size() < static_cast<std::size_t>(fd + 1))
            _state.resize(fd + 1);

        if (!_state[fd])
            _state[fd] = std::make_unique<fd_state>(fd);

        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        _state[fd]->event.events = EPOLLET | EPOLLONESHOT;
        int r = epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, fd, &_state[fd]->event);
        assert(r == 0);
    }

    inline auto epoll_reactor::deassociate_fd(int fd) -> void
    {
        std::lock_guard lock(_state_mtx);

        sk::detail::check(fd >= 0, "attempt to deassociate a negative fd");

        int r = epoll_ctl(*epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
        assert(r == 0);
        _state[fd].reset();
    }

    inline auto epoll_reactor::post(std::function<void()> fn) -> void
    {
        _workq.post(std::move(fn));
    }

    inline auto epoll_reactor::register_read_interest(int fd,
                                                      epoll_coro_state *cstate)
        -> void
    {
        std::lock_guard lock(_state_mtx);

        sk::detail::check(fd >= 0, "attempt to register a negative fd");
        assert(_state.size() > static_cast<std::size_t>(fd));

        auto &state = _state[fd];
        assert(state);
        assert(!state->write_waiter);
        assert(!(state->event.events & EPOLLIN));

        state->read_waiter = cstate;
        state->event.events |= EPOLLIN;
        int r = epoll_ctl(*epoll_fd, EPOLL_CTL_MOD, fd, &state->event);
        assert(r == 0);
    }

    inline auto epoll_reactor::register_write_interest(int fd,
                                                       epoll_coro_state *cstate)
        -> void
    {
        std::lock_guard lock(_state_mtx);

        sk::detail::check(fd >= 0, "attempt to register a negative fd");

        assert(_state.size() > static_cast<std::size_t>(fd));

        auto &state = _state[fd];
        assert(state);
        assert(!state->write_waiter);
        assert(!(state->event.events & EPOLLIN));

        state->write_waiter = cstate;
        state->event.events |= EPOLLIN;
        int r = epoll_ctl(*epoll_fd, EPOLL_CTL_MOD, fd, &state->event);
        assert(r == 0);
    }

    /*************************************************************************
     *
     * POSIX async API.
     *
     */

    /*
     * Socket operations.
     */

    struct co_fd_is_readable {
        epoll_reactor &reactor;
        int fd;
        epoll_coro_state cstate;

        explicit co_fd_is_readable(epoll_reactor &reactor_, int fd_)
            : reactor(reactor_), fd(fd_)
        {
        }

        bool await_ready()
        {
            return false;
        }

        bool await_suspend(coroutine_handle<> coro_handle_)
        {
            std::lock_guard lock(cstate.mutex);
            cstate.coro_handle = coro_handle_;
            reactor.register_read_interest(fd, &cstate);
            return true;
        }

        void await_resume() {}
    };

    struct co_fd_is_writable {
        epoll_reactor &reactor;
        int fd;
        epoll_coro_state cstate;

        explicit co_fd_is_writable(epoll_reactor &reactor_, int fd_)
            : reactor(reactor_), fd(fd_)
        {
        }

        bool await_ready()
        {
            return false;
        }

        bool await_suspend(coroutine_handle<> coro_handle_)
        {
            std::lock_guard lock(cstate.mutex);
            cstate.coro_handle = coro_handle_;
            reactor.register_write_interest(fd, &cstate);
            return true;
        }

        void await_resume() {}
    };

    inline auto
    epoll_reactor::async_fd_recv(int fd, void *buf, std::size_t n, int flags)
        -> task<expected<ssize_t, std::error_code>>
    {
        do {
            auto ret = ::recv(fd, buf, n, flags);
            if (ret != -1)
                co_return ret;

            if (errno != EWOULDBLOCK)
                co_return make_unexpected(get_errno());

            co_await co_fd_is_readable(*this, fd);
        } while (true);
    }

    inline auto epoll_reactor::async_fd_send(int fd,
                                             void const *buf,
                                             std::size_t n,
                                             int flags)
        -> task<expected<ssize_t, std::error_code>>
    {
        do {
            auto ret = ::send(fd, buf, n, flags);
            if (ret != -1)
                co_return ret;

            if (errno != EWOULDBLOCK)
                co_return make_unexpected(get_errno());

            co_await co_fd_is_writable(*this, fd);
        } while (true);
    }

    inline auto epoll_reactor::async_fd_connect(int fd,
                                                sockaddr const *addr,
                                                socklen_t addrlen)
        -> task<expected<void, std::error_code>>
    {
        auto ret = ::connect(fd, addr, addrlen);

        if (ret == 0)
            co_return {};

        if (errno != EWOULDBLOCK)
            co_return make_unexpected(get_errno());

        co_await co_fd_is_writable(*this, fd);
        co_return {};
    }

    inline auto
    epoll_reactor::async_fd_accept(int fd, sockaddr *addr, socklen_t *addrlen)
        -> task<expected<int, std::error_code>>
    {
        do {
            auto ret = ::accept(fd, addr, addrlen);
            if (ret != -1)
                co_return ret;

            if (errno != EWOULDBLOCK)
                co_return make_unexpected(get_errno());

            co_await co_fd_is_readable(*this, fd);
        } while (true);
    }

    /*
     * File I/O operations.  This is a very naive implementation based on
     * thread dispatch; it doesn't scale well and will only be used if no
     * better file I/O mechanism (e.g, io_uring) is available.
     */
    inline auto
    epoll_reactor::async_fd_open(char const *path, int flags, int mode)
        -> task<expected<int, std::error_code>>
    {
        int fd = co_await async_invoke([&]() -> int {
            int r = ::open(path, flags, mode);
            return r >= 0 ? r : -errno;
        });

        if (fd < 0)
            co_return make_unexpected(
                std::error_code(-fd, std::system_category()));
        else
            co_return fd;
    }

    inline auto epoll_reactor::async_fd_close(int fd)
        -> task<expected<int, std::error_code>>
    {
        auto ret = co_await async_invoke([&]() -> int {
            int r = ::close(fd);
            return r >= 0 ? r : -errno;
        });

        if (ret == -1)
            co_return make_unexpected(
                std::error_code(-ret, std::system_category()));
        else
            co_return fd;
    }

    inline auto epoll_reactor::async_fd_read(int fd, void *buf, std::size_t n)
        -> task<expected<ssize_t, std::error_code>>
    {
        ssize_t ret = co_await async_invoke([&]() -> ssize_t {
            ssize_t r = ::read(fd, buf, n);
            return r >= 0 ? r : -errno;
        });

        if (ret < 0)
            co_return make_unexpected(
                std::error_code(-ret, std::system_category()));
        else
            co_return ret;
    }

    inline auto
    epoll_reactor::async_fd_pread(int fd, void *buf, std::size_t n, off_t offs)
        -> task<expected<ssize_t, std::error_code>>
    {
        ssize_t ret = co_await async_invoke([&]() -> ssize_t {
            ssize_t r = ::pread(fd, buf, n, offs);
            return r >= 0 ? r : -errno;
        });

        if (ret < 0)
            co_return make_unexpected(
                std::error_code(-ret, std::system_category()));
        else
            co_return ret;
    }

    inline auto
    epoll_reactor::async_fd_write(int fd, void const *buf, std::size_t n)
        -> task<expected<ssize_t, std::error_code>>
    {
        ssize_t ret = co_await async_invoke([&]() -> ssize_t {
            ssize_t r = ::write(fd, buf, n);
            return r >= 0 ? r : -errno;
        });

        if (ret < 0)
            co_return make_unexpected(
                std::error_code(-ret, std::system_category()));
        else
            co_return ret;
    }

    inline auto epoll_reactor::async_fd_pwrite(int fd,
                                               void const *buf,
                                               std::size_t n,
                                               off_t offs)
        -> task<expected<ssize_t, std::error_code>>
    {
        ssize_t ret = co_await async_invoke([&]() -> ssize_t {
            ssize_t r = ::pwrite(fd, buf, n, offs);
            return r >= 0 ? r : -errno;
        });

        if (ret < 0)
            co_return make_unexpected(
                std::error_code(-ret, std::system_category()));
        else
            co_return ret;
    }

} // namespace sk::posix::detail

#endif // SK_CIO_POSIX_EPOLL_REACTOR_HXX_INCLUDED
