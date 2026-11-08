#include "core/challenge.hpp"

#include <random>

namespace ac {

std::optional<ChallengeKind> parse_challenge(std::string_view text) {
    if (text.empty() || text == "none" || text == "off") return ChallengeKind::None;
    if (text == "math") return ChallengeKind::Math;
    return std::nullopt;
}

std::string challenge_to_string(ChallengeKind kind) { return kind == ChallengeKind::Math ? "math" : "none"; }

MathProblem make_math_problem(std::uint32_t seed) {
    // std::mt19937 is specified exactly by the standard, so results are identical everywhere;
    // the distributions are not, so map the raw output by hand.
    std::mt19937 rng(seed);
    auto pick = [&rng](int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<std::uint32_t>(hi - lo + 1)); };
    const int a = pick(3, 12);
    const int b = pick(3, 12);
    const int c = pick(10, 99);
    return {std::to_string(a) + " x " + std::to_string(b) + " + " + std::to_string(c) + " = ?", a * b + c};
}

}  // namespace ac
