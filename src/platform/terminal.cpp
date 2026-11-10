#include "platform/terminal.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ac {

#ifdef _WIN32
namespace {
// _isatty() is also true for character devices such as NUL, so ask for a console instead.
bool is_console(DWORD std_handle) {
    HANDLE h = GetStdHandle(std_handle);
    DWORD mode = 0;
    return h != nullptr && h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode) != 0;
}
}  // namespace

bool stdin_is_terminal() { return is_console(STD_INPUT_HANDLE); }
bool stdout_is_terminal() { return is_console(STD_OUTPUT_HANDLE); }
#else
bool stdin_is_terminal() { return isatty(0) != 0; }
bool stdout_is_terminal() { return isatty(1) != 0; }
#endif

}  // namespace ac
