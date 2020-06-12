#include <iostream>

#include "catch2/catch.hpp"
#include "Encoders.h"

TEST_CASE("Base64", "[toolkit][encoders]") {
    std::string out;
    std::string err;

    SECTION("Encoding empty input") {
        REQUIRE(darwin::toolkit::Base64::Encode("") == "");
    }

    SECTION("Encoding input that give output without padding") {
        REQUIRE(darwin::toolkit::Base64::Encode("Hello World!") == "SGVsbG8gV29ybGQh");
    }

    SECTION("Encoding input that requires one '=' for padding") {
        REQUIRE(darwin::toolkit::Base64::Encode("test with padding") == "dGVzdCB3aXRoIHBhZGRpbmc=");
    }

    SECTION("Encoding input that requires two '=' for padding") {
        REQUIRE(darwin::toolkit::Base64::Encode("test with more padding") == "dGVzdCB3aXRoIG1vcmUgcGFkZGluZw==");
    }

    SECTION("Encoding input with special characters") {
        REQUIRE(darwin::toolkit::Base64::Encode("azertyuiop QSDFGHJKLM 123456789 &(-_)=^$*,;:!") == "YXplcnR5dWlvcCBRU0RGR0hKS0xNIDEyMzQ1Njc4OSAmKC1fKT1eJCosOzoh");
    }
    SECTION("Encoding input to yield all encoded values") {
        REQUIRE(darwin::toolkit::Base64::Encode("\x69\xb7\x1d\x79\xf8\x21\x8a\x39\x25\x9a\x7a\x29\xaa\xbb\x2d\xba\xfc\x31\xcb\x30\x01\x08\x31\x05\x18\x72\x09\x28\xb3\x0d\x38\xf4\x11\x49\x35\x15\x59\x76\x19\xd3\x5d\xb7\xe3\x9e\xbb\xf3\xdf\xbf") == "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+/");
    }

    // +++++ DECODING +++++
    SECTION("Decoding empty input") {
        err = darwin::toolkit::Base64::Decode("", out);
        REQUIRE(err == "Input data size is zero");
        CHECK(out.empty());
    }

    SECTION("Decoding input") {
        err = darwin::toolkit::Base64::Decode("YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXpBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWjAxMjM0NTY3ODkq", out);
        REQUIRE(out == "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789*");
        CHECK(err.empty());
    }

    SECTION("Not decoding wrong lengths") {
        out = "data";
        err = darwin::toolkit::Base64::Decode("SGVsbG8gV29ybGQha", out);
        REQUIRE(err == "Input data size is not a multiple of 4");
        REQUIRE(out == "data");
        err = darwin::toolkit::Base64::Decode("SGVsbG8gV29ybGQhaa", out);
        REQUIRE(err == "Input data size is not a multiple of 4");
        REQUIRE(out == "data");
        err = darwin::toolkit::Base64::Decode("SGVsbG8gV29ybGQhaaa", out);
        REQUIRE(err == "Input data size is not a multiple of 4");
        REQUIRE(out == "data");
    }

    SECTION("Decoding all base characters to byte stream") {
        err = darwin::toolkit::Base64::Decode("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+/", out);
        REQUIRE(out == "\x69\xb7\x1d\x79\xf8\x21\x8a\x39\x25\x9a\x7a\x29\xaa\xbb\x2d\xba\xfc\x31\xcb\x30\x01\x08\x31\x05\x18\x72\x09\x28\xb3\x0d\x38\xf4\x11\x49\x35\x15\x59\x76\x19\xd3\x5d\xb7\xe3\x9e\xbb\xf3\xdf\xbf");
        CHECK(err == "");
    }
}



TEST_CASE("Hex", "[toolkit][encoders]") {
    std::string out;
    std::string err;

    SECTION("Encoding empty input") {
        REQUIRE(darwin::toolkit::Hex::Encode("") == "");
    }

    SECTION("Encoding input 1") {
        REQUIRE(darwin::toolkit::Hex::Encode("Hello World!") == "48656C6C6F20576F726C6421");
    }

    SECTION("Encoding input 2") {
        REQUIRE(darwin::toolkit::Hex::Encode("azertyuiop QSDFGHJKLM 123456789 &(-_)=^$*,;:!") == "617A6572747975696F70205153444647484A4B4C4D203132333435363738392026282D5F293D5E242A2C3B3A21");
    }

    SECTION("Encoding byte stream") {
        REQUIRE(darwin::toolkit::Hex::Encode(std::string({'\x00', '\x01', '\x03', '\x05', '\x06', '\x10', '\xFF', '\x56', '\x45', '\x30', '\xEA', '\xDC', '\x95', '\x45', '\x23'})) == "000103050610FF564530EADC954523");
    }


    //+++++ DECODING +++++
    SECTION("Decoding empty input") {
        err = darwin::toolkit::Hex::Decode("", out);
        REQUIRE(err == "Input has zero length.");
        REQUIRE(out.empty());
    }

    SECTION("Decoding input with even length") {
        err = darwin::toolkit::Hex::Decode("01234567890", out);
        REQUIRE(err == "Input has wrong length.");
        REQUIRE(out.empty());
    }

    SECTION("Decoding valid input") {
        err = darwin::toolkit::Hex::Decode("000123456789", out);
        REQUIRE(out == std::string({'\x00', '\x01', '\x23', '\x45', '\x67', '\x89'}));
        CHECK(err.empty());
    }

    SECTION("Decoding valid input (uppercase)") {
        err = darwin::toolkit::Hex::Decode("48656C6C6F20576F726C6421", out);
        REQUIRE(out == "Hello World!");
        CHECK(err.empty());
    }

    SECTION("Decoding valid input (lowercase)") {
        err = darwin::toolkit::Hex::Decode("48656c6c6f20576f726c6421", out);
        REQUIRE(out == "Hello World!");
        CHECK(err.empty());
    }

    SECTION("Decoding invalid input") {
        err = darwin::toolkit::Hex::Decode("48656c6c6f20576f726c642z", out);
        REQUIRE(err == "Invalid hexadecimal value.");
    }
}
