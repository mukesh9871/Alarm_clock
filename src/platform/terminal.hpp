#pragma once

namespace ac {

// True when the stream is attached to an interactive terminal (not a pipe, file or /dev/null).
[[nodiscard]] bool stdin_is_terminal();
[[nodiscard]] bool stdout_is_terminal();

}  // namespace ac
