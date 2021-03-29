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

#include <chrono>
#include <fstream>
#include <iostream>

#include <catch.hpp>

#include <sk/cio.hxx>

using namespace sk;
using namespace std::literals::chrono_literals;

constexpr int nthreads = 25;
constexpr int nops = 500;
constexpr auto run_for = 20s;

task<int> stress_task(idafilechannel &chnl)
{
    std::random_device r;
    std::default_random_engine eng(r());
    std::uniform_int_distribution<io_offset_t> rnd(0, 9);

    auto start = std::chrono::steady_clock::now();

    for (;;) {
        for (int i = 0; i < nops; ++i) {
            std::byte b;
            auto offs = rnd(eng);
            auto ret =
                co_await async_read_some_at(chnl, offs, std::span(&b, 1));

            if (!ret)
                co_return 1;

            auto expected_byte = std::byte('0' + offs);
            if (b != expected_byte)
                co_return 1;
        }

        auto now = std::chrono::steady_clock::now();
        if ((now - start) >= run_for)
            break;
    }

    co_return 0;
}

TEST_CASE("idafilechannel stress test")
{
    // Create the test file
    {
        std::ignore = std::remove("__sk_cio_test.txt");
        std::ofstream f("__sk_cio_test.txt");
        f << "0123456789";
        f.close();
    }

    idafilechannel chnl;
    auto ret = chnl.open("__sk_cio_test.txt");
    REQUIRE(ret);

    std::vector<std::future<int>> futures;

    std::cerr << "starting stress tasks\n";

    for (int i = 0; i < nthreads; ++i)
        futures.emplace_back(std::async(std::launch::async, [&]() -> int {
            return wait(stress_task(chnl));
        }));

    std::cerr << "joining stress tasks\n";

    int errors = 0;

    for (auto &&future : futures) {
        auto res = future.get();
        errors += res;
    }

    REQUIRE(errors == 0);
}
