#pragma once

// Fail-fast (message + exit) — see CLAUDE.md's global constraint: no retry/
// fallback logic anywhere in this codebase. One declaration shared by every
// caller (gfx/vk_core.cpp, gfx/shape_pipeline.cpp, app/main.cpp); each
// platform backend (platform/windows/fatal.cpp today, future
// platform/android|linux/fatal.cpp) supplies its own definition.
void fatal(const char* msg);
