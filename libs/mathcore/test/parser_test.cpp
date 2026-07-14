// libs/mathcore/test/parser_test.cpp
#include <mathcore/parser.h>
#include <mathcore/lexer.h>
#include <cassert>
#include <cstdio>

static mathx::ParseResult parseStr(const std::string& s) {
    std::vector<mathx::Token> toks;
    bool lexOk = mathx::tokenize(s, toks);
    assert(lexOk);
    return mathx::parse(toks);
}

int main() {
    // Basic arithmetic with correct precedence
    auto r1 = parseStr("1+2*3");
    assert(r1.ok && r1.root->kind == mathx::Node::Add);
    assert(r1.root->kids[1]->kind == mathx::Node::Mul); // "2*3" binds tighter than "1+"

    // Right-associative power
    auto r2 = parseStr("2^3^2");
    assert(r2.ok && r2.root->kind == mathx::Node::Pow);
    assert(r2.root->kids[1]->kind == mathx::Node::Pow); // 2^(3^2), not (2^3)^2

    // Function call
    auto r3 = parseStr("sqrt(4)");
    assert(r3.ok && r3.root->kind == mathx::Node::Call);
    assert(r3.root->name == "sqrt");
    assert(r3.root->kids[0]->kind == mathx::Node::Num);
    assert(r3.root->kids[0]->num == 4.0);

    // Constant recognition
    auto r4 = parseStr("pi");
    assert(r4.ok && r4.root->kind == mathx::Node::Const);
    assert(r4.root->name == "pi");

    // Unary minus
    auto r5 = parseStr("-5");
    assert(r5.ok && r5.root->kind == mathx::Node::Neg);

    // Parenthesized division
    auto r6 = parseStr("1/(2+3)");
    assert(r6.ok && r6.root->kind == mathx::Node::Div);
    assert(r6.root->kids[1]->kind == mathx::Node::Add);

    // Syntax error: leftover token
    auto r7 = parseStr("1 2");
    assert(!r7.ok);

    printf("parser_test: OK\n");
    return 0;
}
