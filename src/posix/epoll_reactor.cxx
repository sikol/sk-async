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

#include <sk/cio/detail/config.hxx>

#if defined(SK_CIO_PLATFORM_LINUX)

#    include <sys/epoll.h>
#    include <cassert>
#    include <fcntl.h>

#    include <sk/cio/posix/epoll_reactor.hxx>
#    include <sk/cio/posix/error.hxx>
#    include <sk/cio/reactor.hxx>

namespace sk::cio::posix {

    epoll_reactor::epoll_reactor()
    {
        ::pipe(shutdown_pipe);

        auto epoll_fd_ = ::epoll_create(64);
        epoll_fd.assign(epoll_fd_);

        struct epoll_event shutdown_ev;
        std::memset(&shutdown_ev, 0, sizeof(shutdown_ev));
        shutdown_ev.data.fd = shutdown_pipe[0];
        shutdown_ev.events = EPOLLET | EPOLLONESHOT | EPOLLIN;
        ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, shutdown_pipe[0], &shutdown_ev);
    }

    auto epoll_reactor::epoll_thread_fn() -> void
    {
        auto ep = epoll_fd.fd();

        epoll_event events[16];
        int nevents;

        //std::cerr << "epoll_reactor : starting\n";
        while ((nevents = epoll_wait(
                    ep, events, sizeof(events) / sizeof(*events), -1)) != -1) {

            //std::cerr << "epoll_reactor : got events = " << nevents << '\n';
            std::lock_guard state_lock(_state_mtx);

            for (int i = 0; i < nevents; ++i) {
                auto &event = events[i];
                auto fd = event.data.fd;

                if (fd == shutdown_pipe[0])
                    return;

                auto &state = _state[fd];
//                std::cerr << "epoll_reactor : event on fd=" << fd << '\n';

                if (event.events & EPOLLIN) {
                    //std::cerr << "epoll_reactor : EPOLLIN, read_waiter = " << state->read_waiter << '\n';
                    if (state->read_waiter) {
                        //std::cerr << "epoll_reactor : resuming the read_waiter\n";
                        std::lock_guard h_lock(state->read_waiter->mutex);
                        auto h = state->read_waiter->coro_handle;
                        state->read_waiter = nullptr;
                        state->event.events &= ~EPOLLIN;
                        _workq.post([=] { h.resume(); });
                    }
                }

                if (event.events & EPOLLOUT) {
                    //std::cerr << "epoll_reactor : EPOLLOUT, write_waiter = " << state->read_waiter << '\n';
                    if (state->write_waiter) {
                        //std::cerr << "epoll_reactor : resuming the write_waiter\n";
                        std::lock_guard h_lock(state->write_waiter->mutex);
                        auto h = state->write_waiter->coro_handle;
                        state->write_waiter = nullptr;
                        state->event.events &= ~EPOLLOUT;
                        _workq.post([=] { h.resume(); });
                    }
                }
            }

            //std::cerr << "epoll_reactor: waiting\n";
        }
    }

    auto epoll_reactor::start() -> void
    {
        epoll_thread = std::jthread(&epoll_reactor::epoll_thread_fn, this);
        _workq.start_threads();
    }

    auto epoll_reactor::stop() -> void
    {
        //std::cerr << "epoll_reactor: requesting stop\n";
        ::write(shutdown_pipe[1], "", 1);
        _workq.stop();
        epoll_fd.close();
        epoll_thread.join();
    }

    auto epoll_reactor::associate_fd(int fd) -> void
    {
        //std::cerr << "epoll_reactor : associate_fd " << fd << '\n';
        std::lock_guard lock(_state_mtx);

        if (_state.size() < (fd + 1))
            _state.resize(fd + 1);

        if (!_state[fd])
            _state[fd] = std::make_unique<fd_state>(fd);

        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        _state[fd]->event.events = EPOLLET | EPOLLONESHOT;
        int r = epoll_ctl(epoll_fd.fd(), EPOLL_CTL_ADD, fd, &_state[fd]->event);
        assert(r == 0);
    }

    auto epoll_reactor::deassociate_fd(int fd) -> void
    {
        std::lock_guard lock(_state_mtx);

        int r = epoll_ctl(epoll_fd.fd(), EPOLL_CTL_DEL, fd, nullptr);
        assert(r == 0);
        _state[fd].reset();
    }

    auto epoll_reactor::post(std::function<void()> fn) -> void
    {
        _workq.post(std::move(fn));
    }

    auto epoll_reactor::register_read_interest(int fd, epoll_coro_state *cstate)
        -> void
    {
        //std::cerr << "epoll_reactor : register_read_interest " << fd << '\n';
        std::lock_guard lock(_state_mtx);

        assert(_state.size() > fd);

        auto &state = _state[fd];
        assert(state);
        assert(!state->write_waiter);
        assert(!(state->event.events & EPOLLIN));

        state->read_waiter = cstate;
        state->event.events |= EPOLLIN;
        int r = epoll_ctl(epoll_fd.fd(), EPOLL_CTL_MOD, fd, &state->event);
        assert(r == 0);
    }

    auto epoll_reactor::register_write_interest(int fd,
                                                epoll_coro_state *cstate)
        -> void
    {
        //std::cerr << "epoll_reactor : register_write_interest " << fd << '\n';
        std::lock_guard lock(_state_mtx);

        assert(_state.size() > fd);

        auto &state = _state[fd];
        assert(state);
        assert(!state->write_waiter);
        assert(!(state->event.events & EPOLLIN));

        state->write_waiter = cstate;
        state->event.events |= EPOLLIN;
        int r = epoll_ctl(epoll_fd.fd(), EPOLL_CTL_MOD, fd, &state->event);
        assert(r == 0);
    }

} // namespace sk::cio::posix

#endif // SK_CIO_PLATFORM_LINUX
