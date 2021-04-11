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

#ifndef SK_DETAIL_STRPARSE_HXX
#define SK_DETAIL_STRPARSE_HXX

#include <charconv>
#include <string_view>
#include <ranges>
#include <cctype>

namespace sk::detail {

    // If `v` begins with an unsigned integer, remove it and return it and the
    // rest of the string.  Otherwise, return nullopt and the original string.
    template<typename T>
    inline auto span_number(std::string_view v, int base = 10)
        -> std::pair<std::optional<T>, std::string_view>
    {
        T i;

        auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), i, base);
        if (ec != std::errc())
            return {{}, v};

        return {i, v.substr(p - v.data())};
    }

} // namespace sk::detail

#endif // SK_DETAIL_STRPARSE_HXX
