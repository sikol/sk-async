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

#ifndef SK_STATIC_RANGE_HXX_INCLUDED
#define SK_STATIC_RANGE_HXX_INCLUDED

#include <cstddef>
#include <initializer_list>
#include <type_traits>

namespace sk {

    /*************************************************************************
     *
     * static_range: a very simple container that stores up to a fixed number
     * of items.  unlike vector<>, static_range can be stack allocated.
     *
     * static_range is not exception-safe and should only be used to store
     * objects with non-throwing constructors.
     *
     * static_range currently only exists for internal use by the buffers
     * and is not intended to be a general-purpose container.
     */

    template <typename T, std::size_t max_size>
    struct static_range {
        using value_type = T;
        using const_value_type = const value_type;

        using iterator = T *;
        using const_iterator = T const *;
        using size_type = std::size_t;

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        static_range() noexcept = default;

        static_range(std::initializer_list<T> items)
        {
            if (items.size() > max_size)
                throw std::bad_alloc();

            for (auto &&item : items)
                new (&_data[_size++]) T(std::move(item)); // NOLINT
        }

        static_range(static_range const &other) noexcept : _size(other._size)
        {
            for (std::size_t i = 0; i < _size; ++i)
                new (&_data[i]) T(other._data[i]); // NOLINT
        }

        static_range(static_range &&other) noexcept : _size(other._size)
        {
            for (std::size_t i = 0; i < _size; ++i)
                new (&_data[i]) T(std::move(other[i])); // NOLINT
        }

        auto operator=(static_range const &other) noexcept -> static_range &
        {
            if (&other == this)
                return *this;

            for (std::size_t i = 0; i < _size; ++i)
                (*this)[i].~T(); // NOLINT

            for (std::size_t i = 0; i < other._size; ++i) {
                new (&_data[i]) T(other[i]); // NOLINT
            }

            _size = other._size;
            return *this;
        }

        auto operator=(static_range &&other) noexcept -> static_range &
        {
            for (std::size_t i = 0; i < _size; ++i)
                (*this)[i].~T(); // NOLINT

            for (std::size_t i = 0; i < other._size; ++i) {
                new (&_data[i]) T(std::move(other[i])); // NOLINT
                other[i].~T();                          // NOLINT
            }

            _size = std::exchange(other._size, 0);
            return *this;
        }

        ~static_range()
        {
            for (std::size_t i = 0; i < _size; ++i)
                (*this)[i].~T(); // NOLINT
        }

        [[nodiscard]] auto operator[](size_type n) noexcept -> value_type &
        {
            return *(begin() + n);
        }

        [[nodiscard]] auto operator[](size_type n) const noexcept -> const_value_type &
        {
            return *(begin() + n);
        }

        [[nodiscard]] auto size() const noexcept -> size_type
        {
            return _size;
        }

        [[nodiscard]] auto capacity() const noexcept -> size_type
        {
            return max_size;
        }

        [[nodiscard]] auto begin() noexcept -> iterator
        {
            return std::launder(reinterpret_cast<value_type *>(&_data[0]));
        }

        [[nodiscard]] auto begin() const noexcept -> const_iterator
        {
            return std::launder(
                reinterpret_cast<const_value_type *>(&_data[0]));
        }

        [[nodiscard]] auto end() noexcept -> iterator
        {
            return begin() + size();
        }

        [[nodiscard]] auto end() const noexcept -> const_iterator
        {
            return begin() + size();
        }

        auto push_back(T const &o) -> void
        {
            if (size() == capacity())
                throw std::bad_alloc();

            new (&_data[_size]) T(o); // NOLINT
            ++_size;
        }

        auto push_back(T &&o) -> void
        {
            if (size() == capacity())
                throw std::bad_alloc();

            new (&_data[_size]) T(o); // NOLINT
            ++_size;
        }

        template <typename... Args>
        auto emplace_back(Args &&...args)
        {
            if (size() == capacity())
                throw std::bad_alloc();

            new (&_data[_size]) T(std::forward<Args>(args)...); // NOLINT
            ++_size;
        }

    private:
        using storage_type =
            typename std::aligned_storage<sizeof(value_type),
                                          alignof(value_type)>::type;
        static_assert(sizeof(storage_type) == sizeof(value_type));

        // NOLINTNEXTLINE
        storage_type _data[max_size];
        size_type _size{};
    };

} // namespace sk
#endif // SK_STATIC_RANGE_HXX_INCLUDED
