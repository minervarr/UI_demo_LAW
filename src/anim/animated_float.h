#pragma once

using EaseFn = float(*)(float);

float easeLinear(float t);
float easeInOutCubic(float t);

class AnimatedFloat {
 public:
    explicit AnimatedFloat(float initial = 0.0f)
        : current_(initial), from_(initial), to_(initial) {}

    void set(float target, float durationSeconds, EaseFn ease = easeLinear) {
        from_ = current_;
        to_ = target;
        duration_ = durationSeconds;
        elapsed_ = 0.0f;
        ease_ = ease;
    }

    void update(float dtSeconds) {
        if (elapsed_ >= duration_) { current_ = to_; return; }
        elapsed_ += dtSeconds;
        if (elapsed_ >= duration_) { current_ = to_; return; }
        float t = duration_ > 0.0f ? elapsed_ / duration_ : 1.0f;
        current_ = from_ + (to_ - from_) * ease_(t);
    }

    float value() const { return current_; }
    bool isAnimating() const { return elapsed_ < duration_; }

 private:
    float current_, from_, to_;
    float elapsed_ = 0.0f, duration_ = 0.0f;
    EaseFn ease_ = easeLinear;
};
