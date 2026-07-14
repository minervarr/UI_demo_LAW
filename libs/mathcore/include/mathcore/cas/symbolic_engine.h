// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\core\cas\symbolic_engine.hh (same author, AGPLv3).
// symbolic_engine.h — the "dyno" CAS backend: a small but real symbolic engine.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
//
// This is the load cell on the dyno, not the McLaren engine. It is a genuine
// symbolic differentiator built on the existing mathx AST: it parses with the
// production lexer/parser, differentiates w.r.t. a variable (sum / product /
// quotient / power / chain rules over sin cos tan ln log sqrt), simplifies
// (constant-folds, drops 0/1 identities), and serializes back to canonical
// infix. Zero external dependencies — it compiles into mathcore like everything
// else and lets us prove the CasEngine seam and the S<->D / d/dx UI end-to-end
// *today*.
//
// Integration is intentionally NOT implemented here (it returns a clear "needs
// the Giac engine" reply). That gap is the whole point: once the plumbing is
// proven on this backend, Eigenmath and then Giac drop in behind the identical
// CasEngine interface and bring the real integrator.
#pragma once
#include <mathcore/cas/cas.h>

namespace mathcore::cas {

class SymbolicEngine : public CasEngine {
public:
    Reply evaluate(const Request& req, const std::atomic<bool>& cancel) override;
    const char* name() const override { return "symbolic(dyno)"; }
};

}  // namespace mathcore::cas
