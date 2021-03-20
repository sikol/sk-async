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

#include <catch.hpp>

#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <sk/cio/channel/read.hxx>
#include <sk/cio/channel/dafilechannel.hxx>
#include <sk/cio/task.hxx>

using namespace sk::cio;

TEST_CASE("idafilechannel::read()") {
    {
        std::ofstream testfile("test.txt", std::ios::binary | std::ios::trunc);
        testfile << "This is a test\n";
        testfile << "This is a test\n";
        testfile << "This is a test\n";
        testfile.flush();
        testfile.close();
    }

    idafilechannel<char> chnl;
    auto ret = chnl.open("test.txt");
    if (ret) {
        INFO(ret.message());
        REQUIRE(false);
    }

    for (int i = 3; i >= 0; --i) {
        std::string buf(15, 'X');
        auto nbytes = read_some_at(chnl, 15 - i, i, buf);
        if (!nbytes) {
            INFO(nbytes.error().message());
            REQUIRE(false);
        }

        REQUIRE(*nbytes == 15 - i);
        auto expected = std::string("This is a test\n").substr(i);
        REQUIRE(buf.substr(0, 15 - i) == expected);
    }

    std::string buf(15, 'X');
    auto nbytes = read_some_at(chnl, unlimited, 50, buf);
    REQUIRE(!nbytes);
    REQUIRE(nbytes.error() == error::end_of_file);
}

TEST_CASE("idafilechannel::async_read()") {
    {
        std::ofstream testfile("test.txt", std::ios::binary | std::ios::trunc);
        testfile << "This is a test\n";
        testfile << "This is a test\n";
        testfile << "This is a test\n";
        testfile.flush();
        testfile.close();
    }

    idafilechannel<char> chnl;
    auto ret = chnl.async_open("test.txt").wait();
    if (ret) {
        INFO(ret.message());
        REQUIRE(false);
    }

    for (int i = 3; i >= 0; --i) {
        std::string buf(15, 'X');
        auto nbytes = async_read_some_at(chnl, 15 - i, i, buf).wait();
        if (!nbytes) {
            INFO(nbytes.error().message());
            REQUIRE(false);
        }

        REQUIRE(*nbytes == 15 - i);
        auto expected = std::string("This is a test\n").substr(i);
        REQUIRE(buf.substr(0, 15 - i) == expected);
    }

    std::string buf(15, 'X');
    auto nbytes = async_read_some_at(chnl, unlimited, 50, buf).wait();
    REQUIRE(!nbytes);
    REQUIRE(nbytes.error() == error::end_of_file);
}
