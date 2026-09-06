#pragma once
#include <string>

#include "canvas.hh"
#include "frame_input.hh"
#include "layout.hh"
#include "text_buffer.hh"
#include "undo_redo.hh"

#include "gray.h"
#include "widgets_gray.h"

// Text entry, undo/redo, and the input-method seam.
//
// Two ways text arrives, and they are not variants of each other:
//
//   typedChars   — finished characters, one at a time. Win32 WM_CHAR and
//                  Wayland's xkb path both produce these.
//   editedText   — an INPUT METHOD replaced the field's whole contents.
//                  Composing Korean jamo into a syllable, or picking a Chinese
//                  candidate over its pinyin reading, REWRITES the pending run:
//                  what was on screen a moment ago is not a prefix of what
//                  follows, so a stream of "a character was typed" has nowhere
//                  to put it. The platform states the authoritative contents
//                  and the app adopts them wholesale, which also leaves
//                  composition state owned by the IME — the only place it can
//                  correctly live.
//
// That second path is why this page exists on Android at all: it is how you
// type Japanese into the demo with the system keyboard.
//
// The page needs to ask the host to raise and dismiss the soft keyboard, and a
// page has no Host. It takes callbacks instead, so nothing here names a
// platform and the desktop can simply pass empty ones.
class TextEditPage {
 public:
    using KeyboardFn     = void (*)(void* ctx, const std::string& text, size_t cursorByte);
    using HideKeyboardFn = void (*)(void* ctx);
    // Asks the app to make sure every codepoint in the string has a glyph in
    // the atlas. Anything typed after startup is by definition not in a
    // startup bake, and a missing glyph draws as nothing at all.
    using EnsureGlyphsFn = void (*)(void* ctx, const std::string& text);

    // The text the field starts with. Named so the app can put its codepoints
    // in the startup bake — otherwise the Japanese in it renders blank, which
    // is exactly the bug this seed is here to disprove.
    static constexpr const char* kSeedText = "Type here. \u65e5\u672c\u8a9e\u3082\u3002";

    TextEditPage();

    void setKeyboardHooks(void* ctx, KeyboardFn show, HideKeyboardFn hide) {
        ctx_ = ctx; show_ = show; hide_ = hide;
    }
    void setGlyphHook(void* ctx, EnsureGlyphsFn ensure) {
        glyphCtx_ = ctx; ensure_ = ensure;
    }

    void updateLayout(Rect content, float scale);
    void update(float dt, const FrameInput& in);
    void draw(Canvas& c);

    bool hoversAnyWidget(const FrameInput& in) const {
        return undoBtn_.hovered(in) || redoBtn_.hovered(in) || clearBtn_.hovered(in) ||
               field_.contains(in.pointerX, in.pointerY);
    }

 private:
    void syncKeyboard();

    TextBuffer buf_;
    UndoRedo   undo_;

    Button undoBtn_{0, 0, 0, 0};
    Button redoBtn_{0, 0, 0, 0};
    Button clearBtn_{0, 0, 0, 0};

    Rect  content_{0, 0, 0, 0};
    Rect  field_{0, 0, 0, 0};
    float scale_   = 1.0f;
    float caretT_  = 0.0f;
    bool  focused_ = false;

    void*          ctx_  = nullptr;
    KeyboardFn     show_ = nullptr;
    HideKeyboardFn hide_ = nullptr;
    void*          glyphCtx_ = nullptr;
    EnsureGlyphsFn ensure_   = nullptr;
};
