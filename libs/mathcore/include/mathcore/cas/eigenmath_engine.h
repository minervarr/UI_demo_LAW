// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\core\cas\eigenmath_engine.hh (same author, AGPLv3).
// eigenmath_engine.h — the next backend behind the SAME mathcore::cas::CasEngine seam.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
// (This bridge file is AGPLv3. The vendored engine it wraps,
//  third_party/eigenmath/eigenmath.c, is a SEPARATE work: BSD-2-Clause,
//  Copyright (c) 2024 George Weigt -- see third_party/eigenmath/LICENSE-eigenmath.txt.
//  The two licenses are never blurred into one statement.)
//
// This is the documented drop-in point for the real CAS. The dyno
// (mathcore::cas::SymbolicEngine) proves the seam + worker isolation + UI; swapping in
// Eigenmath is mechanical because nothing above CasEngine knows which backend
// is bolted in:
//
//   CasWorker worker(mathcore::cas::makeEigenmathEngine());   // instead of the default
//
// OFF BY DEFAULT (matches the original calculator app's own CAS_USE_EIGENMATH
// gate): only compiled/linked when libs/mathcore's CMakeLists.txt has
// MATHCORE_ENABLE_EIGENMATH=ON (see build.bat's `eigenmath` arg), which also
// adds third_party/eigenmath/eigenmath.c to the mathcore target.
#pragma once
#include <memory>

#include <mathcore/cas/cas.h>

namespace mathcore::cas {

// Defined in eigenmath_engine.cpp, compiled only when MATHCORE_ENABLE_EIGENMATH
// is set and the vendored source is present. Until then the default
// SymbolicEngine ("dyno") runs. The only public symbol of this header.
std::unique_ptr<CasEngine> makeEigenmathEngine();

}  // namespace mathcore::cas
