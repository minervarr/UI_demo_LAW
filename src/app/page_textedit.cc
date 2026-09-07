#include "page_textedit.h"

#include <cmath>
#include <cstdio>

#include "keys.h"
#include "keys.hh"
#include "utf8.hh"

namespace {

// TextBuffer is a BYTE gap buffer: erase_before() removes one byte and
// move_cursor(±1) moves by one. That is the right primitive for it to offer
// and the wrong one to drive a caret with — backspacing "語" would delete one
// of its three bytes and leave the buffer holding invalid UTF-8, and an arrow
// key would park the caret mid-character so the text measured for its x
// position is cut through a codepoint.
//
// This page's seed text is Japanese, so it is the first thing that breaks.
// The two helpers below step whole codepoints by walking continuation bytes
// (0b10xxxxxx), which is all UTF-8 requires to be scanned in either direction.
bool isContinuation(unsigned char c) { return (c & 0xC0) == 0x80; }

size_t prevBoundary(const std::string& s, size_t pos) {
    if (pos == 0) return 0;
    size_t i = pos - 1;
    while (i > 0 && isContinuation((unsigned char)s[i])) --i;
    return i;
}

size_t nextBoundary(const std::string& s, size_t pos) {
    if (pos >= s.size()) return s.size();
    size_t i = pos + 1;
    while (i < s.size() && isContinuation((unsigned char)s[i])) ++i;
    return i;
}

}  // namespace

TextEditPage::TextEditPage() {
    buf_.insert(std::string(kSeedText));
}

const std::string& TextEditPage::text() const {
    if (cachedGen_ != buf_.generation()) {
        cachedText_ = buf_.to_string();
        cachedGen_  = buf_.generation();
    }
    return cachedText_;
}

void TextEditPage::updateLayout(Rect content, float scale) {
    content_ = content;
    scale_   = scale;

    const float pad = 22.0f * scale;
    // Below the title, whose box is 24 px BEFORE scaling — a fixed gap here
    // overlapped it as soon as the scale went above about 1.4.
    float y = content.y + pad + 24.0f * scale + 14.0f * scale;

    field_ = Rect{content.x + pad, y, content.w - 2.0f * pad, 130.0f * scale};
    y += field_.h + 22.0f * scale;

    const float bw = 120.0f * scale, bh = 38.0f * scale, gap = 12.0f * scale;
    RowCursor row(content.x + pad, y, gap);
    Rect r = row.next(bw, bh);
    undoBtn_.x = r.x; undoBtn_.y = r.y; undoBtn_.w = r.w; undoBtn_.h = r.h;
    r = row.next(bw, bh);
    redoBtn_.x = r.x; redoBtn_.y = r.y; redoBtn_.w = r.w; redoBtn_.h = r.h;
    r = row.next(bw, bh);
    clearBtn_.x = r.x; clearBtn_.y = r.y; clearBtn_.w = r.w; clearBtn_.h = r.h;

    // Two explanatory lines follow the buttons in draw(); the extent has to
    // include them or the last one sits under the bottom edge with no way to
    // reach it.
    contentW_ = field_.w + 2.0f * pad;
    contentH_ = (clearBtn_.y + clearBtn_.h + 60.0f * scale) - content.y;
}

void TextEditPage::syncKeyboard() {
    if (focused_) {
        if (show_) show_(ctx_, text(), buf_.cursor_pos());
    } else {
        if (hide_) hide_(ctx_);
    }
}

void TextEditPage::update(float dt, const FrameInput& in) {
    caretT_ += dt;

    // Tapping the field focuses it and raises the soft keyboard; tapping away
    // dismisses it. On desktop the hooks are empty and this is just focus.
    if (in.pointerWentUp) {
        const bool hit = field_.contains(in.pointerX, in.pointerY);
        if (hit != focused_) { focused_ = hit; syncKeyboard(); }
        else if (hit) syncKeyboard();   // re-raise a keyboard the user dismissed
    }

    if (undoBtn_.update(in) && undo_.can_undo()) { undo_.undo(buf_); syncKeyboard(); }
    if (redoBtn_.update(in) && undo_.can_redo()) { undo_.redo(buf_); syncKeyboard(); }
    if (clearBtn_.update(in)) {
        const std::string all = text();
        if (!all.empty()) {
            undo_.record_erase(0, all);
            while (buf_.length() > 0) { buf_.move_cursor_to(buf_.length()); buf_.erase_before(); }
        }
        syncKeyboard();
    }

    if (!focused_) return;

    // The IME path FIRST, and it wins: when the platform states the field's
    // whole contents, any per-character events in the same frame are part of
    // the run it just rewrote, and replaying them would double-type.
    if (in.textEdited) {
        const std::string before = text();
        if (before != in.editedText) {
            if (!before.empty()) undo_.record_erase(0, before);
            while (buf_.length() > 0) { buf_.move_cursor_to(buf_.length()); buf_.erase_before(); }
            buf_.insert(in.editedText);
            undo_.record_insert(0, in.editedText);
        }
        return;
    }

    // typedChars is a byte stream, and a non-ASCII character arrives as two to
    // four of them. Insert them all, but only offer the FIRST byte of each
    // character to the undo coalescer: coalescing per byte would let one undo
    // stop halfway through a character and leave the buffer invalid.
    for (size_t i = 0; i < in.typedChars.size(); ) {
        const size_t end = nextBoundary(in.typedChars, i);
        const std::string ch = in.typedChars.substr(i, end - i);
        i = end;
        // Control codes arrive here on some backends; backspace has its own
        // key edge below and must not also be inserted as a glyph.
        if (ch.size() == 1 && (unsigned char)ch[0] < 0x20) continue;
        const size_t pos = buf_.cursor_pos();
        // Coalescing is what makes undo step by WORD rather than by keystroke,
        // which is what a person expects one press of undo to do.
        if (ch.size() > 1 || !undo_.try_coalesce_insert(pos, ch[0]))
            undo_.record_insert(pos, ch);
        buf_.insert(ch);
    }

    if (in.keyWentDown(keys::Back) && buf_.cursor_pos() > 0) {
        const std::string& all = text();
        const size_t pos   = buf_.cursor_pos();
        const size_t start = prevBoundary(all, pos);
        undo_.record_erase(start, all.substr(start, pos - start));
        // One erase_before() per BYTE of the one character.
        for (size_t i = start; i < pos; ++i) buf_.erase_before();
    }
    if (in.keyWentDown(key::Delete)) {
        const std::string& all = text();
        const size_t pos = buf_.cursor_pos();
        const size_t end = nextBoundary(all, pos);
        for (size_t i = pos; i < end; ++i) buf_.erase_after();
    }
    if (in.keyWentDown(keys::Left))
        buf_.move_cursor_to(prevBoundary(text(), buf_.cursor_pos()));
    if (in.keyWentDown(keys::Right))
        buf_.move_cursor_to(nextBoundary(text(), buf_.cursor_pos()));
    if (in.keyWentDown(key::Home))   buf_.move_cursor_to(0);
    if (in.keyWentDown(key::End))    buf_.move_cursor_to(buf_.length());

    // Ctrl+Z / Ctrl+Y, for the desktop half.
    if (in.ctrlDown && in.keyWentDown(keys::Z) && undo_.can_undo()) undo_.undo(buf_);
    if (in.ctrlDown && in.keyWentDown(keys::Y) && undo_.can_redo()) undo_.redo(buf_);
}

void TextEditPage::draw(Canvas& c) {
    const float pad = 22.0f * scale_;
    c.text("Text entry, undo/redo & IME", content_.x + pad, content_.y + pad,
           24.0f * scale_, gray(0.95f));

    c.rect(field_.x, field_.y, field_.w, field_.h,
           gray(focused_ ? 0.28f : 0.22f), 6.0f * scale_);

    const float textSize = 22.0f * scale_;
    const float tx = field_.x + 12.0f * scale_;
    const float ty = field_.y + 14.0f * scale_;

    const std::string& all = text();
    c.setClip(field_.x, field_.y, field_.w, field_.h);
    c.text(all, tx, ty, textSize, gray(0.96f));

    // The caret sits after the cursor's BYTE position, so its x is the width of
    // the text up to there — measured, never guessed from a character count,
    // which would be wrong the moment a multi-byte character is on the line.
    if (focused_ && std::fmod(caretT_, 1.06f) < 0.53f) {
        const std::string upto = all.substr(0, buf_.cursor_pos());
        const float caretX = tx + c.textWidth(upto, textSize);
        c.rect(caretX, ty, 2.0f * scale_, textSize * 1.15f, gray(0.98f));
    }
    c.clearClip();

    undoBtn_.draw(c, undo_.can_undo() ? "Undo" : "Undo -");
    redoBtn_.draw(c, undo_.can_redo() ? "Redo" : "Redo -");
    clearBtn_.draw(c, "Clear");

    char line[160];
    std::snprintf(line, sizeof line, "%zu bytes, cursor at %zu%s",
                  buf_.length(), buf_.cursor_pos(),
                  focused_ ? "  -  focused" : "  -  tap the field to type");
    c.text(line, content_.x + pad, undoBtn_.y + undoBtn_.h + 18.0f * scale_,
           15.0f * scale_, gray(0.78f));
    c.text("an IME replaces the whole buffer (onTextEditPortable), so CJK composition works",
           content_.x + pad, undoBtn_.y + undoBtn_.h + 40.0f * scale_,
           14.0f * scale_, gray(0.68f));
}
