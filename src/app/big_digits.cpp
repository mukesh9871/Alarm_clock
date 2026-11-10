#include "app/big_digits.hpp"

namespace ac {

namespace {

// 3x5 glyphs; '#' = filled. Each cell is drawn two columns wide for better proportions.
using Glyph = std::array<std::string_view, kBigDigitRows>;

constexpr std::array<Glyph, 10> kDigits{{
    {"###", "# #", "# #", "# #", "###"},  // 0
    {"  #", "  #", "  #", "  #", "  #"},  // 1
    {"###", "  #", "###", "#  ", "###"},  // 2
    {"###", "  #", "###", "  #", "###"},  // 3
    {"# #", "# #", "###", "  #", "  #"},  // 4
    {"###", "#  ", "###", "  #", "###"},  // 5
    {"###", "#  ", "###", "# #", "###"},  // 6
    {"###", "  #", "  #", "  #", "  #"},  // 7
    {"###", "# #", "###", "# #", "###"},  // 8
    {"###", "# #", "###", "  #", "###"},  // 9
}};
constexpr Glyph kColon{" ", "#", " ", "#", " "};
constexpr Glyph kSpace{"   ", "   ", "   ", "   ", "   "};

const Glyph& glyph_for(char c) {
    if (c >= '0' && c <= '9') return kDigits[static_cast<std::size_t>(c - '0')];
    if (c == ':') return kColon;
    return kSpace;
}

}  // namespace

std::array<std::string, kBigDigitRows> big_digits(std::string_view text) {
    std::array<std::string, kBigDigitRows> rows;
    bool first = true;
    for (char c : text) {
        const Glyph& g = glyph_for(c);
        for (int r = 0; r < kBigDigitRows; ++r) {
            auto& row = rows[static_cast<std::size_t>(r)];
            if (!first) row += ' ';
            for (char cell : g[static_cast<std::size_t>(r)]) row += cell == '#' ? "██" : "  ";
        }
        first = false;
    }
    return rows;
}

int big_digits_width(std::string_view text) {
    int width = 0;
    bool first = true;
    for (char c : text) {
        if (!first) width += 1;
        width += 2 * static_cast<int>(glyph_for(c)[0].size());
        first = false;
    }
    return width;
}

}  // namespace ac
