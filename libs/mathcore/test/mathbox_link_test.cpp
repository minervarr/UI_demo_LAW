// Compile/link smoke test for mathbox.cpp against the fake IMathCanvas/
// IMathFontMetrics backend (libs/mathcore/test/fake_math_backend.h). Not a
// functional/visual test — mathbox has no automated rendering test per this
// repo's stated convention (see the plan's Verification summary); this just
// proves mathbox.cpp compiles+links standalone against the mathcore seam with
// no MsdfFont/Vulkan dependency, and exercises every Node::Kind + the
// showCaret path once each so a signature mismatch would fail to compile.
#include <cassert>
#include <cstdio>

#include <mathcore/mathbox.h>

#include "fake_math_backend.h"

using namespace mathcore;
using namespace mathcore::mathbox;

namespace {

NodePtr buildSample() {
    // (x + 1/2)^sqrt(y)  — exercises HBox/Text/Glyph/Frac/Radical/Script/Delim.
    auto hbox = mk(Node::HBox);

    auto openParen = mk(Node::Delim);
    {
        auto inner = mk(Node::HBox);
        inner->kids.push_back(mkGlyph(mathItalicCp('x'), MathClass::Ord));
        auto frac = mk(Node::Frac);
        frac->kids.push_back(mkText("1"));
        frac->kids.push_back(mkText("2"));
        inner->kids.back()->cls = MathClass::Ord;
        inner->kids.push_back(mkText("+", MathClass::Bin));
        inner->kids.push_back(std::move(frac));
        openParen->kids.push_back(std::move(inner));
    }

    auto script = mk(Node::Script);
    script->kids.push_back(std::move(openParen));
    {
        auto radical = mk(Node::Radical);
        radical->kids.push_back(mkGlyph(mathItalicCp('y')));
        script->kids.push_back(std::move(radical));
    }
    script->scriptCaret = true;

    hbox->kids.push_back(std::move(script));

    auto caret = mk(Node::Caret);
    hbox->kids.push_back(std::move(caret));

    auto placeholder = mk(Node::Placeholder);
    hbox->kids.push_back(std::move(placeholder));

    return hbox;
}

}  // namespace

int main() {
    // With a fake math font present.
    {
        test::FakeMathCanvas canvas(/*withFont=*/true);
        NodePtr root = buildSample();
        Box b = layout(canvas, *root, 48.0f, MathStyle::Display);
        assert(b.w > 0.0f);
        assert(b.above > 0.0f);
        assert(b.below >= 0.0f);
        draw(canvas, *root, 0.0f, 100.0f, Ink::gray(0.9f), /*showCaret=*/false);
        draw(canvas, *root, 0.0f, 100.0f, Ink::gray(0.9f), /*showCaret=*/true);
    }
    // Legacy no-math-font fallback path.
    {
        test::FakeMathCanvas canvas(/*withFont=*/false);
        NodePtr root = buildSample();
        Box b = layout(canvas, *root, 48.0f, MathStyle::Display);
        assert(b.w > 0.0f);
        draw(canvas, *root, 0.0f, 100.0f, Ink::gray(0.9f), /*showCaret=*/true);
    }

    std::puts("mathbox_link_test: OK");
    return 0;
}
