#pragma once

#include "app/session.hpp"

#include <string>

namespace ac {

// Full-screen interactive terminal UI.
int run_tui(AlarmSession& session);

// Renders one frame of the UI (for an already-started session) into text, without
// touching the terminal. Used by tests and for debugging layout.
[[nodiscard]] std::string render_tui_snapshot(AlarmSession& session, int width, int height);

}  // namespace ac
