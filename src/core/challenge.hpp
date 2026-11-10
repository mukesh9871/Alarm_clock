#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ac {

// What you must do to turn a ringing alarm off (snoozing is always allowed).
enum class ChallengeKind { None, Math };

[[nodiscard]] std::optional<ChallengeKind> parse_challenge(std::string_view text);  // "none" / "math"
[[nodiscard]] std::string challenge_to_string(ChallengeKind kind);

// A problem hard enough to need a working brain, easy enough to solve half-asleep:
// a * b + c with a, b in [3, 12] and c in [10, 99].
struct MathProblem {
    std::string question;  // "7 x 8 + 23 = ?"
    int answer = 0;
};

// Deterministic for a given seed (tests rely on this).
[[nodiscard]] MathProblem make_math_problem(std::uint32_t seed);

}  // namespace ac
