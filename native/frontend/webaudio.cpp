#include "webaudio.hpp"
#include <algorithm>
#include <stdexcept>

namespace ssx_native {

namespace wa {

// ---- AudioParam ----

void Param::insert(Event e) {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    // after every event at the same time (insertion order), before later ones
    auto it = std::upper_bound(events_.begin(), events_.end(), e.time, [](double t, const Event& x) { return t < x.time; });
    events_.insert(it, std::move(e));
}

double Param::valueAt(double t, size_t count) const {
    double value = intrinsic_;
    double prevTime = 0;
    bool hasPrev = false;
    // the event in effect (a SetTarget keeps changing after its time)
    const Event* target = nullptr;
    double targetStart = 0;
    for (size_t idx = 0; idx < std::min(count, events_.size()); ++idx) {
        const Event& e = events_[idx];
        if (e.time > t) {
            if (e.kind == LinearRamp && hasPrev) {
                const double v0 = target ? target->value + (targetStart - target->value) * std::exp(-(prevTime - target->time) / target->tau) : value;
                if (e.time <= prevTime) {
                    return e.value;
                }
                return v0 + (e.value - v0) * (t - prevTime) / (e.time - prevTime);
            }
            break;
        }
        // the value just before this event (a SetTarget in progress at this time)
        if (target) {
            value = target->value + (targetStart - target->value) * std::exp(-(e.time - target->time) / target->tau);
            target = nullptr;
        }
        switch (e.kind) {
        case SetValue:
        case LinearRamp:
            value = e.value;
            prevTime = e.time;
            break;
        case SetTarget:
            target = &e;
            targetStart = value;
            prevTime = e.time;
            break;
        case Curve: {
            const auto& c = *e.curve;
            const size_t n = c.size();
            if (t < e.time + e.duration && n > 0) {
                const double pos = double(n - 1) * (t - e.time) / e.duration;
                const size_t k = size_t(std::floor(pos));
                if (k + 1 >= n) {
                    return c[n - 1];
                }
                return c[k] + (c[k + 1] - c[k]) * (pos - double(k));
            }
            value = n ? c[n - 1] : value;
            prevTime = e.time + e.duration;
            break;
        }
        }
        hasPrev = true;
    }
    if (target) {
        return target->value + (targetStart - target->value) * std::exp(-(t - target->time) / target->tau);
    }
    return value;
}

double Param::value() const {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    return float(valueAt(ctx_->currentTime()));
}

void Param::setValue(double v) {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    if (events_.empty()) {
        intrinsic_ = v;
        return;
    }
    setValueAtTime(v, ctx_->currentTime());
}

void Param::setValueAtTime(double v, double t) {
    insert({SetValue, std::max(0.0, t), v});
}

void Param::linearRampToValueAtTime(double v, double t) {
    insert({LinearRamp, std::max(0.0, t), v});
}

void Param::setTargetAtTime(double target, double t, double tau) {
    if (tau <= 0) {
        setValueAtTime(target, t);
        return;
    }
    Event e{SetTarget, std::max(0.0, t), target};
    e.tau = tau;
    insert(e);
}

void Param::setValueCurveAtTime(const std::vector<float>& curve, double t, double duration) {
    if (curve.size() < 2 || !(duration > 0)) {
        throw std::logic_error("setValueCurveAtTime: a curve of 2 or more values over a positive duration");
    }
    Event e{Curve, std::max(0.0, t), curve.back()};
    e.duration = duration;
    e.curve = std::make_shared<std::vector<float>>(curve);
    insert(e);
}

void Param::cancelScheduledValues(double t) {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    events_.erase(std::remove_if(events_.begin(), events_.end(), [&](const Event& e) { return e.time >= t; }), events_.end());
}

void Param::cancelAndHoldAtTime(double t) {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    const double held = valueAt(t);
    // the first event after t: a linear ramp in progress is cut to end at t on the held value (the spec's E2 rule); a SetTarget or
    // curve in progress gives way to a SetValue at t
    auto next = std::find_if(events_.begin(), events_.end(), [&](const Event& e) { return e.time > t; });
    const bool ramp = next != events_.end() && next->kind == LinearRamp && next != events_.begin();
    events_.erase(next, events_.end());
    events_.erase(std::remove_if(events_.begin(), events_.end(), [&](const Event& e) { return e.kind == Curve && e.time + e.duration > t; }), events_.end());
    if (ramp) {
        events_.push_back({LinearRamp, t, held});
    } else {
        events_.push_back({SetValue, t, held});
    }
}

void Param::compute(int64_t f0, float* out) {
    const double sr = ctx_->sampleRate();
    const double now = double(f0) / sr;
    // events wholly in the past collapse (the list stays short, a SetTarget every frame included): everything before the last event
    // that has started becomes one SetValue of the value just before it, at its time
    size_t k = 0;
    while (k < events_.size() && events_[k].time <= now) {
        ++k;
    }
    if (k >= 2) {
        const size_t last = k - 1;
        const double t = events_[last].time;
        const double before = valueAt(t, last);
        events_.erase(events_.begin(), events_.begin() + long(last));
        events_.insert(events_.begin(), Event{SetValue, t, before});
    }
    if (!events_.empty() && events_.size() <= 2) {
        // a finished SetValue / LinearRamp at the end: the intrinsic value
        const Event& e = events_.back();
        if ((e.kind == SetValue || e.kind == LinearRamp) && e.time <= now) {
            intrinsic_ = e.value;
            events_.clear();
        }
    }
    if (events_.empty()) {
        std::fill(out, out + kQuantum, float(intrinsic_));
    } else if (aRate_) {
        for (int i = 0; i < kQuantum; ++i) {
            out[i] = float(valueAt(double(f0 + i) / sr));
        }
    } else {
        std::fill(out, out + kQuantum, float(valueAt(now)));
    }
    // audio-rate inputs, mixed down to mono (a k-rate param takes each input's first frame)
    const int64_t quantum = f0 / kQuantum;
    const auto list = inputs;
    for (const auto& n : list) {
        const Block& b = n->pull(quantum);
        if (b.channels == 0) {
            continue;
        }
        auto mono = [&](int i) { return b.channels == 2 ? 0.5f * (b.data[0][size_t(i)] + b.data[1][size_t(i)]) : b.data[0][size_t(i)]; };
        if (aRate_) {
            for (int i = 0; i < kQuantum; ++i) {
                out[i] += mono(i);
            }
        } else {
            const float s0 = mono(0);
            for (int i = 0; i < kQuantum; ++i) {
                out[i] += s0;
            }
        }
    }
}

float Param::computeK(int64_t f0) {
    float v[kQuantum];
    compute(f0, v);
    return v[0];
}

// ---- nodes ----

void Node::connect(const std::shared_ptr<Node>& dest) {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    for (const auto& n : dest->inputs) {
        if (n.get() == this) {
            return;
        }
    }
    dest->inputs.push_back(shared_from_this());
    outs_.push_back({dest, nullptr, {}});
}

void Node::connect(Param& param) {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    for (const auto& n : param.inputs) {
        if (n.get() == this) {
            return;
        }
    }
    param.inputs.push_back(shared_from_this());
    outs_.push_back({{}, &param, {}});
}

void Node::disconnect() {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    auto self = shared_from_this();
    for (auto& o : outs_) {
        if (auto n = o.node.lock()) {
            auto& in = n->inputs;
            in.erase(std::remove(in.begin(), in.end(), self), in.end());
        } else if (o.param) {
            auto& in = o.param->inputs;
            in.erase(std::remove(in.begin(), in.end(), self), in.end());
        }
    }
    outs_.clear();
}

const Block& Node::pull(int64_t quantum) {
    if (pulled_ != quantum) {
        pulled_ = quantum;
        process(quantum, block_);
    }
    return block_;
}

void Node::sumInputs(int64_t quantum, Block& in) {
    int channels = 0;
    // inputs can be disconnected while pulled: iterate a copy
    const auto list = inputs;
    std::vector<const Block*> blocks;
    for (const auto& n : list) {
        const Block& b = n->pull(quantum);
        if (b.channels > 0) {
            blocks.push_back(&b);
            channels = std::max(channels, b.channels);
        }
    }
    in.silence(std::max(channels, 1));
    in.channels = channels;
    for (const Block* b : blocks) {
        for (int c = 0; c < channels; ++c) {
            // up-mix mono to stereo (speakers): the same samples on both
            const auto& src = b->data[size_t(b->channels == 1 ? 0 : c)];
            auto& dst = in.data[size_t(c)];
            for (int i = 0; i < kQuantum; ++i) {
                dst[size_t(i)] += src[size_t(i)];
            }
        }
    }
}

void GainNode::process(int64_t quantum, Block& out) {
    sumInputs(quantum, out);
    float g[kQuantum];
    gain.compute(quantum * kQuantum, g);
    for (int c = 0; c < out.channels; ++c) {
        for (int i = 0; i < kQuantum; ++i) {
            out.data[size_t(c)][size_t(i)] *= g[i];
        }
    }
}

void BufferSourceNode::start(double when, double offset, double duration) {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    if (started_) {
        throw std::logic_error("InvalidStateError: start() called twice");
    }
    started_ = true;
    startTime_ = std::max(0.0, when);
    offset_ = std::max(0.0, offset);
    duration_ = duration;
    ctx_->playing.push_back(std::static_pointer_cast<BufferSourceNode>(shared_from_this()));
}

void BufferSourceNode::stop(double when) {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    if (!started_) {
        throw std::logic_error("InvalidStateError: stop() before start()");
    }
    stopCalled_ = true;
    stopTime_ = std::max(0.0, when);
}

void BufferSourceNode::process(int64_t quantum, Block& out) {
    const int channels = buffer ? int(std::min<size_t>(2, buffer->channels.size())) : 1;
    out.silence(channels);
    const double sr = ctx_->sampleRate();
    const int64_t f0 = quantum * kQuantum;
    if (!started_ || ended_ || !buffer || buffer->length == 0) {
        return;
    }
    const double rateParam = playbackRate.computeK(f0);
    const double detuneParam = detune.computeK(f0);
    const double rate = rateParam * std::pow(2.0, detuneParam / 1200) * double(buffer->sampleRate) / sr;
    const int64_t startFrame = int64_t(std::ceil(startTime_ * sr - 1e-9));
    const int64_t stopFrame = std::isfinite(stopTime_) ? int64_t(std::ceil(stopTime_ * sr - 1e-9)) : INT64_MAX;
    const double length = double(buffer->length);
    double ls = 0;
    double le = length;
    if (loop && loopStart >= 0 && loopEnd > 0 && loopStart < loopEnd) {
        ls = loopStart * buffer->sampleRate;
        le = std::min(length, loopEnd * buffer->sampleRate);
    }
    const double maxPlay = duration_ >= 0 ? duration_ * buffer->sampleRate : INFINITY;
    for (int i = 0; i < kQuantum; ++i) {
        const int64_t f = f0 + i;
        if (f < startFrame) {
            continue;
        }
        if (f >= stopFrame) {
            ended_ = true;
            break;
        }
        if (!playing_) {
            // Chromium's sub-sample start: the playhead begins where it would be at this frame had it started at startTime
            playing_ = true;
            position_ = std::min(offset_ * buffer->sampleRate + (double(f) - startTime_ * sr) * rate, length);
        }
        if (!loop && position_ >= length) {
            ended_ = true;
            break;
        }
        if (played_ >= maxPlay) {
            ended_ = true;
            break;
        }
        // Chromium's RenderFromBuffer: the second sample wraps by the loop length at the loop's end (looping), or the last two
        // samples are extrapolated at the buffer's end
        const double p = position_;
        const int64_t i0 = int64_t(std::floor(p));
        const double frac = p - double(i0);
        int64_t i1 = i0 + 1;
        bool extrapolate = false;
        const double endFrame = loop ? le : double(buffer->length);
        if (double(i1) >= endFrame) {
            if (loop) {
                i1 = int64_t(p + 1 - (le - ls));
            } else {
                i1 = i0;
                extrapolate = i0 >= 1;
            }
        }
        for (int c = 0; c < channels; ++c) {
            const auto& d = buffer->channels[size_t(c)];
            auto at = [&](int64_t k) { return (k >= 0 && size_t(k) < d.size()) ? double(d[size_t(k)]) : 0.0; };
            double v;
            if (extrapolate) {
                const double s1 = at(i0 - 1);
                const double s2 = at(i0);
                v = s2 + (s2 - s1) * frac;
            } else {
                v = (1.0 - frac) * at(i0) + frac * at(i1);
            }
            out.data[size_t(c)][size_t(i)] = float(v);
        }
        position_ += rate;
        played_ += rate;
        if (loop && position_ >= le && le > ls) {
            position_ -= le - ls;
            if (position_ >= le) {
                position_ = ls + std::fmod(position_ - ls, le - ls);
            }
        }
    }
}

void StereoPannerNode::process(int64_t quantum, Block& out) {
    Block in;
    sumInputs(quantum, in);
    out.silence(2);
    if (in.channels == 0) {
        return;
    }
    float p[kQuantum];
    pan.compute(quantum * kQuantum, p);
    const double half = M_PI / 2;
    for (int i = 0; i < kQuantum; ++i) {
        const double v = std::clamp(double(p[i]), -1.0, 1.0);
        if (in.channels == 1) {
            const double x = (v + 1) / 2;
            const float s = in.data[0][size_t(i)];
            out.data[0][size_t(i)] = float(s * std::cos(x * half));
            out.data[1][size_t(i)] = float(s * std::sin(x * half));
        } else {
            const float l = in.data[0][size_t(i)];
            const float r = in.data[1][size_t(i)];
            if (v <= 0) {
                const double x = v + 1;
                out.data[0][size_t(i)] = float(l + r * std::cos(x * half));
                out.data[1][size_t(i)] = float(r * std::sin(x * half));
            } else {
                const double x = v;
                out.data[0][size_t(i)] = float(l * std::cos(x * half));
                out.data[1][size_t(i)] = float(r + l * std::sin(x * half));
            }
        }
    }
}

void BiquadFilterNode::process(int64_t quantum, Block& out) {
    Block in;
    sumInputs(quantum, in);
    out = in;
    if (in.channels == 0) {
        return;
    }
    const double sr = ctx_->sampleRate();
    const int64_t f0 = quantum * kQuantum;
    const double f = std::clamp(double(frequency.computeK(f0)), 0.0, sr / 2);
    const double q = Q.computeK(f0);
    // the spec's lowpass (Q in dB)
    const double w0 = 2 * M_PI * f / sr;
    const double alpha = std::sin(w0) / (2 * std::pow(10.0, q / 20));
    const double cw = std::cos(w0);
    double b0 = (1 - cw) / 2;
    double b1 = 1 - cw;
    double b2 = (1 - cw) / 2;
    const double a0 = 1 + alpha;
    double a1 = -2 * cw;
    double a2 = 1 - alpha;
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
    for (int c = 0; c < in.channels; ++c) {
        auto& s = state_[size_t(c)];
        for (int i = 0; i < kQuantum; ++i) {
            const double x = in.data[size_t(c)][size_t(i)];
            const double y = b0 * x + b1 * s[0] + b2 * s[1] - a1 * s[2] - a2 * s[3];
            s[1] = s[0];
            s[0] = x;
            s[3] = s[2];
            s[2] = y;
            out.data[size_t(c)][size_t(i)] = float(y);
        }
    }
}

void DestinationNode::process(int64_t quantum, Block& out) {
    Block in;
    sumInputs(quantum, in);
    out.silence(2);
    if (in.channels == 1) {
        out.data[0] = in.data[0];
        out.data[1] = in.data[0];
    } else if (in.channels == 2) {
        out.data = in.data;
    }
}

// ---- the context ----

Context::Context(float sampleRate) : sampleRate_(sampleRate) {
    destination = std::make_shared<DestinationNode>(this);
}

std::shared_ptr<GainNode> Context::createGain() {
    return std::make_shared<GainNode>(this);
}

std::shared_ptr<BufferSourceNode> Context::createBufferSource() {
    return std::make_shared<BufferSourceNode>(this);
}

std::shared_ptr<StereoPannerNode> Context::createStereoPanner() {
    return std::make_shared<StereoPannerNode>(this);
}

std::shared_ptr<BiquadFilterNode> Context::createBiquadFilter() {
    return std::make_shared<BiquadFilterNode>(this);
}

std::shared_ptr<Buffer> Context::createBuffer(int channels, size_t length, float sampleRate) {
    auto b = std::make_shared<Buffer>();
    b->sampleRate = sampleRate;
    b->length = length;
    b->channels.assign(size_t(channels), std::vector<float>(length, 0.f));
    return b;
}

void Context::renderQuantum() {
    if (state != "running") {
        out_.silence(2);
        return;
    }
    const Block& b = destination->pull(quantum_);
    out_ = b;
    // every playing source advances even when nothing pulls it (its onended still fires)
    for (const auto& s : playing) {
        s->pull(quantum_);
    }
    ++quantum_;
    frames_ += kQuantum;
    for (auto it = playing.begin(); it != playing.end();) {
        if ((*it)->ended_) {
            ended_.push_back(*it);
            it = playing.erase(it);
        } else {
            ++it;
        }
    }
}

void Context::render(float* interleaved, size_t frames) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    size_t done = 0;
    while (done < frames) {
        if (outPos_ >= size_t(kQuantum)) {
            renderQuantum();
            outPos_ = 0;
        }
        const size_t n = std::min(frames - done, size_t(kQuantum) - outPos_);
        for (size_t i = 0; i < n; ++i) {
            interleaved[2 * (done + i)] = out_.data[0][outPos_ + i];
            interleaved[2 * (done + i) + 1] = out_.data[1][outPos_ + i];
        }
        outPos_ += n;
        done += n;
    }
}

void Context::queueEnded(std::shared_ptr<BufferSourceNode> node) {
    ended_.push_back(std::move(node));
}

void Context::dispatch() {
    std::vector<std::shared_ptr<BufferSourceNode>> ended;
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        ended.swap(ended_);
    }
    for (const auto& s : ended) {
        if (s->onended) {
            auto fn = s->onended;
            fn();
        }
    }
}

}  // namespace wa

}  // namespace ssx_native
