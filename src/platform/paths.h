#pragma once
#include <string>

// Returns the directory containing the running executable (no trailing
// slash), as a UTF-8 std::string. Used to resolve asset paths (shaders,
// fonts) relative to the exe rather than the process's current working
// directory, which varies depending on how the app is launched.
std::string exeDirectory();
