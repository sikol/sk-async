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

#ifndef SK_CIO_FILECHANNEL_OSEQFILECHANNEL_HXX_INCLUDED
#define SK_CIO_FILECHANNEL_OSEQFILECHANNEL_HXX_INCLUDED

#include <cstddef>
#include <filesystem>
#include <system_error>

#include <sk/channel/concepts.hxx>
#include <sk/channel/error.hxx>
#include <sk/channel/filechannel/detail/filechannel_base.hxx>
#include <sk/channel/filechannel/filechannel.hxx>
#include <sk/channel/types.hxx>
#include <sk/task.hxx>

namespace sk {

    /*************************************************************************
     *
     * oseqfilechannel: a direct access channel that writes to a file.
     */

    // clang-format off
    struct oseqfilechannel final : detail::seqfilechannel_base {

        /*
         * Create an oseqfilechannel which is closed.
         */
        oseqfilechannel() = default;

        /*
         * Open a file.
         */
        [[nodiscard]]
        auto async_open(std::filesystem::path const &,
                        fileflags_t = fileflags::none)
        -> task<expected<void, std::error_code>>;

        [[nodiscard]]
        auto open(std::filesystem::path const &,
                  fileflags_t = fileflags::none)
        -> expected<void, std::error_code>;

        /*
         * Write data.
         */
        [[nodiscard]]
        auto async_write_some(std::byte const *buffer, io_size_t)
        -> task<expected<io_size_t, std::error_code>>;

        [[nodiscard]]
        auto write_some(std::byte const *buffer, io_size_t)
        -> expected<io_size_t, std::error_code>;

        oseqfilechannel(oseqfilechannel const &) = delete;
        oseqfilechannel(oseqfilechannel &&) noexcept = default;
        auto operator=(oseqfilechannel const &) -> oseqfilechannel & = delete;
        auto operator=(oseqfilechannel &&) noexcept -> oseqfilechannel & = default;
        ~oseqfilechannel() = default;
    };

    // clang-format on

    static_assert(oseqchannel<oseqfilechannel>);

    /*************************************************************************
     * oseqfilechannel::async_open()
     */
    inline auto oseqfilechannel::async_open(std::filesystem::path const &path,
                                            fileflags_t flags)
        -> task<expected<void, std::error_code>>
    {

        if (flags & fileflags::read)
            co_return make_unexpected(sk::error::filechannel_invalid_flags);

        flags |= fileflags::write;
        co_return co_await this->_async_open(path, flags);
    }

    /*************************************************************************
     * oseqfilechannel::open()
     */
    inline auto oseqfilechannel::open(std::filesystem::path const &path,
                                      fileflags_t flags)
        -> expected<void, std::error_code>
    {

        if (flags & fileflags::read)
            return make_unexpected(sk::error::filechannel_invalid_flags);

        flags |= fileflags::write;
        return this->_open(path, flags);
    }

    /*************************************************************************
     * oseqfilechannel::async_write_some()
     */

    inline auto oseqfilechannel::async_write_some(std::byte const *buffer,
                                                  io_size_t nobjs)
        -> task<expected<io_size_t, std::error_code>>
    {
        return _async_write_some(buffer, nobjs);
    }

    /*************************************************************************
     * oseqfilechannel::write_some()
     */

    inline auto oseqfilechannel::write_some(std::byte const *buffer,
                                            io_size_t nobjs)
        -> expected<io_size_t, std::error_code>
    {
        return _write_some(buffer, nobjs);
    }

} // namespace sk

#endif // SK_CIO_FILECHANNEL_OSEQFILECHANNEL_HXX_INCLUDED
