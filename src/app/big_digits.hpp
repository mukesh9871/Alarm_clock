#pragma once

#include <array>
#include <string>
#include <string_view>

namespace ac {

inline constexpr int kBigDigitRows = 5;

// Renders digits, ':' and ' ' as 5-row block-character art (UTF-8).
// Other characters render as blanks. All rows have the same display width.
[[nodiscard]] std::array<std::string, kBigDigitRows> big_digits(std::string_view text);

// Display width (terminal columns) of big_digits(text).
[[nodiscard]] int big_digits_width(std::string_view text);

}  // namespace ac
