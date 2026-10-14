#pragma once
// The Web Audio API subset the page's audio modules use (web/audio-engine.js, sfx.js, pathfinder.js, audio-speech.js, ...),
// natively, so those modules port 1:1 onto the same calls: an AudioContext rendering 128-frame quanta, AudioParams with the
// spec's automation (setValueAtTime, linearRampToValueAtTime, setTargetAtTime, setValueCurveAtTime, cancelScheduledValues,
// cancelAndHoldAtTime) and audio-rate inputs (a node connected to a param), and the nodes GainNode, AudioBufferSourceNode
// (linear interpolation, loops, start / stop times, onended), StereoPannerNode (equal power), BiquadFilterNode (lowpass) and
// the destination. Channel up-mixing follows the spec's 'speakers' rules for mono and stereo.
//
// Threads: the game thread builds and changes the graph, the audio thread (SDL3's stream callback, or render() offline) pulls
// it; one mutex guards both (lock()). onended runs on the game thread (dispatch()).
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ssx_native {

namespace wa {

constexpr int kQuantum = 128;

class Context;
class Node;

// one quantum of audio: 1 or 2 channels
struct Block {
    int channels = 0;
    std::array<std::array<float, kQuantum>, 2> data{};
    void silence(int ch) {
        channels = ch;
        for (int c = 0; c < ch; ++c) {
            data[size_t(c)].fill(0.f);
        }
    }
};

class Param {
public:
    Param(Context* ctx, double defaultValue, bool aRate) : ctx_(ctx), default_(defaultValue), intrinsic_(defaultValue), aRate_(aRate) {}
    // the value now (the intrinsic value at the context's current time)
    double value() const;
    void setValue(double v);
    void setValueAtTime(double v, double t);
    void linearRampToValueAtTime(double v, double t);
    void setTargetAtTime(double target, double t, double tau);
    void setValueCurveAtTime(const std::vector<float>& curve, double t, double duration);
    void cancelScheduledValues(double t);
    void cancelAndHoldAtTime(double t);

    // render: the param's value for each frame of the quantum starting at frame `f0` (audio thread)
    void compute(int64_t f0, float* out);
    // its value for the first frame (k-rate)
    float computeK(int64_t f0);
    std::vector<std::shared_ptr<Node>> inputs;

private:
    enum Kind { SetValue, LinearRamp, SetTarget, Curve };
    struct Event {
        Kind kind;
        double time;
        double value;
        double tau = 0;
        double duration = 0;
        std::shared_ptr<std::vector<float>> curve;
    };
    void insert(Event e);
    // the value at t from the first `count` events
    double valueAt(double t, size_t count = SIZE_MAX) const;
    Context* ctx_;
    double default_;
    double intrinsic_;
    bool aRate_;
    std::vector<Event> events_;
};

class Node : public std::enable_shared_from_this<Node> {
public:
    explicit Node(Context* ctx) : ctx_(ctx) {}
    virtual ~Node() = default;
    void connect(const std::shared_ptr<Node>& dest);
    void connect(Param& param);
    // every outgoing connection removed
    void disconnect();
    // this quantum's output (pulled once per quantum, cached)
    const Block& pull(int64_t quantum);
    std::vector<std::shared_ptr<Node>> inputs;

protected:
    // the inputs summed (up-mixed to the widest)
    void sumInputs(int64_t quantum, Block& in);
    virtual void process(int64_t quantum, Block& out) = 0;
    Context* ctx_;

private:
    struct Out {
        std::weak_ptr<Node> node;
        Param* param = nullptr;
        // the param's owner (kept so a param target outliving its node is detected)
        std::weak_ptr<Node> paramOwner;
    };
    std::vector<Out> outs_;
    int64_t pulled_ = -1;
    Block block_;
    friend class Context;
};

class GainNode : public Node {
public:
    explicit GainNode(Context* ctx) : Node(ctx), gain(ctx, 1, true) {}
    Param gain;

protected:
    void process(int64_t quantum, Block& out) override;
};

// an AudioBuffer: planar float channels at its own rate
struct Buffer {
    float sampleRate = 48000;
    size_t length = 0;
    std::vector<std::vector<float>> channels;
    double duration() const {
        return double(length) / sampleRate;
    }
};

class BufferSourceNode : public Node {
public:
    explicit BufferSourceNode(Context* ctx) : Node(ctx), playbackRate(ctx, 1, false), detune(ctx, 0, false) {}
    std::shared_ptr<const Buffer> buffer;
    bool loop = false;
    double loopStart = 0;
    double loopEnd = 0;
    Param playbackRate;
    Param detune;
    // start(when, offset, duration); stop(when): the spec's errors (InvalidStateError) are thrown as std::logic_error
    void start(double when = 0, double offset = 0, double duration = -1);
    void stop(double when = 0);
    std::function<void()> onended;
    bool started() const {
        return started_;
    }

protected:
    void process(int64_t quantum, Block& out) override;

private:
    bool started_ = false;
    bool stopCalled_ = false;
    double startTime_ = 0;
    double stopTime_ = INFINITY;
    double offset_ = 0;
    double duration_ = -1;
    // the playhead in buffer frames, set at the first rendered frame
    double position_ = 0;
    bool playing_ = false;
    bool ended_ = false;
    double played_ = 0;
    friend class Context;
};

class StereoPannerNode : public Node {
public:
    explicit StereoPannerNode(Context* ctx) : Node(ctx), pan(ctx, 0, true) {}
    Param pan;

protected:
    void process(int64_t quantum, Block& out) override;
};

class BiquadFilterNode : public Node {
public:
    explicit BiquadFilterNode(Context* ctx) : Node(ctx), frequency(ctx, 350, true), Q(ctx, 1, true), gain(ctx, 0, true) {}
    // 'lowpass' only (the page's music filter)
    std::string type = "lowpass";
    Param frequency;
    Param Q;
    Param gain;

protected:
    void process(int64_t quantum, Block& out) override;

private:
    std::array<std::array<double, 4>, 2> state_{};
};

class DestinationNode : public Node {
public:
    explicit DestinationNode(Context* ctx) : Node(ctx) {}

protected:
    void process(int64_t quantum, Block& out) override;
};

class Context {
public:
    explicit Context(float sampleRate = 48000);
    float sampleRate() const {
        return sampleRate_;
    }
    // seconds rendered so far (the game thread reads it like ctx.currentTime)
    double currentTime() const {
        return double(frames_.load()) / sampleRate_;
    }
    std::shared_ptr<DestinationNode> destination;
    std::shared_ptr<GainNode> createGain();
    std::shared_ptr<BufferSourceNode> createBufferSource();
    std::shared_ptr<StereoPannerNode> createStereoPanner();
    std::shared_ptr<BiquadFilterNode> createBiquadFilter();
    std::shared_ptr<Buffer> createBuffer(int channels, size_t length, float sampleRate);
    // 'running' or 'suspended': a suspended context renders silence and its clock stops
    std::string state = "running";

    // the audio thread: `frames` interleaved stereo frames
    void render(float* interleaved, size_t frames);
    // the game thread: the graph lock (hold it while changing the graph from the game thread)
    std::recursive_mutex& mutex() {
        return mutex_;
    }
    // the game thread: queued onended callbacks run
    void dispatch();
    // the audio thread: a node's onended queued for the game thread
    void queueEnded(std::shared_ptr<BufferSourceNode> node);
    // every source that has been started and has not ended (kept alive while it plays, as the spec's sources are)
    std::vector<std::shared_ptr<BufferSourceNode>> playing;

private:
    void renderQuantum();
    float sampleRate_;
    std::atomic<int64_t> frames_{0};
    int64_t quantum_ = 0;
    Block out_;
    size_t outPos_ = kQuantum;
    std::recursive_mutex mutex_;
    std::vector<std::shared_ptr<BufferSourceNode>> ended_;
};

}  // namespace wa

}  // namespace ssx_native
