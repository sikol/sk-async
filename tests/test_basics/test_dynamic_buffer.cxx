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

#include <algorithm>
#include <array>
#include <numeric>
#include <ranges>

#include <catch.hpp>

#include "sk/buffer/dynamic_buffer.hxx"

TEST_CASE("dynamic_buffer<>, small char buffer")
{
    std::string input_string =
        "this is a long test string that will fill several extents";
    sk::dynamic_buffer<char, sk::dynamic_buffer_size(3)> buf;

    buffer_write(buf, input_string);

    std::string output_string(input_string.size(), 'A');
    auto nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == input_string.size());
    REQUIRE(output_string == input_string);

    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);

    // repeat the test to make sure the buffer can handle being emptied
    // then filled again.
    buffer_write(buf, input_string);

    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == input_string.size());
    REQUIRE(output_string == input_string);

    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);

    // now read a few bytes at a time
    buffer_write(buf, input_string);

    output_string.resize(0);

    for (;;) {
        std::array<char, 3> data{};
        nbytes = buffer_read(buf, data);
        if (nbytes == 0)
            break;
        output_string.append(data.begin(), data.begin() + nbytes);
    }

    REQUIRE(output_string == input_string);
    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);
}

TEST_CASE("dynamic_buffer<>, small char buffer, commit/discard")
{
    std::string input_string =
        "this is a long test string that will fill several extents";
    sk::dynamic_buffer<char, sk::dynamic_buffer_size(3)> buf;

    // Write into the buffer.
    std::span<char const> inbuf(input_string);
    while (!inbuf.empty()) {
        auto writable_ranges = buf.writable_ranges();

        for (auto &&range : writable_ranges) {
            if (inbuf.empty())
                break;

            auto can_write = std::min(range.size(), inbuf.size());
            std::ranges::copy(inbuf.subspan(0, can_write),
                              std::ranges::begin(range));
            buf.commit(can_write);
            inbuf = inbuf.subspan(can_write);
        }
    }

    // Read the buffer into the output string.
    std::string output_string(input_string.size(), 'A');
    std::span<char> outbuf(output_string);

    while (!outbuf.empty()) {
        auto readable_ranges = buf.readable_ranges();

        for (auto &&range : readable_ranges) {
            if (outbuf.empty())
                break;

            auto can_read = std::min(range.size(), outbuf.size());
            std::ranges::copy(range.subspan(0, can_read),
                              std::ranges::begin(outbuf));
            buf.discard(can_read);
            outbuf = outbuf.subspan(can_read);
        }
    }

    REQUIRE(output_string == input_string);

    auto nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);
}

TEST_CASE("dynamic_buffer<>, small char buffer, single-byte writes")
{
    std::string input_string =
        "this is a long test string that will fill several extents";
    sk::dynamic_buffer<char, sk::dynamic_buffer_size(3)> buf;

    for (char const &c : input_string)
        buf.write(&c, 1u);

    std::string output_string(input_string.size(), 'A');
    auto nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == input_string.size());
    REQUIRE(output_string == input_string);

    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);
}

TEST_CASE("dynamic_buffer<>, small char buffer, two-byte writes")
{
    std::string input_string =
        "this is a long test string that will fill several extents";
    sk::dynamic_buffer<char, sk::dynamic_buffer_size(3)> buf;

    for (auto it = input_string.begin(), end = input_string.end(); it != end;) {
        if ((it + 1) == end) {
            buffer_write(buf, std::span(it, end));
            break;
        }

        buffer_write(buf, std::span(it, it + 2));
        it += 2;
    }

    std::string output_string(input_string.size(), 'A');
    auto nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == input_string.size());
    REQUIRE(output_string == input_string);

    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);
}

TEST_CASE("dynamic_buffer<>, large char buffer")
{
    std::string input_string =
        "this is a test string that will not fill more than one extent";
    sk::dynamic_buffer<char, 4096> buf;

    buffer_write(buf, input_string);

    std::string output_string(input_string.size(), 'A');
    auto nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == input_string.size());
    REQUIRE(output_string == input_string);

    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);

    // repeat the test to make sure the buffer can handle being emptied
    // then filled again.
    buffer_write(buf, input_string);

    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == input_string.size());
    REQUIRE(output_string == input_string);

    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);

    // now read a few bytes at a time
    buffer_write(buf, input_string);

    output_string.resize(0);

    for (;;) {
        std::array<char, 3> data{};
        nbytes = buffer_read(buf, data);
        if (nbytes == 0)
            break;
        output_string.append(data.begin(), data.begin() + nbytes);
    }

    REQUIRE(output_string == input_string);
    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);
}

TEST_CASE("dynamic_buffer<>, large char buffer, commit/discard")
{
    std::string input_string =
        "this is a test string that will not fill more than one extent";
    sk::dynamic_buffer<char, 4096> buf;

    // Write into the buffer.
    std::span<char const> inbuf(input_string);
    while (!inbuf.empty()) {
        auto writable_ranges = buf.writable_ranges();

        for (auto &&range : writable_ranges) {
            if (inbuf.empty())
                break;

            auto can_write = std::min(range.size(), inbuf.size());
            std::ranges::copy(inbuf.subspan(0, can_write),
                              std::ranges::begin(range));
            buf.commit(can_write);
            inbuf = inbuf.subspan(can_write);
        }
    }

    // Read the buffer into the output string.
    std::string output_string(input_string.size(), 'A');
    std::span<char> outbuf(output_string);

    while (!outbuf.empty()) {
        auto readable_ranges = buf.readable_ranges();

        for (auto &&range : readable_ranges) {
            if (outbuf.empty())
                break;

            auto can_read = std::min(range.size(), outbuf.size());
            std::ranges::copy(range.subspan(0, can_read),
                              std::ranges::begin(outbuf));
            buf.discard(can_read);
            outbuf = outbuf.subspan(can_read);
        }
    }

    REQUIRE(output_string == input_string);

    auto nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);
}

TEST_CASE("dynamic_buffer<>, small buffer discard")
{
    std::string input_string =
        "this is a long test string that will fill several extents";
    sk::dynamic_buffer<char, sk::dynamic_buffer_size(3)> buf;

    buffer_write(buf, input_string);
    buf.discard(7);

    std::string output_string(input_string.size() - 7, 'A');
    auto nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == (input_string.size() - 7));
    REQUIRE(output_string == input_string.substr(7));

    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);
}

TEST_CASE("dynamic_buffer<>, large buffer discard")
{
    std::string input_string =
        "this is a test string that will not fill more than one extent";
    sk::dynamic_buffer<char, 4096> buf;

    buffer_write(buf, input_string);
    buf.discard(7);

    std::string output_string(input_string.size() - 7, 'A');
    auto nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == (input_string.size() - 7));
    REQUIRE(output_string == input_string.substr(7));

    nbytes = buffer_read(buf, output_string);
    REQUIRE(nbytes == 0);
}

TEST_CASE("dynamic_buffer<char>, small buffer write+read")
{
    static constexpr std::size_t buffer_size = 6;
    static constexpr std::size_t max_test_size = buffer_size * 10;
    static constexpr std::size_t npasses = 400;
    sk::dynamic_buffer<char, sk::dynamic_buffer_size(buffer_size)> buf;

    /*
     * Try repeatedly writing and reading to the buffer with
     * various write sizes.
     */
    for (std::size_t write_size = 1; write_size <= max_test_size;
         ++write_size) {
        for (std::size_t pass = 0; pass < npasses; ++pass) {
            std::string input_string(write_size, 'X');
            std::iota(input_string.begin(), input_string.end(), 'A');

            INFO("write_size=" + std::to_string(write_size) + ", pass=" +
                 std::to_string(pass) + "/" + std::to_string(npasses));

            auto n = buffer_write(buf, input_string);
            REQUIRE(n == input_string.size());

            std::string output_string(write_size, 'X');
            n = buffer_read(buf, output_string);
            REQUIRE(n == output_string.size());

            REQUIRE(input_string == output_string);
        }
    }

    /*
     * Now do the same but vary the write size each time.
     */
    for (std::size_t pass = 0; pass < npasses; ++pass) {
        for (std::size_t write_size = 1; write_size <= max_test_size;
             ++write_size) {

            std::string input_string(write_size, 'X');
            std::iota(input_string.begin(), input_string.end(), 'A');

            INFO("write_size=" + std::to_string(write_size) + ", pass=" +
                 std::to_string(pass) + "/" + std::to_string(npasses));

            auto n = buffer_write(buf, input_string);
            REQUIRE(n == input_string.size());

            std::string output_string(write_size, 'X');
            n = buffer_read(buf, output_string);
            REQUIRE(n == output_string.size());

            REQUIRE(input_string == output_string);
        }
    }
}

TEST_CASE("dynamic_buffer<wchar_t>, small buffer write+read")
{
    static constexpr std::size_t buffer_size = 6 * sizeof(wchar_t);
    static constexpr std::size_t max_test_size = buffer_size * 10;
    static constexpr std::size_t npasses = 400;
    sk::dynamic_buffer<wchar_t, sk::dynamic_buffer_size(buffer_size)> buf;

    /*
     * Try repeatedly writing and reading to the buffer with
     * various write sizes.
     */
    for (std::size_t write_size = 1; write_size <= max_test_size;
         ++write_size) {
        for (std::size_t pass = 0; pass < npasses; ++pass) {
            std::wstring input_string(write_size, L'X');
            std::iota(input_string.begin(), input_string.end(), L'A');

            INFO("write_size=" + std::to_string(write_size) + ", pass=" +
                 std::to_string(pass) + "/" + std::to_string(npasses));

            auto n = buffer_write(buf, input_string);
            REQUIRE(n == input_string.size());

            std::wstring output_string(write_size, L'X');
            n = buffer_read(buf, output_string);
            REQUIRE(n == output_string.size());

            REQUIRE(input_string == output_string);
        }
    }

    /*
     * Now do the same but vary the write size each time.
     */
    for (std::size_t pass = 0; pass < npasses; ++pass) {
        for (std::size_t write_size = 1; write_size <= max_test_size;
             ++write_size) {

            std::wstring input_string(write_size, L'X');
            std::iota(input_string.begin(), input_string.end(), L'A');

            INFO("write_size=" + std::to_string(write_size) + ", pass=" +
                 std::to_string(pass) + "/" + std::to_string(npasses));

            auto n = buffer_write(buf, input_string);
            REQUIRE(n == input_string.size());

            std::wstring output_string(write_size, L'X');
            n = buffer_read(buf, output_string);
            REQUIRE(n == output_string.size());

            REQUIRE(input_string == output_string);
        }
    }
}

TEST_CASE("dynamic_buffer<char>, large buffer write+read")
{
    static constexpr std::size_t buffer_size = 512;
    static constexpr std::size_t max_test_size = buffer_size * 2 + 1;
    static constexpr std::size_t npasses = 20;
    sk::dynamic_buffer<char, sk::dynamic_buffer_size(buffer_size)> buf;

    /*
     * Try repeatedly writing and reading to the buffer with
     * various write sizes.
     */
    for (std::size_t write_size = 1; write_size <= max_test_size;
         ++write_size) {
        for (std::size_t pass = 0; pass < npasses; ++pass) {
            std::string input_string(write_size, 'X');
            std::iota(input_string.begin(), input_string.end(), 'A');

            INFO("write_size=" + std::to_string(write_size) + ", pass=" +
                 std::to_string(pass) + "/" + std::to_string(npasses));

            auto n = buffer_write(buf, input_string);
            REQUIRE(n == input_string.size());

            std::string output_string(write_size, 'X');
            n = buffer_read(buf, output_string);
            REQUIRE(n == output_string.size());

            REQUIRE(input_string == output_string);
        }
    }

    /*
     * Now do the same but vary the write size each time.
     */
    for (std::size_t pass = 0; pass < npasses; ++pass) {
        for (std::size_t write_size = 1; write_size <= max_test_size;
             ++write_size) {

            std::string input_string(write_size, 'X');
            std::iota(input_string.begin(), input_string.end(), 'A');

            INFO("write_size=" + std::to_string(write_size) + ", pass=" +
                 std::to_string(pass) + "/" + std::to_string(npasses));

            auto n = buffer_write(buf, input_string);
            REQUIRE(n == input_string.size());

            std::string output_string(write_size, 'X');
            n = buffer_read(buf, output_string);
            REQUIRE(n == output_string.size());

            REQUIRE(input_string == output_string);
        }
    }
}

TEST_CASE("dynamic_buffer<wchar_t>, large buffer write+read")
{
    static constexpr std::size_t buffer_size = 512;
    static constexpr std::size_t max_test_size = buffer_size * 2 + 1;
    static constexpr std::size_t npasses = 20;
    sk::dynamic_buffer<wchar_t, sk::dynamic_buffer_size(buffer_size)> buf;

    /*
     * Try repeatedly writing and reading to the buffer with
     * various write sizes.
     */
    for (std::size_t write_size = 1; write_size <= max_test_size;
         ++write_size) {
        for (std::size_t pass = 0; pass < npasses; ++pass) {
            std::wstring input_string(write_size, L'X');
            std::iota(input_string.begin(), input_string.end(), L'A');

            INFO("write_size=" + std::to_string(write_size) + ", pass=" +
                 std::to_string(pass) + "/" + std::to_string(npasses));

            auto n = buffer_write(buf, input_string);
            REQUIRE(n == input_string.size());

            std::wstring output_string(write_size, L'X');
            n = buffer_read(buf, output_string);
            REQUIRE(n == output_string.size());

            REQUIRE(input_string == output_string);
        }
    }

    /*
     * Now do the same but vary the write size each time.
     */
    for (std::size_t pass = 0; pass < npasses; ++pass) {
        for (std::size_t write_size = 1; write_size <= max_test_size;
             ++write_size) {

            std::wstring input_string(write_size, L'X');
            std::iota(input_string.begin(), input_string.end(), L'A');

            INFO("write_size=" + std::to_string(write_size) + ", pass=" +
                 std::to_string(pass) + "/" + std::to_string(npasses));

            auto n = buffer_write(buf, input_string);
            REQUIRE(n == input_string.size());

            std::wstring output_string(write_size, 'X');
            n = buffer_read(buf, output_string);
            REQUIRE(n == output_string.size());

            REQUIRE(input_string == output_string);
        }
    }
}
