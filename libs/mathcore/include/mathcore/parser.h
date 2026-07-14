// Ported from C:\Users\incxiuefb\Documents\Files\clone\calculator\core\math\parser.hh (same author, AGPLv3).
// parser.hh — recursive-descent parser producing the expression AST.
//
// Copyright (C) 2026 nava. Licensed under the GNU AGPLv3 or later; see LICENSE.
#pragma once
#include <mathcore/ast.h>
#include <mathcore/lexer.h>

namespace mathx {

struct ParseResult {
    NodePtr root;
    bool    ok = false;
};

// Parse a token stream (terminated by Tok::End). Missing ')' at end is tolerated
// (auto-closed). On any structural error, ok == false.
ParseResult parse(const std::vector<Token>& toks);

}  // namespace mathx
