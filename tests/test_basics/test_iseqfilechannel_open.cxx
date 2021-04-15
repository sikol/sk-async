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

#include <fstream>
#include <iostream>

#include <catch.hpp>

#include <sk/channel/filechannel.hxx>

TEST_CASE("iseqfilechannel::open() existing file") {
    std::ignore = std::remove("test.txt");

    {
        std::ofstream testfile("test.txt", std::ios::binary | std::ios::trunc);
        testfile << "This is a test\n";
        testfile.close();
    }

    {
        sk::iseqfilechannel chnl;
        auto ret = chnl.open("test.txt");
        REQUIRE(ret);
    }

    std::ignore = std::remove("test.txt");
}

TEST_CASE("iseqfilechannel::open() with write flags is an error") {
    sk::iseqfilechannel chnl;

    auto ret = chnl.open("test.txt", sk::fileflags::write);
    REQUIRE(!ret);
    REQUIRE(ret.error() == sk::error::filechannel_invalid_flags);

    ret = chnl.open("test.txt", sk::fileflags::trunc);
    REQUIRE(!ret);
    REQUIRE(ret.error() == sk::error::filechannel_invalid_flags);

    ret = chnl.open("test.txt", sk::fileflags::append);
    REQUIRE(!ret);
    REQUIRE(ret.error() == sk::error::filechannel_invalid_flags);

    ret = chnl.open("test.txt", sk::fileflags::create_new);
    REQUIRE(!ret);
    REQUIRE(ret.error() == sk::error::filechannel_invalid_flags);
}

TEST_CASE("iseqfilechannel::open() non-existing file") {
    std::ignore = std::remove("__sk_cio_nonexisting_file.txt");

    {
        sk::iseqfilechannel chnl;
        auto ret = chnl.open("__sk_cio_nonexisting_file.txt");
        REQUIRE(!ret);
        REQUIRE(ret.error() == std::errc::no_such_file_or_directory);
    }
}
