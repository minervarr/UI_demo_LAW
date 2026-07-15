#pragma once
#include "canvas.hh"

// Grayscale is app policy, not an engine mode (see vk_canvas CLAUDE.md's
// "Color policy"): the library draws full Color{r,g,b,a}; this demo passes
// equal channels everywhere via this one helper, which keeps the showcase
// grayscale end to end without narrowing the library's API.
inline Color gray(float v, float a = 1.0f) { return Color{v, v, v, a}; }
