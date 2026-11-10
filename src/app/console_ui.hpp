#pragma once

#include "app/session.hpp"

namespace ac {

// Line-based clock: works in any terminal, including when input is piped.
int run_console(AlarmSession& session);

}  // namespace ac
