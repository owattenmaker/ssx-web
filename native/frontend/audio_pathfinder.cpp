#include "audio_pathfinder.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ssx_native {

namespace audio {

namespace pf {

namespace {

constexpr double kMainLatency = 500;
constexpr int kMainCapacity = 2;
constexpr double kLoopLatency = 10;
constexpr int kLoopCapacity = 1;
constexpr double kSyncBelow = 50;
constexpr size_t kMaxEvents = 16;
constexpr double kMixerRate = 36000;
constexpr double kEps = 0.5;
constexpr double kShiftMaxS = 0.012;

int s8(int v) {
    return int(int8_t(uint8_t(v)));
}
int s16(int v) {
    return int(int16_t(uint16_t(v)));
}
int s4(int v) {
    return int(int32_t(uint32_t(v & 0xf) << 28) >> 28);
}

Action parseAction(const std::string& raw) {
    std::vector<uint8_t> b;
    for (size_t i = 0; i + 1 < raw.size(); i += 2) {
        b.push_back(uint8_t(std::stoi(raw.substr(i, 2), nullptr, 16)));
    }
    b.resize(std::max<size_t>(b.size(), 16), 0);
    auto u32 = [&](size_t p) { return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) | (uint32_t(b[p + 2]) << 16) | (uint32_t(b[p + 3]) << 24); };
    Action a;
    a.mask = uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16);
    a.timeout = u32(4);
    a.op = b[9];
    a.word = u32(12);
    a.value = s16(b[12] | (b[13] << 8));
    a.arg = s8(b[14]);
    a.argU = b[14];
    a.flags = b[15];
    a.b12 = s8(b[12]);
    a.b13 = b[13];
    a.u16_14 = b[14] | (b[15] << 8);
    return a;
}

}  // namespace

Graph buildGraph(const Json& json) {
    Graph g;
    g.id = json.value("id", "");
    g.inf = json.contains("inf") ? json["inf"] : Json::object();
    for (const auto& n : json["nodes"]) {
        Node nd;
        const uint32_t w4 = uint32_t(n.value("w4", int64_t(0)));
        const uint32_t w8 = uint32_t(n.value("w8", int64_t(0)));
        nd.index = n.value("index", 0);
        nd.sample = n.value("sample", 0);
        nd.flags = n.value("flags", 0);
        nd.track = nd.flags & 0x1f;
        nd.section = (nd.flags >> 5) & 0x3f;
        nd.loopInit = (nd.flags >> 12) & 0xf;
        nd.router = int(w4 & 0xff);
        nd.measures = int((w4 >> 20) & 0xf);
        nd.beats = int((w4 >> 24) & 0xf);
        nd.random = (w4 & 0xe0000) == 0x20000;
        nd.sync = int(w8 & 0x300);
        nd.group = int(w8 >> 20);
        nd.trigger = int((w8 >> 10) & 0x1f);
        nd.w12 = uint32_t(n.value("w12", int64_t(0)));
        nd.sliced = (nd.w12 & 2) != 0;
        for (const auto& b : n["branches"]) {
            nd.branches.push_back({s8(b["lo"].get<int>()), s8(b["hi"].get<int>()), s16(b["node"].get<int>())});
        }
        g.nodes.push_back(nd);
    }
    for (const auto& e : json["events"]) {
        EventDef ev;
        ev.id = e.value("id", 0);
        for (const auto& a : e["actions"]) {
            ev.actions.push_back(parseAction(a["raw"].get<std::string>()));
        }
        g.events.push_back(ev);
    }
    for (const auto& s : json["samples"]) {
        Sample sm;
        sm.index = s.value("index", 0);
        sm.track = s.value("track", 0);
        sm.kind = s.value("kind", "");
        const double count = s.value("sampleCount", 0.0);
        const double rate = s.value("sampleRate", 0.0);
        sm.durMs = count != 0 && rate != 0 ? (count * 1000) / rate : s.value("ms", 0.0);
        sm.offset = s.value("offset", size_t(0));
        sm.size = s.value("size", size_t(0));
        if (s.contains("bankIndex")) {
            sm.bankIndex = s["bankIndex"].get<int>();
        }
        sm.value = s.value("value", 0);
        sm.codec = s.value("codec", "");
        g.samples.push_back(sm);
    }
    if (json.contains("routers")) {
        for (const auto& r : json["routers"]) {
            std::vector<std::pair<int, int>> list;
            for (const auto& p : r) {
                list.push_back({p["fromNode"].get<int>(), p["toNode"].get<int>()});
            }
            g.routers.push_back(list);
        }
    }
    return g;
}

std::optional<Decoded> Song::decodeSegment(int sample, std::optional<int> bankIndex) {
    if (sample < 1 || size_t(sample) > graph.samples.size()) {
        return std::nullopt;
    }
    const Sample& s = graph.samples[size_t(sample - 1)];
    if (s.kind == "stream") {
        if (!mus || s.offset + s.size > mus->size()) {
            return std::nullopt;
        }
        return decodeStream(mus->data() + s.offset, s.size);
    }
    if (!loops) {
        return std::nullopt;
    }
    if (!bank) {
        bank = parseBank(loops->data(), loops->size());
    }
    if (!bankIndex || *bankIndex < 0 || size_t(*bankIndex) >= bank->entries.size()) {
        return std::nullopt;
    }
    const auto& entry = bank->entries[size_t(*bankIndex)];
    if (entry.index < 0 || entry.patches.empty()) {
        return std::nullopt;
    }
    return decodeBankPatch(loops->data(), loops->size(), entry.patches[0]);
}

std::shared_ptr<Song> loadSong(Files& files, const std::string& id) {
    auto json = files.json("music/" + id + ".json");
    auto song = std::make_shared<Song>();
    song->id = json->value("id", id);
    song->graph = buildGraph(*json);
    song->inf = song->graph.inf;
    for (const auto& t : (*json)["tracks"]) {
        if (t.value("kind", "") == "stream" && !song->mus) {
            song->mus = files.bytes("music/" + t["file"].get<std::string>());
        } else if (t.value("kind", "") == "bank" && !song->loops) {
            song->loops = files.bytes("music/" + t["file"].get<std::string>());
        }
    }
    return song;
}

double rampLevel(double from, double to, double t, int curve) {
    if (t >= 1) {
        return to;
    }
    const double d = to - from;
    const bool rising = d >= 0;
    const double tp = rising ? 1 - t : t;
    double f;
    if (curve == 1) {
        f = 1 - tp;
    } else if (curve == 2) {
        f = 1 - tp * tp;
    } else if (curve == 3) {
        f = 1 / tp / 25;
    } else {
        return from;
    }
    if (!(f >= 0 && f <= 1)) {
        return from;
    }
    return std::min(from, to) + std::abs(d) * f;
}

// ---- the core ----

Core::Core(const Graph& g, std::function<double()> random, int intensity, std::function<double(double)> clock)
    : g_(g), random_(std::move(random)), clock_(std::move(clock)) {
    auto makeVoice = [&](int track) {
        const bool main = track == 0;
        Voice v;
        v.track = track;
        v.latency = main ? kMainLatency : kLoopLatency;
        v.capacity = main ? kMainCapacity : kLoopCapacity;
        v.sync = v.latency < kSyncBelow;
        v.slice = main ? -1 : 0;
        v.intensity = intensity & 0x7f;
        return v;
    };
    st_.voices.push_back(makeVoice(0));
    bool hasTrack1 = false;
    for (const auto& s : g.samples) {
        hasTrack1 = hasTrack1 || s.track == 1;
    }
    if (hasTrack1) {
        st_.voices.push_back(makeVoice(1));
    }
}

int Core::rand7() {
    const uint64_t i = st_.rngIdx - rngBase_;
    double v;
    if (i < rngLog_.size()) {
        v = rngLog_[size_t(i)];
    } else {
        v = random_();
        rngLog_.push_back(v);
    }
    st_.rngIdx++;
    return int(std::floor(v * 128)) & 0x7f;
}

int Core::route(int prev, int n) {
    const Node* pn = node(prev);
    if (!pn || !pn->router) {
        return n;
    }
    if (size_t(pn->router - 1) < g_.routers.size()) {
        for (const auto& p : g_.routers[size_t(pn->router - 1)]) {
            if (n == p.first) {
                n = p.second;
            }
        }
    }
    return n;
}

int Core::choose(Voice& v, int n, int value) {
    const Node* nd = node(n);
    if (!nd) {
        return -1;
    }
    Voice& tv = size_t(nd->track) < st_.voices.size() ? st_.voices[size_t(nd->track)] : v;
    int val = value;
    if (tv.loopNode == n) {
        const int c = s4(tv.loopCount);
        val = c & 0x7f;
        if (s4(nd->loopInit) > 0) {
            const int c2 = c - 1;
            tv.loopCount = c2 & 0xf;
            if (s4(c2) == -1) {
                tv.loopNode = -1;
            }
        }
    }
    for (const auto& b : nd->branches) {
        if (b.lo <= val && val <= b.hi) {
            return b.node;
        }
    }
    if (nd->branches.empty()) {
        return -1;
    }
    int best = 127;
    const Branch* pick = &nd->branches[0];
    for (const auto& b : nd->branches) {
        const int d = std::min(std::abs(b.lo - val), std::abs(b.hi - val));
        if (d < best) {
            best = d;
            pick = &b;
        }
    }
    return pick->node;
}

int Core::resolve(Voice& v, int prev, int n, int value, double t) {
    n = route(prev, n);
    for (int guard = 0; n >= 0 && n < int(g_.nodes.size()) && g_.nodes[size_t(n)].sample <= 0 && guard < 4096; ++guard) {
        const Node& nd = g_.nodes[size_t(n)];
        int val = value;
        if (nd.sample == 0) {
            (size_t(nd.track) < st_.voices.size() ? st_.voices[size_t(nd.track)] : v).group = n;
            if (nd.random) {
                val = clock_ ? int(std::floor(clock_(t) / 23)) & 0x7f : rand7();
            }
        } else if (nd.sample == -1) {
            Voice& tv = size_t(nd.track) < st_.voices.size() ? st_.voices[size_t(nd.track)] : v;
            if (nd.loopInit && tv.loopNode != n) {
                tv.loopNode = n;
                tv.loopCount = nd.loopInit;
            }
        } else if (nd.sample == -2) {
            val = rand7();
        } else if (nd.sample == -3) {
            postEvent(int((nd.w12 >> 2) & 0xfff), t);
        }
        n = route(prev, choose(v, n, val));
    }
    return n >= 0 && n < int(g_.nodes.size()) ? n : -1;
}

Voice* Core::master() {
    Voice* m = nullptr;
    for (auto& v : st_.voices) {
        if (v.active && v.node >= 0 && v.group >= 0 && !v.paused && (!m || v.latency > m->latency)) {
            m = &v;
        }
    }
    return m;
}

const Seg* Core::segAt(const Voice& v, double t) const {
    for (const auto& s : v.queue) {
        if (s.start - kEps <= t && t < s.end - kEps) {
            return &s;
        }
    }
    return nullptr;
}

std::optional<Core::Beat> Core::masterBeat(double t, bool strict) {
    Voice* m = master();
    if (!m) {
        return std::nullopt;
    }
    const Seg* seg = segAt(*m, t);
    if (!seg) {
        return std::nullopt;
    }
    const Node& nd = g_.nodes[size_t(seg->node)];
    const int per = nd.beats && nd.measures ? nd.beats * nd.measures : 1;
    const double beatMs = (seg->end - seg->start) / per;
    const int bpm = nd.beats ? nd.beats : 1;
    int k = strict ? int(std::floor((t - seg->start + kEps) / beatMs)) + 1 : int(std::ceil((t - seg->start - kEps) / beatMs));
    if (k < 0) {
        k = 0;
    }
    const double at = k >= per ? seg->end : seg->start + k * beatMs;
    return Beat{at, k, k % bpm, bpm, beatMs};
}

double Core::lastEnd(const Voice& v, double t) const {
    return !v.queue.empty() && v.queue.back().end > t ? v.queue.back().end : t;
}

Seg& Core::pushSeg(Voice& v, int n, double start, double dur, std::optional<int> slice, std::optional<int> bankIndex) {
    const Node& nd = g_.nodes[size_t(n)];
    const Sample& smp = g_.samples[size_t(nd.sample - 1)];
    Seg seg;
    seg.id = ++st_.seq;
    seg.voice = v.track;
    seg.node = n;
    seg.sample = nd.sample;
    seg.section = nd.section;
    seg.group = v.group;
    seg.start = start;
    seg.end = start + dur;
    seg.commit = st_.time;
    seg.kind = smp.kind;
    seg.scale = v.scale;
    seg.slice = slice;
    seg.bankIndex = bankIndex;
    if (!v.sync) {
        seg.beat0 = v.beats;
        v.beats += nd.beats && nd.measures ? nd.beats * nd.measures : 1;
    }
    v.queue.push_back(seg);
    if (segmentLog) {
        segmentLog->push_back(seg);
    }
    return v.queue.back();
}

void Core::stopVoice(Voice& v) {
    v.node = -1;
    v.active = false;
    v.group = -1;
    v.nextAt.reset();
    v.startAt.reset();
    if (v.sync) {
        v.slice = 0;
    }
}

void Core::cutVoice(Voice& v, double t) {
    v.queue.erase(std::remove_if(v.queue.begin(), v.queue.end(), [&](const Seg& s) { return !(s.start < t - kEps); }), v.queue.end());
    for (auto& s : v.queue) {
        if (s.end > t) {
            s.end = t;
        }
    }
    v.nextAt.reset();
    v.startAt.reset();
}

std::string Core::commit(Voice& v, int n, double t) {
    v.node = n;
    if (n < 0) {
        stopVoice(v);
        return "stopped";
    }
    const Node& nd = g_.nodes[size_t(n)];
    if (nd.sample <= 0) {
        stopVoice(v);
        return "stopped";
    }
    v.group = nd.group;
    v.active = true;
    st_.started = true;
    const Sample& smp = g_.samples[size_t(nd.sample - 1)];
    if (!v.sync) {
        pushSeg(v, n, lastEnd(v, t), smp.durMs);
        v.nextAt.reset();
        return "queued";
    }
    // the beat-synced slave voice (voice+0x2F)
    bool fresh = false;
    const Node* gn = node(nd.group);
    const int gsync = gn ? gn->sync : 0;
    if (v.slice <= 0) {
        const auto mb = masterBeat(t);
        int sl;
        if (!mb) {
            sl = gsync == 0 ? 1 : 0;
        } else if (gsync == 0x100 || gsync == 0x200) {
            sl = mb->inMeasure + 1;
        } else {
            sl = 1;
        }
        v.slice = (sl & 0x1f) > 0 ? sl & 0x1f : 1;
        fresh = true;
    }
    Voice* m = master();
    const int bankIndex = (smp.bankIndex ? *smp.bankIndex : smp.value) + v.slice - 1;
    if (!m || m == &v || !gsync) {
        pushSeg(v, n, lastEnd(v, t), smp.durMs, v.slice, bankIndex);
        v.nextAt.reset();
        return "queued";
    }
    if (v.startAt) {
        if (*v.startAt > t + kEps) {
            return "defer";
        }
        v.startAt.reset();
    } else if (fresh && (gsync == 0x100 || gsync == 0x300)) {
        const auto mb = masterBeat(t);
        if (mb && mb->at > t + kEps) {
            v.startAt = mb->at;
            v.nextAt.reset();
            return "defer";
        }
    }
    const int per = nd.beats && nd.measures ? nd.beats * nd.measures : 1;
    pushSeg(v, n, t, smp.durMs / per, v.slice, bankIndex);
    const int old = v.slice;
    v.slice = old < per && nd.sliced ? old + 1 : 0;
    const auto nb = masterBeat(t, true);
    v.nextAt = nb ? nb->at : t + smp.durMs / per;
    return "queued";
}

std::string Core::advance(Voice& v, double t) {
    const int cur = v.node;
    const Node* cn = node(cur);
    const int per = cn && cn->beats && cn->measures ? cn->beats * cn->measures : 1;
    if (v.sync && v.slice > 0 && cn && cn->sliced && v.slice <= per) {
        return commit(v, cur, t);
    }
    int next = cur >= 0 ? choose(v, cur, v.intensity) : -1;
    if (next >= 0) {
        next = resolve(v, cur, next, v.intensity, t);
    }
    const Node* nn = node(next);
    if (nn && nn->track != v.track && size_t(nn->track) < st_.voices.size()) {
        stopVoice(v);
        return commit(st_.voices[size_t(nn->track)], next, t);
    }
    return commit(v, next, t);
}

double Core::voiceDue(const Voice& v) const {
    if (v.pendingJump || !v.active || v.node < 0 || v.paused) {
        return INFINITY;
    }
    if (v.startAt) {
        return std::max(*v.startAt, st_.time);
    }
    if (v.nextAt) {
        return std::max(*v.nextAt, st_.time);
    }
    std::vector<const Seg*> live;
    for (const auto& s : v.queue) {
        if (s.end > st_.time + kEps) {
            live.push_back(&s);
        }
    }
    if (live.empty()) {
        return st_.time;
    }
    double d = live.back()->end - v.latency;
    if (int(live.size()) >= v.capacity) {
        d = std::max(d, live[live.size() - size_t(v.capacity)]->end);
    }
    return std::max(d, st_.time);
}

bool Core::postEvent(int e, double t) {
    const int k = e & 0xff;
    if (k >= int(g_.events.size())) {
        return false;
    }
    if (st_.events.size() >= kMaxEvents) {
        return false;
    }
    auto inst = std::make_shared<EventInst>();
    inst->e = k;
    inst->post = t;
    inst->last = t;
    for (const auto& a : g_.events[size_t(k)].actions) {
        inst->actions.push_back({a, double(a.timeout), false});
    }
    st_.events.push_back(inst);
    return true;
}

bool Core::runOp(EventInst& inst, EventInst::A& ea, Voice& v, double t) {
    const Action& a = ea.a;
    switch (a.op) {
    case 0x02:
        if (t - inst.last >= double(a.word) - 1e-6) {
            return true;
        }
        inst.wake = inst.last + a.word;
        return false;
    case 0x04: {
        int target = a.value;
        int sec = a.arg;
        if (target >= -31 && target <= -16) {
            target = s16(st_.regs[size_t((-target) & 0xf)]);
        }
        if (sec <= -16 && target >= -31) {
            sec = s16(st_.regs[size_t((-sec) & 0xf)]);
        }
        if (target < 0 || target > int(g_.nodes.size())) {
            target = -1;
        } else if (sec == -32) {
            sec = sectionOf();
        }
        if (sec >= 0) {
            int k = target;
            int found = -1;
            for (size_t i = 0; i < g_.nodes.size(); ++i) {
                if (g_.nodes[i].section != sec || g_.nodes[i].sample != 0) {
                    continue;
                }
                if (--k == 0) {
                    found = int(i);
                    break;
                }
            }
            target = found;
        }
        if (a.flags & 1) {
            cutVoice(v, t);
        }
        const int cur = v.node;
        const std::string r = commit(v, resolve(v, cur, target, v.intensity, t), t);
        if (r == "defer") {
            inst.wake = v.startAt;
            v.pendingJump = true;
            return false;
        }
        v.pendingJump = false;
        return true;
    }
    case 0x05:
        v.paused = (a.word & 1) != 0;
        return true;
    case 0x06: {
        const double from = v.scale;
        v.scale = a.b12;
        v.ramps.push_back({t, from, double(a.b12), double(a.u16_14), a.b13 & 0x7f});
        return true;
    }
    case 0x09:
        v.scale = a.b12;
        return true;
    case 0x0a:
        if (a.argU < 16) {
            int val = a.value;
            if (val == -33) {
                val = s16(v.node & 0xffff);
            } else if (val == -32) {
                val = sectionOf();
            }
            st_.regs[size_t(a.argU)] = val;
        }
        return true;
    case 0x0c:
        postEvent(a.b12 & 0xff, t);
        return true;
    case 0x0d:
    case 0x0e:
    case 0x0f: {
        EventInst* self = &inst;
        if (a.b12 == -1) {
            st_.events.erase(std::remove_if(st_.events.begin(), st_.events.end(), [&](const auto& x) { return x.get() != self; }), st_.events.end());
        } else {
            st_.events.erase(std::remove_if(st_.events.begin(), st_.events.end(), [&](const auto& x) { return x.get() != self && x->e == (a.b12 & 0xff); }),
                             st_.events.end());
        }
        return true;
    }
    default:
        return true;
    }
}

void Core::processEvents(double t) {
    const auto list = st_.events;
    for (const auto& instP : list) {
        if (std::find(st_.events.begin(), st_.events.end(), instP) == st_.events.end()) {
            continue;
        }
        EventInst& inst = *instP;
        inst.wake.reset();
        while (inst.idx < inst.actions.size()) {
            EventInst::A& a = inst.actions[inst.idx];
            if (a.deadline != 0) {
                if (a.deadline < inst.post) {
                    a.deadline += t;
                }
                if (a.deadline < t) {
                    a.done = true;
                }
            }
            if (!a.done) {
                bool complete = false;
                bool any = false;
                for (int tr = 0; tr < 24; ++tr) {
                    if (!(a.a.mask & (1u << tr))) {
                        continue;
                    }
                    if (size_t(tr) >= st_.voices.size()) {
                        continue;
                    }
                    any = true;
                    if (runOp(inst, a, st_.voices[size_t(tr)], t)) {
                        complete = true;
                    }
                }
                if (!any) {
                    inst.wake.reset();
                    break;
                }
                if (!complete) {
                    break;
                }
                a.done = true;
            }
            inst.idx++;
            inst.last = t;
        }
        if (inst.idx >= inst.actions.size()) {
            st_.events.erase(std::remove(st_.events.begin(), st_.events.end(), instP), st_.events.end());
        }
    }
}

int Core::sectionOf() {
    Voice* m = master();
    return m && m->group >= 0 ? g_.nodes[size_t(m->group)].section : 0;
}

double Core::nextDue() {
    double d = INFINITY;
    for (const auto& v : st_.voices) {
        d = std::min(d, voiceDue(v));
    }
    for (const auto& inst : st_.events) {
        if (inst->wake) {
            d = std::min(d, std::max(*inst->wake, st_.time));
        }
    }
    return d;
}

void Core::advanceTo(double T) {
    for (int guard = 0; guard < 100000; ++guard) {
        const double d = nextDue();
        if (!(d <= T)) {
            break;
        }
        st_.time = std::max(st_.time, d);
        const double t = st_.time;
        bool wake = false;
        for (const auto& i : st_.events) {
            wake = wake || (i->wake && *i->wake <= t + 1e-6);
        }
        if (wake) {
            processEvents(t);
        }
        for (size_t k = 0; k < st_.voices.size(); ++k) {
            Voice& v = st_.voices[k];
            if (voiceDue(v) <= t + 1e-6) {
                v.nextAt.reset();
                advance(v, t);
            }
        }
    }
    st_.time = std::max(st_.time, T);
    for (auto& v : st_.voices) {
        if (v.queue.size() > 24) {
            v.queue.erase(std::remove_if(v.queue.begin(), v.queue.end(), [&](const Seg& s) { return !(s.end > st_.time - 10000); }), v.queue.end());
        }
    }
}

bool Core::sendEvent(int e, std::optional<double> t) {
    const double at = t ? *t : st_.time;
    advanceTo(at);
    const bool ok = postEvent(e, at);
    processEvents(at);
    return ok;
}

void Core::setIntensity(int value, std::optional<double> t) {
    advanceTo(t ? *t : st_.time);
    if (!(value >= 0 && value < 128)) {
        return;
    }
    for (auto& v : st_.voices) {
        v.prevIntensity = v.intensity;
        v.intensity = value;
    }
}

bool Core::finished(std::optional<double> t) {
    const double at = t ? *t : st_.time;
    const Voice& v = st_.voices[0];
    if (!st_.started || v.active) {
        return false;
    }
    for (const auto& s : v.queue) {
        if (s.end > at + kEps) {
            return false;
        }
    }
    return true;
}

const Seg* Core::playing(int track, std::optional<double> t) {
    Voice* v = voice(track);
    return v ? segAt(*v, t ? *t : st_.time) : nullptr;
}

void Core::trimRng() {
    const int64_t drop = int64_t(st_.rngIdx) - int64_t(rngBase_) - 64;
    if (drop > 256) {
        rngLog_.erase(rngLog_.begin(), rngLog_.begin() + drop);
        rngBase_ += uint64_t(drop);
    }
}

// ---- the player ----

Player::Player(PlayerOptions o)
    : o_(o), ctx_(o.context), song_(o.song),
      core_(song_->graph, o.random ? o.random : [] { return 0.0; }, o.intensity,
            o.clockRandom ? std::function<double(double)>([this](double t) { return clockMs_ + t; }) : nullptr) {
    dk_ = std::max(0.0, o.declickMs) / 1000;
    pathLevelPct_ = song_->inf.value("PathLevel", 100.0);
    out_ = ctx_->createGain();
    out_->connect(o.destination);
    byteLoops_ = o.loopDestination && !o.ambience;
    if (byteLoops_) {
        loopOut_ = ctx_->createGain();
        loopOut_->connect(o.loopDestination);
    }
    for (size_t i = 0; i < core_.state().voices.size(); ++i) {
        Chain c{ctx_->createGain(), ctx_->createGain(), 0};
        c.ramp->connect(c.level);
        c.level->connect(i == 1 && loopOut_ ? loopOut_ : out_);
        chains_.push_back(c);
    }
}

Player::~Player() {
    if (timer_ && o_.timers) {
        o_.timers->clear(*timer_);
    }
}

double Player::songNow() const {
    return paused_ ? pausedAt_ : std::max(0.0, (ctx_->currentTime() - base_) * 1000);
}

std::shared_ptr<const wa::Buffer> Player::buffer(const Seg& seg, double now) {
    (void)now;
    const std::string key = seg.kind == "stream" ? "s" + std::to_string(seg.sample) : "b" + std::to_string(seg.bankIndex ? *seg.bankIndex : -1);
    if (auto* b = cache_.get(key)) {
        auto buf = *b;
        cache_.erase(key);
        cache_.set(key, buf);
        return buf;
    }
    const auto dec = song_->decodeSegment(seg.sample, seg.bankIndex);
    if (!dec || !dec->length) {
        return nullptr;
    }
    auto buf = std::make_shared<wa::Buffer>();
    buf->sampleRate = float(dec->sampleRate);
    buf->length = std::max<size_t>(1, dec->length);
    if (dec->channels == 6) {
        // [[0, 2, 4], [1, 3, 5]]: the stems summed into stereo
        for (int c = 0; c < 2; ++c) {
            std::vector<float> mix(buf->length, 0.f);
            for (int src : {c, c + 2, c + 4}) {
                const auto& d = dec->data[size_t(src)];
                for (size_t i = 0; i < dec->length; ++i) {
                    mix[i] += d[i];
                }
            }
            buf->channels.push_back(std::move(mix));
        }
    } else {
        for (const auto& d : dec->data) {
            std::vector<float> v = d;
            v.resize(buf->length, 0.f);
            buf->channels.push_back(std::move(v));
        }
    }
    for (auto& ch : buf->channels) {
        for (float& x : ch) {
            if (!std::isfinite(x)) {
                x = 0;
            }
        }
    }
    cache_.set(key, buf);
    while (cache_.size() > size_t(o_.cacheSize)) {
        cache_.erase(cache_.items().front().first);
    }
    return buf;
}

double Player::sliceGain(const Seg& seg) const {
    return byteLoops_ ? std::max(0.0, std::trunc(loopsPct_ * 0.01 * seg.scale)) / 127 : (loopsPct_ / 100) * std::max(0, seg.scale) / 127;
}

void Player::applyLevels() {
    const double t = ctx_->currentTime();
    const double main = std::min(std::max(level_ * pathLevelPct_ / 100, 0.0), 127.0) / 127;
    auto put = [&](wa::Param& g, double v) {
        if (!dk_ || !started_) {
            g.setValueAtTime(v, t);
            return;
        }
        g.cancelAndHoldAtTime(t);
        g.linearRampToValueAtTime(v, t + dk_);
    };
    put(chains_[0].level->gain, main);
    if (chains_.size() < 2) {
        return;
    }
    put(chains_[1].level->gain, o_.ambience ? loopsPct_ / 100 : 1);
    if (o_.ambience) {
        return;
    }
    const double now = songNow();
    for (auto& [id, rec] : scheduled_) {
        if (rec.gain && rec.seg.start > now) {
            rec.gain->gain.setValueAtTime(sliceGain(rec.seg), t);
        }
    }
}

void Player::applyRamps() {
    if (!o_.ambience) {
        return;
    }
    auto& voices = core_.state().voices;
    for (size_t i = 0; i < voices.size() && i < chains_.size(); ++i) {
        auto& v = voices[i];
        auto& ch = chains_[i];
        if (v.ramps.empty()) {
            ch.rampsApplied = 0;
            continue;
        }
        if (v.ramps.size() == ch.rampsApplied) {
            continue;
        }
        ch.rampsApplied = v.ramps.size();
        const Ramp& r = v.ramps.back();
        auto& g = ch.ramp->gain;
        const double t0 = std::max(ctx_->currentTime(), ctxTime(r.time));
        g.cancelScheduledValues(t0);
        if (!(r.durMs > 0) || r.curve == 0) {
            g.setValueAtTime(r.to / 127, ctxTime(r.time + r.durMs));
            continue;
        }
        const int n = 64;
        std::vector<float> curve(n);
        for (int k = 0; k < n; ++k) {
            curve[size_t(k)] = float(std::max(0.0, rampLevel(r.from, r.to, double(k) / (n - 1), r.curve) / 127));
        }
        try {
            g.setValueCurveAtTime(curve, t0, r.durMs / 1000);
        } catch (...) {
            g.setValueAtTime(r.to / 127, t0);
        }
    }
}

std::optional<Player::Rec> Player::startSource(const Seg& seg, double now, bool shift) {
    auto buf = buffer(seg, now);
    if (!buf) {
        return std::nullopt;
    }
    Rec rec;
    rec.src = ctx_->createBufferSource();
    rec.src->buffer = buf;
    std::shared_ptr<wa::Node> head = rec.src;
    if (dk_ > 0) {
        rec.fade = ctx_->createGain();
        rec.src->connect(rec.fade);
        head = rec.fade;
    }
    if (seg.voice == 1 && !o_.ambience) {
        rec.gain = ctx_->createGain();
        rec.gain->gain.setValue(sliceGain(seg));
        head->connect(rec.gain);
        rec.gain->connect(chains_[1].ramp);
    } else {
        head->connect(chains_[size_t(seg.voice)].ramp);
    }
    const double when = ctxTime(seg.start);
    const double t = ctx_->currentTime();
    double at = when;
    const bool firstAfterResume = std::find(resumed_.begin(), resumed_.end(), seg.voice) == resumed_.end();
    if (when >= t) {
        rec.src->start(when);
        if (rec.fade && resuming_ && firstAfterResume && when - t < 0.05) {
            rec.fade->gain.setValueAtTime(0, when);
            rec.fade->gain.linearRampToValueAtTime(1, when + dk_);
        }
        if (resuming_) {
            resumed_.push_back(seg.voice);
        }
    } else if (shift && seg.kind == "bank" && t - when <= kShiftMaxS) {
        rec.src->start(t);
        at = t;
    } else {
        const double off = t - when;
        if (off >= buf->duration()) {
            letGo(rec);
            return std::nullopt;
        }
        rec.src->start(t, off);
        at = t;
        if (resuming_) {
            resumed_.push_back(seg.voice);
        }
        if (rec.fade) {
            rec.fade->gain.setValueAtTime(0, t);
            rec.fade->gain.linearRampToValueAtTime(1, t + dk_);
        }
    }
    rec.at = at;
    rec.seg = seg;
    auto src = rec.src;
    auto fade = rec.fade;
    auto gain = rec.gain;
    rec.src->onended = [src, fade, gain] {
        src->disconnect();
        if (fade) {
            fade->disconnect();
        }
        if (gain) {
            gain->disconnect();
        }
    };
    if (seg.kind == "stream" && seg.end < seg.start + buf->duration() * 1000 - 1) {
        rec.stopAt = seg.end;
        rec.src->stop(ctxTime(seg.end));
    }
    return rec;
}

bool Player::halt(Rec& rec, double at) {
    const double t = ctx_->currentTime();
    at = std::max(t, at);
    if (!rec.fade || !(rec.at < at - 1e-4)) {
        try {
            rec.src->stop(at);
        } catch (...) {
        }
        return false;
    }
    const double r0 = std::max(t, at - dk_);
    auto& g = rec.fade->gain;
    g.cancelAndHoldAtTime(r0);
    g.linearRampToValueAtTime(0, r0 + dk_);
    try {
        rec.src->stop(r0 + dk_);
    } catch (...) {
    }
    return true;
}

void Player::kill(Rec& rec, double atMs) {
    const bool faded = halt(rec, ctxTime(atMs));
    if (!faded && atMs <= songNow() + 1) {
        letGo(rec);
    }
}

void Player::letGo(Rec& rec) {
    for (const std::shared_ptr<wa::Node>& n : std::vector<std::shared_ptr<wa::Node>>{rec.src, rec.fade, rec.gain}) {
        if (n) {
            n->disconnect();
        }
    }
}

void Player::reconcile(double now) {
    OrderedMap<uint64_t, Seg> want;
    for (const auto& v : core_.state().voices) {
        for (const auto& s : v.queue) {
            if (s.end > now + kEps) {
                want.set(s.id, s);
            }
        }
    }
    std::vector<uint64_t> drop;
    for (auto& [id, rec] : scheduled_) {
        const Seg* s = want.get(id);
        if (!s && rec.seg.end <= now + kEps) {
            drop.push_back(id);
            continue;
        }
        if (!s || s->start != rec.seg.start || s->bankIndex != rec.seg.bankIndex || s->sample != rec.seg.sample) {
            kill(rec, std::max(now, rec.seg.start));
            drop.push_back(id);
            continue;
        }
        if (s->end < rec.seg.end - kEps && (!rec.stopAt || s->end < *rec.stopAt)) {
            halt(rec, ctxTime(s->end));
            rec.stopAt = s->end;
            rec.seg.end = s->end;
        }
    }
    for (uint64_t id : drop) {
        scheduled_.erase(id);
    }
    std::vector<uint64_t> gone;
    for (auto& [id, s] : waiting_) {
        if (!want.has(id)) {
            gone.push_back(id);
        }
    }
    for (uint64_t id : gone) {
        waiting_.erase(id);
    }
    for (auto& [id, s] : want) {
        if (scheduled_.has(id)) {
            continue;
        }
        if (s.voice == 1 && !core_.state().armed) {
            continue;
        }
        auto rec = startSource(s, now);
        if (rec) {
            scheduled_.set(id, *rec);
            waiting_.erase(id);
        } else {
            waiting_.set(id, s);
        }
    }
    std::vector<uint64_t> old;
    for (auto& [id, rec] : scheduled_) {
        if (rec.seg.end < now - 2000) {
            old.push_back(id);
        }
    }
    for (uint64_t id : old) {
        scheduled_.erase(id);
    }
}

void Player::startDue(double now) {
    for (const auto& v : core_.state().voices) {
        for (const auto& s : v.queue) {
            if (s.start > now + kEps || s.end <= now + kEps || (s.voice == 1 && !core_.state().armed)) {
                continue;
            }
            if (Rec* old = scheduled_.get(s.id)) {
                if (old->seg.start == s.start && old->seg.sample == s.sample && old->seg.bankIndex == s.bankIndex) {
                    continue;
                }
                kill(*old, std::max(now, old->seg.start));
                scheduled_.erase(s.id);
            }
            auto rec = startSource(s, now, true);
            if (rec) {
                scheduled_.set(s.id, *rec);
                waiting_.erase(s.id);
            }
        }
    }
}

void Player::simulate(double now, const std::function<void(double)>& input) {
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    if (snap_) {
        core_.restore(*snap_);
    }
    core_.advanceTo(now);
    if (input) {
        input(now);
        if (o_.eventFirst && !paused_) {
            startDue(now);
        }
    }
    snap_ = core_.snapshot();
    core_.trimRng();
    core_.advanceTo(now + o_.lookahead * 1000);
    if (paused_) {
        return;
    }
    reconcile(now);
    applyRamps();
}

void Player::schedulePump() {
    if (!o_.timers || stopped_) {
        return;
    }
    std::weak_ptr<Player> w = weak_from_this();
    timer_ = o_.timers->post(o_.interval * 1000, [w] {
        if (auto p = w.lock()) {
            p->pump();
            p->schedulePump();
        }
    });
}

void Player::pump() {
    if (!started_ || paused_ || stopped_) {
        return;
    }
    simulate(songNow());
}

bool Player::finished() {
    return started_ && core_.finished(songNow());
}

int Player::intensity() {
    Voice* v = core_.voice(0);
    return v ? v->intensity : 0;
}

void Player::start(int event, std::optional<double> when) {
    if (started_ || stopped_) {
        return;
    }
    started_ = true;
    base_ = when ? *when : ctx_->currentTime();
    clockMs_ = std::floor(base_ * 1000);
    applyLevels();
    setLowpass(song_->inf.value("LOWPASS", double(0xffff)));
    snap_.reset();
    simulate(songNow(), [this, event](double t) {
        core_.sendEvent(event, t);
        if (pending_) {
            core_.sendEvent(*pending_, t);
            pending_.reset();
        }
    });
    schedulePump();
}

bool Player::send(int e) {
    if (!started_) {
        pending_ = e;
        return true;
    }
    if (stopped_) {
        return false;
    }
    simulate(songNow(), [this, e](double t) { core_.sendEvent(e, t); });
    return true;
}

bool Player::event(int e) {
    if (e == lastEvent_) {
        return false;
    }
    lastEvent_ = e;
    if (!started_) {
        pending_ = e;
        return true;
    }
    if (stopped_) {
        return false;
    }
    simulate(songNow(), [this, e](double t) { core_.sendEvent(e, t); });
    return true;
}

void Player::setIntensity(int v) {
    v = std::max(0, std::min(127, v));
    if (v == intensity()) {
        return;
    }
    if (!started_) {
        core_.setIntensity(v, 0);
        return;
    }
    simulate(songNow(), [this, v](double t) { core_.setIntensity(v, t); });
}

void Player::pause() {
    if (!started_ || paused_ || stopped_) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    pausedAt_ = songNow();
    paused_ = true;
    for (auto& [id, rec] : scheduled_) {
        kill(rec, pausedAt_);
    }
    scheduled_.clear();
}

void Player::resume() {
    if (!paused_ || stopped_) {
        return;
    }
    base_ = ctx_->currentTime() - pausedAt_ / 1000;
    paused_ = false;
    clockMs_ = std::floor(base_ * 1000);
    for (auto& ch : chains_) {
        ch.rampsApplied = 0;
    }
    resuming_ = true;
    resumed_.clear();
    pump();
    resuming_ = false;
}

void Player::fadeOut(double seconds) {
    if (stopped_) {
        return;
    }
    fading_ = true;
    const double t = ctx_->currentTime();
    auto& g = out_->gain;
    g.cancelScheduledValues(t);
    g.setValueAtTime(g.value(), t);
    g.linearRampToValueAtTime(0, t + std::max(0.0, seconds));
    if (o_.timers) {
        auto self = shared_from_this();
        o_.timers->post(std::max(0.0, seconds) * 1000 + 50, [self] { self->stop(); });
    }
}

void Player::stop() {
    if (stopped_) {
        return;
    }
    stopped_ = true;
    if (timer_ && o_.timers) {
        o_.timers->clear(*timer_);
    }
    std::lock_guard<std::recursive_mutex> lock(ctx_->mutex());
    if (dk_ > 0) {
        const double t = ctx_->currentTime();
        for (auto* g : {&out_->gain, loopOut_ ? &loopOut_->gain : nullptr}) {
            if (!g) {
                continue;
            }
            g->cancelAndHoldAtTime(t);
            g->linearRampToValueAtTime(0, t + dk_);
        }
        std::vector<Rec> recs;
        for (auto& [id, rec] : scheduled_) {
            try {
                rec.src->stop(t + dk_);
            } catch (...) {
            }
            recs.push_back(rec);
        }
        scheduled_.clear();
        if (o_.timers) {
            auto self = shared_from_this();
            o_.timers->post(dk_ * 1000 + 60, [self, recs]() mutable {
                self->out_->disconnect();
                if (self->loopOut_) {
                    self->loopOut_->disconnect();
                }
                for (auto& r : recs) {
                    letGo(r);
                }
                for (auto& ch : self->chains_) {
                    ch.ramp->disconnect();
                    ch.level->disconnect();
                }
                if (self->lowpass_) {
                    self->lowpass_->disconnect();
                }
            });
        }
        return;
    }
    for (auto& [id, rec] : scheduled_) {
        try {
            rec.src->stop();
        } catch (...) {
        }
        letGo(rec);
    }
    scheduled_.clear();
    out_->disconnect();
    if (loopOut_) {
        loopOut_->disconnect();
    }
    for (auto& ch : chains_) {
        ch.ramp->disconnect();
        ch.level->disconnect();
    }
    if (lowpass_) {
        lowpass_->disconnect();
    }
}

void Player::setLevel(double v) {
    level_ = std::max(0.0, std::min(127.0, std::isfinite(v) ? v : 0));
    if (!fading_) {
        applyLevels();
    }
}

void Player::setLowpass(double value) {
    lowpassValue_ = value;
    const bool on = value < kMixerRate / 2;
    chains_[0].level->disconnect();
    if (on) {
        if (!lowpass_) {
            lowpass_ = ctx_->createBiquadFilter();
        }
        lowpass_->frequency.setValue(std::max(10.0, value));
        lowpass_->Q.setValue(M_SQRT1_2);
        chains_[0].level->connect(lowpass_);
        lowpass_->connect(out_);
    } else {
        if (lowpass_) {
            lowpass_->disconnect();
        }
        chains_[0].level->connect(out_);
    }
}

void Player::loopsStart() {
    requested_ = true;
    if (!core_.state().armed) {
        core_.setArmed(true);
        if (snap_) {
            snap_->armed = true;
        }
        pump();
    }
}

void Player::loopsSetLevel(double v) {
    loopsPct_ = std::max(0.0, std::min(100.0, std::isfinite(v) ? v : 0));
    applyLevels();
}

void Player::warmEvents(const std::vector<int>& list) {
    const Graph& g = song_->graph;
    for (int e : list) {
        if (e < 0 || size_t(e) >= g.events.size()) {
            continue;
        }
        for (const auto& a : g.events[size_t(e)].actions) {
            if (a.op != 0x04 || !(a.mask & 3)) {
                continue;
            }
            int target = a.value;
            const int sec = a.arg;
            if (target < 0 || target > int(g.nodes.size())) {
                continue;
            }
            if (sec >= 0) {
                int k = target;
                int found = -1;
                for (size_t i = 0; i < g.nodes.size(); ++i) {
                    if (g.nodes[i].section != sec || g.nodes[i].sample != 0) {
                        continue;
                    }
                    if (--k == 0) {
                        found = int(i);
                        break;
                    }
                }
                target = found;
            }
            if (!(target >= 0) || size_t(target) >= g.nodes.size()) {
                continue;
            }
            const int s = g.nodes[size_t(target)].sample;
            if (s > 0) {
                const Sample& smp = g.samples[size_t(s - 1)];
                if (smp.kind == "bank") {
                    Seg seg;
                    seg.kind = "bank";
                    seg.voice = 1;
                    seg.sample = s;
                    seg.bankIndex = smp.bankIndex ? *smp.bankIndex : smp.value;
                    buffer(seg, 0);
                }
            }
        }
    }
}

}  // namespace pf

}  // namespace audio

}  // namespace ssx_native
