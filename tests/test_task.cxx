/*
 * Copyright(c) 2019, 2020, 2021 SiKol Ltd.
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license(the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third - parties to whom the Software is furnished to
 * do so, all subject to the following :
 *
 * The copyright notices in the Softwareand this entire statement, including
 * the above license grant, this restrictionand the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine - executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON - INFRINGEMENT.IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <cstring>
#include <stdexcept>
#include <string>
#include <iostream>

#include <sk/cio/task.hxx>

using sk::cio::task;

auto get_int() -> task<int> {
    std::cerr << "get_int() : running\n";
    co_return 42;
}

TEST_CASE("task<int> works") {
    std::cerr << "\n\ntask<int> starting\n";
    int i = get_int().wait();
    REQUIRE(i == 42);
}

auto set_int(int &i) -> task<void> {
    std::cerr << "set_int() : running\n";
    i = 42;
    co_return;
}

TEST_CASE("task<void> works") {
    int i = 0;
    std::cerr << "\n\ntask<void> starting\n";
    set_int(i).wait();
    REQUIRE(i == 42);
}