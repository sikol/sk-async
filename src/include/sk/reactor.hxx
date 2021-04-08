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

#ifndef SK_REACTOR_HXX_INCLUDED
#define SK_REACTOR_HXX_INCLUDED

#include <mutex>

#include <sk/detail/platform.hxx>

#if defined(SK_CIO_PLATFORM_WINDOWS)
#    include <sk/win32/detail/iocp_reactor.hxx>

namespace sk {

    using system_reactor_type = win32::detail::iocp_reactor;

} // namespace sk

#elif defined(SK_CIO_PLATFORM_LINUX)
#    include <sk/posix/detail/linux_reactor.hxx>

namespace sk {

    using system_reactor_type = posix::detail::linux_reactor;

} // namespace sk

#else

#    error reactor is not supported on this platform

#endif

namespace sk {

    struct reactor_handle {
        reactor_handle();
        ~reactor_handle();

        reactor_handle(reactor_handle const &) = delete;
        reactor_handle(reactor_handle &&) = delete;
        auto operator=(reactor_handle const &) -> reactor_handle & = delete;
        auto operator=(reactor_handle &&) -> reactor_handle & = delete;

        // Fetch the global reactor handle.
        static auto get_global_reactor() -> system_reactor_type &;

    private:
        inline static int _refs;
        inline static std::mutex _mutex;
    };

    inline reactor_handle::reactor_handle()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (++_refs > 1)
            return;

        get_global_reactor().start();
    }

    inline reactor_handle::~reactor_handle()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (--_refs > 0)
            return;

        get_global_reactor().stop();
    }

    inline auto reactor_handle::get_global_reactor() -> system_reactor_type &
    {
        static system_reactor_type global_reactor;
        return global_reactor;
    }

} // namespace sk

#endif // SK_REACTOR_HXX_INCLUDED
