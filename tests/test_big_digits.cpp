#include "app/big_digits.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace {
// Terminal columns of a UTF-8 string where every code point is one column wide.
int columns(const std::string& s) {
    int n = 0;
    for (char ch : s) {
        if ((static_cast<unsigned char>(ch) & 0xC0) != 0x80) ++n;
    }
    return n;
}
}  // namespace

TEST_CASE("big_digits rows have consistent width") {
    for (const char* text : {"0", "12:34:56", "07:05", "  ", "9"}) {
        INFO(text);
        const auto rows = big_digits(text);
        for (const auto& r : rows) CHECK(columns(r) == big_digits_width(text));
    }
}

TEST_CASE("big_digits draws recognisable glyphs") {
    const auto one = big_digits("1");
    // '1' is a single bar on the right.
    for (const auto& r : one) CHECK(r == "    ██");
    const auto colon = big_digits(":");
    CHECK(colon[0] == "  ");
    CHECK(colon[1] == "██");
    CHECK(big_digits_width("12:34") == 6 + 1 + 6 + 1 + 2 + 1 + 6 + 1 + 6);
}

TEST_CASE("big_digits treats unknown characters as blanks") {
    CHECK(big_digits("x") == big_digits(" "));
    CHECK(big_digits("").at(0).empty());
}
