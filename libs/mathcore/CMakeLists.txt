# mathcore — standalone, reusable math-typesetting/evaluation/CAS library,
# extracted from windows_ui_demo's src/math/. Built as the mathcore static
# library. Mirrors libs/firstparty/vulkan_font_engine/core/CMakeLists.txt's
# add_library/target_include_directories/target_compile_features pattern.

add_library(mathcore STATIC
    src/lexer.cpp
    src/parser.cpp
    src/mathbox.cpp
    src/math_layout.cpp
    src/numeric_evaluator.cpp
    src/cached_expression.cpp
    src/editor.cpp
    src/editor_layout.cpp
    src/calc.cpp
    src/graphing/plot.cpp
    src/cas/symbolic_engine.cpp
)

target_include_directories(mathcore PUBLIC include)

target_compile_features(mathcore PUBLIC cxx_std_17)

# ---------- optional Eigenmath CAS backend (vendored, off by default) ----------
# Eigenmath (third_party/eigenmath/eigenmath.c, BSD-2-Clause, George Weigt --
# see third_party/eigenmath/LICENSE-eigenmath.txt) is a real symbolic-math
# engine: exact arithmetic, real differentiation/integration. It is a second,
# OPTIONAL mathcore::cas::CasEngine backend behind the same seam as SymbolicEngine
# ("dyno"), matching the original calculator app's own CAS_USE_EIGENMATH gate.
# OFF by default so the ordinary build (build.bat debug/release) never pays
# for compiling a 19k-line vendored C file; `build.bat debug eigenmath` (or
# -DMATHCORE_ENABLE_EIGENMATH=ON directly) turns it on.
option(MATHCORE_ENABLE_EIGENMATH "Vendor and build the Eigenmath CAS backend" OFF)
if(MATHCORE_ENABLE_EIGENMATH)
    target_sources(mathcore PRIVATE
        src/cas/eigenmath_engine.cpp
        third_party/eigenmath/eigenmath.c
    )
    target_compile_definitions(mathcore PUBLIC MATHCORE_ENABLE_EIGENMATH)
    # Vendored third-party C, compiled as-is -- silence its warnings, and keep
    # its upstream standalone-CLI driver (main/run_infile/run_stdin) out of the
    # build so it never collides with a host executable's own main() (see the
    # "VENDORED EDIT (nava)" comment beside main() in eigenmath.c).
    set_source_files_properties(third_party/eigenmath/eigenmath.c
        PROPERTIES COMPILE_DEFINITIONS "EIGENMATH_NO_MAIN")
    if(MSVC)
        set_source_files_properties(third_party/eigenmath/eigenmath.c
            PROPERTIES COMPILE_OPTIONS "/w")
    else()
        set_source_files_properties(third_party/eigenmath/eigenmath.c
            PROPERTIES COMPILE_OPTIONS "-w")
    endif()
endif()

# ---------- math parser/lexer/AST unit test ----------
add_executable(parser_test test/parser_test.cpp)
target_link_libraries(parser_test PRIVATE mathcore)

# ---------- mathbox compile/link smoke test against the fake IMathCanvas/
# IMathFontMetrics backend (test/fake_math_backend.h) — not a functional test,
# just proves mathbox.cpp links standalone with zero Vulkan/MsdfFont
# dependency. fake_math_backend.h is kept around (not deleted after use) for
# Task 4 (math_layout) and Task 11 (editor_layout/caret) to reuse.
add_executable(mathbox_link_test test/mathbox_link_test.cpp)
target_link_libraries(mathbox_link_test PRIVATE mathcore)

# ---------- math_layout (fitAst/drawAstAt) compile/link smoke test against
# the same fake IMathCanvas/IMathFontMetrics backend (test/fake_math_backend.h,
# reused as-is from Task 3) — not a functional test, just proves
# math_layout.cpp links standalone with zero Vulkan/MsdfFont dependency.
add_executable(math_layout_link_test test/math_layout_link_test.cpp)
target_link_libraries(math_layout_link_test PRIVATE mathcore)

# ---------- fitAst/CachedExpression extents test: every mark a typeset
# expression draws must fall inside the [yAxis - fit.above, yAxis + fit.below]
# box the fit reports — hosts (windows_ui_demo's Math page) position the
# "= result" line from these extents, so under-measurement means overlap.
# Functional (recording canvas), unlike the *_link_test smoke tests above.
add_executable(fit_extents_test test/fit_extents_test.cpp)
target_link_libraries(fit_extents_test PRIVATE mathcore)

# ---------- numeric evaluator (IEvaluator/NumericEvaluator) unit test ----------
add_executable(evaluator_test test/evaluator_test.cpp)
target_link_libraries(evaluator_test PRIVATE mathcore)

# ---------- CachedExpression (opt-in tokenize/parse/layout caching) unit
# test, against the same fake IMathCanvas/IMathFontMetrics backend
# (test/fake_math_backend.h) used by mathbox_link_test/math_layout_link_test.
add_executable(cached_expression_test test/cached_expression_test.cpp)
target_link_libraries(cached_expression_test PRIVATE mathcore)

# ---------- calcedit::Editor (structured 2D input tree: cursor, insert/
# backspace, navigation through fractions/roots/exponents) unit test. Pure
# data-structure/logic — no rendering integration (that's Task 11).
add_executable(editor_test test/editor_test.cpp)
target_link_libraries(editor_test PRIVATE mathcore)

# ---------- editor_layout (buildEdRow/drawEditor: live calcedit::Editor ->
# mathbox IR bridge, including the real Caret) compile/link smoke test against
# the same fake IMathCanvas/IMathFontMetrics backend (test/fake_math_backend.h,
# reused as-is from Task 3/4) -- structural assertions on the built node tree
# (kinds/counts/caret position), not a visual test.
add_executable(editor_layout_test test/editor_layout_test.cpp)
target_link_libraries(editor_layout_test PRIVATE mathcore)

# ---------- Calc facade (token dispatch / evaluate / preview / ans-injection /
# auto-paren-balancing, wired to an injected mathx::IEvaluator) unit test.
add_executable(calc_test test/calc_test.cpp)
target_link_libraries(calc_test PRIVATE mathcore)

# ---------- graphing::sampleEquation/evalFunctionAt (Function/Parametric/Polar
# sampling, ported faithfully: tokenize+parse each expression once per call,
# then eval the cached AST per sample) unit test.
add_executable(plot_test test/plot_test.cpp)
target_link_libraries(plot_test PRIVATE mathcore)

# ---------- CAS interface (mathcore::cas::CasEngine) + homegrown SymbolicEngine
# ("dyno" backend: constant-folding simplify(), sum/product/quotient/power/
# chain-rule Differ over sin/cos/tan/ln/log/sqrt, Op::Numeric delegating to
# NumericEvaluator, Op::Integral stubbed with a documented error) unit test.
add_executable(cas_test test/cas_test.cpp)
target_link_libraries(cas_test PRIVATE mathcore)
