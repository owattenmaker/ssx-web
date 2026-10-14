#pragma once
// The EA speech-event interpreter (web/audio-speech-events.js, 1:1; EE 0x3D6498..0x3DB9D8, docs/audio-logic.md section 4):
// Events.evt, the bank registry (speech/registry.json), the EA RNG (0x3DB4D0), the no-repeat ring and line histories, event
// resolution (0x3D9FB0), the 16-slot pending table and dispatcher, and the game's 10-slot request table (0x2B1458..).
// JS numbers become uint32 / int32 exactly where the JS's `>>> 0` / `|` / `>>` convert.
#include "audio_common.hpp"
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ssx_native {

namespace audio {

namespace speech {

inline uint32_t u16(const std::vector<uint8_t>& b, size_t o) {
    return uint32_t(b[o]) | (uint32_t(b[o + 1]) << 8);
}
inline uint32_t u32(const std::vector<uint8_t>& b, size_t o) {
    return uint32_t(b[o]) | (uint32_t(b[o + 1]) << 8) | (uint32_t(b[o + 2]) << 16) | (uint32_t(b[o + 3]) << 24);
}
inline int s8(const std::vector<uint8_t>& b, size_t o) {
    return int(int8_t(b[o]));
}
inline size_t pad4(int64_t x) {
    return size_t((x + 3) & ~int64_t(3));
}

struct Item {
    uint32_t bankId = 0;
    int argIdx = 0;
    int type = 0;
    int nf = 0;
    std::array<int, 4> param{};
    std::array<int, 4> state{};
    std::vector<uint32_t> fmask;
};
struct Record {
    int index = 0;
    int weight = 0;
    int chance = 0;
    int extMask = 0;
    int extVal = 0;
    int mode = 0;
    int nState = 0;
    std::vector<uint32_t> condMasks;
    std::vector<Item> items;
};
struct Event {
    uint32_t id = 0;
    int W = 0;
    int P = 0;
    int maxRepeat = 0;
    int chance = 0;
    int flags = 0;
    int index = 0;
    struct Cond {
        int b0, arg, b2;
    };
    std::vector<int> ext;
    std::vector<Cond> cond;
    std::vector<Record> records;
    std::optional<std::vector<uint32_t>> follow;
    struct Presence {
        uint32_t codes = 0;
        std::vector<uint32_t> req;
    };
    std::optional<Presence> presence;
};
struct EventFile {
    std::map<uint32_t, Event> events;
};

inline Item parseItem(const std::vector<uint8_t>& b, size_t io) {
    Item it;
    it.nf = s8(b, io + 4);
    it.bankId = u16(b, io);
    it.argIdx = b[io + 2];
    it.type = b[io + 3];
    for (int j = 0; j < 4; ++j) {
        it.param[size_t(j)] = b[io + 8 + size_t(j)];
        it.state[size_t(j)] = b[io + 16 + size_t(j)];
    }
    for (int j = 0; j < std::max(it.nf, 0); ++j) {
        it.fmask.push_back(u32(b, io + 20 + 4 * size_t(j)));
    }
    return it;
}

inline Record parseRecord(const std::vector<uint8_t>& b, size_t ro, int index, int nCond) {
    const int wb = b[ro];
    const int nItems = b[ro + 4] >> 2;
    const int nMasks = b[ro + 6];
    Record r;
    r.index = index;
    r.weight = (wb & 31) * (1 << (2 * (wb >> 5)));
    r.chance = b[ro + 1];
    r.extMask = b[ro + 2];
    r.extVal = b[ro + 3];
    r.mode = b[ro + 4] & 3;
    r.nState = b[ro + 5];
    (void)nMasks;
    const size_t mo = ro + 8 + pad4(nItems);
    for (int k = 0; k < nCond; ++k) {
        r.condMasks.push_back(u32(b, mo + 4 * size_t(k)));
    }
    for (int k = 0; k < nItems; ++k) {
        r.items.push_back(parseItem(b, ro + 4 * size_t(b[ro + 8 + size_t(k)])));
    }
    return r;
}

inline EventFile parseEvents(const std::vector<uint8_t>& b) {
    if (u16(b, 0) != 0x0c03) {
        throw std::runtime_error("Events.evt: bad magic");
    }
    EventFile file;
    const int count = int(u16(b, 0x10));
    for (int i = 0; i < count; ++i) {
        const size_t e = size_t(u16(b, 0x18 + 2 * size_t(i))) * 4;
        Event ev;
        const int nrec = b[e + 6];
        const int nExt = b[e + 7] & 15;
        const int nCond = s8(b, e + 7) >> 4;
        ev.id = u16(b, e);
        ev.W = int(u16(b, e + 2));
        ev.P = int(u16(b, e + 4));
        ev.maxRepeat = s8(b, e + 8);
        ev.chance = s8(b, e + 9);
        ev.flags = b[e + 10];
        ev.index = i;
        std::vector<size_t> recOffs;
        for (int r = 0; r < nrec; ++r) {
            recOffs.push_back(size_t(u16(b, e + 12 + 2 * size_t(r))) * 4);
        }
        size_t o = e + 12 + pad4(2 * nrec);
        for (int k = 0; k < nExt; ++k) {
            ev.ext.push_back(b[o + 3 * size_t(k)]);
        }
        o += pad4(3 * nExt);
        for (int k = 0; k < std::max(nCond, 0); ++k) {
            ev.cond.push_back({b[o + 3 * size_t(k)], b[o + 3 * size_t(k) + 1], b[o + 3 * size_t(k) + 2]});
        }
        o = size_t(int64_t(o) + int64_t(pad4(3 * nCond)));
        if (ev.flags & 0x08) {
            Event::Presence p;
            p.codes = u32(b, o);
            for (int r = 0; r < nrec; ++r) {
                p.req.push_back(u16(b, o + 4 + 2 * size_t(r)));
            }
            ev.presence = p;
            o += size_t((2 * nrec + 7) & ~3);
        }
        if (ev.flags & 0x10) {
            const int c = b[o];
            std::vector<uint32_t> f;
            for (int k = 0; k < c; ++k) {
                f.push_back(u16(b, o + 2 + 2 * size_t(k)));
            }
            ev.follow = f;
        }
        for (int r = 0; r < nrec; ++r) {
            ev.records.push_back(parseRecord(b, e + recOffs[size_t(r)], r, std::max(nCond, 0)));
        }
        file.events[ev.id] = ev;
    }
    return file;
}

struct Hdr {
    std::string name;
    uint32_t id = 0;
    uint32_t sub = 0xffff;
    int F = 0, N = 0, H = 0;
    std::vector<std::vector<int>> keys;
    std::vector<uint32_t> masks;
    int regIndex = -1;
    int variants = 0;
};

// speech/registry.json entries ({name, id, F, N, H, masks, keys}), first id wins; the registry index is the no-repeat key
inline std::vector<Hdr> makeBankTable(const Json& registry) {
    std::vector<Hdr> out;
    int i = 0;
    for (const auto& r : registry["banks"]) {
        Hdr h;
        h.name = r.value("name", "");
        h.id = r["id"].get<uint32_t>();
        h.F = r["F"].get<int>();
        h.N = r["N"].get<int>();
        h.H = r["H"].get<int>();
        for (const auto& m : r["masks"]) {
            h.masks.push_back(m.get<uint32_t>());
        }
        for (const auto& k : r["keys"]) {
            std::vector<int> v;
            for (const auto& x : k) {
                v.push_back(x.get<int>());
            }
            h.keys.push_back(v);
        }
        bool dup = false;
        for (const auto& x : out) {
            dup = dup || x.id == h.id;
        }
        if (!dup) {
            h.regIndex = i;
            out.push_back(h);
        }
        ++i;
    }
    return out;
}

// 0x3DB4D0, seeded like 0x3DB790
class EaRng {
public:
    explicit EaRng(uint32_t seed = 0) {
        reseed(seed);
    }
    void reseed(uint32_t seed) {
        const uint32_t x = seed + (seed << 16);
        s_[0] = x + 0xf22d0e56u;
        s_[1] = s_[0] + 0x96041893u;
        s_[2] = s_[1] + 0x3df3b646u;
        s_[3] = s_[2] + 0x40dde76du;
        s_[4] = s_[3] + 0x97327ae1u;
        s_[5] = s_[4] + 0xd1a9fbe7u;
    }
    uint32_t next() {
        auto& s = s_;
        uint32_t a = s[5] + s[4];
        uint32_t c = (a < s[5] || a < s[4]) ? 1 : 0;
        s[4] = a;
        for (int k : {3, 2, 1}) {
            const uint32_t old = s[size_t(k)];
            a = (a + old) + c;
            c = a < old ? 1 : 0;
            s[size_t(k)] = a;
        }
        a = (a + s[0]) + c;
        s[0] = a;
        s[5] = s[5] + 1;
        if (s[5] == 0) {
            s[4] = s[4] + 1;
            if (s[4] == 0) {
                s[3] = s[3] + 1;
                if (s[3] == 0) {
                    s[2] = s[2] + 1;
                    if (s[2] == 0) {
                        s[1] = s[1] + 1;
                        if (s[1] == 0) {
                            s[0] = a + 1;
                        }
                    }
                }
            }
        }
        return s[0];
    }

private:
    std::array<uint32_t, 6> s_{};
};

struct State {
    struct RingEntry {
        uint32_t value = 0xffff;
        uint32_t key = 0xffff;
    };
    std::array<RingEntry, 32> ring{};
    int ringIdx = 0;
    struct Hist {
        int cursor = 0;
        std::vector<int> list;
    };
    std::map<uint32_t, Hist> hdrHist;
    struct Last {
        uint32_t id = 0xffff;
        int count = 0;
    };
    std::map<int, Last> last;
};

inline State::Hist& hist(State& st, const Hdr& h) {
    auto it = st.hdrHist.find(h.id);
    if (it == st.hdrHist.end()) {
        State::Hist x;
        x.cursor = 0;
        x.list.assign(size_t(std::max(h.H, 0)), 0xff);
        it = st.hdrHist.emplace(h.id, x).first;
    }
    return it->second;
}
// 0x3D6E30
inline int recency(State& st, const Hdr& h, int line) {
    auto& x = hist(st, h);
    const int H = h.H;
    if (H <= 0) {
        return 0;
    }
    int p = x.cursor - 1;
    for (int k = 0; k < H; ++k) {
        if (p < 0) {
            p = H - 1;
        }
        if (x.list[size_t(p)] == line) {
            return k + 1;
        }
        p--;
    }
    return 0;
}
// 0x3D6EE8
inline void pushHistory(State& st, const Hdr& h, int line) {
    auto& x = hist(st, h);
    if (x.cursor < h.H) {
        x.list[size_t(x.cursor)] = line;
    }
    int c = (x.cursor + 1) & 0xff;
    if (!(c < h.H)) {
        c = 0;
    }
    x.cursor = c;
}

// 0x3DB5C0
inline int randN(EaRng& rng, State& st, int n, int key = -1) {
    int depth = n >= 0 ? n >> 1 : -((-n) >> 1);
    if (!(depth < 11)) {
        depth = 10;
    }
    auto draw = [&]() { return int(uint32_t(uint64_t(rng.next() >> 16) * uint64_t(uint32_t(n))) >> 16); };
    int best = draw();
    if (key == -1) {
        return best;
    }
    const uint32_t k = uint32_t(key) & 0xffff;
    int fp = -1;
    int attempts = 0;
    int t2 = best;
    for (;;) {
        t2 = draw();
        const int t0 = std::min(depth, 32);
        int idx = st.ringIdx;
        int matches = 0;
        int found = -1;
        int scanned = 0;
        for (;;) {
            bool stop = false;
            if (!(matches < t0)) {
                stop = true;
            } else {
                const auto& e = st.ring[size_t(idx)];
                if (e.key == k) {
                    matches++;
                    if (int(e.value) == t2) {
                        stop = true;
                        found = matches - 1;
                    }
                }
            }
            idx--;
            scanned++;
            if (idx < 0) {
                idx += 32;
            }
            if (scanned >= 33) {
                stop = true;
            }
            if (stop) {
                break;
            }
        }
        bool done = false;
        if (found == -1) {
            best = t2;
            done = true;
        } else if (found > fp) {
            fp = found;
            best = t2;
        }
        attempts++;
        if (attempts >= 32) {
            done = true;
        }
        if (done) {
            break;
        }
    }
    st.ringIdx = st.ringIdx + 1 < 32 ? st.ringIdx + 1 : 0;
    st.ring[size_t(st.ringIdx)] = {uint32_t(best) & 0xffff, k};
    return t2;
}

struct LineItem {
    uint32_t bankId = 0;
    std::string bank;
    int line = 0;
    int variant = -1;
};
struct Resolved {
    uint32_t eventId = 0;
    std::vector<LineItem> items;
    int record = -1;
    std::string reason;
};

class Engine {
public:
    Engine(EventFile events, std::vector<Hdr> banks, EaRng& rng, State& st) : events_(std::move(events)), banks_(std::move(banks)), rng_(rng), st_(st) {}
    const EventFile& events() const {
        return events_;
    }
    const Hdr* bank(uint32_t id) const {
        for (const auto& h : banks_) {
            if (h.id == id) {
                return &h;
            }
        }
        return nullptr;
    }

    // 0x3D9FB0
    Resolved resolve(uint32_t eventId, const std::vector<uint32_t>& args, int ch = 0, bool commit = true) {
        Resolved out;
        out.eventId = eventId;
        auto evIt = events_.events.find(eventId);
        if (evIt == events_.events.end()) {
            out.reason = "no such event";
            return out;
        }
        const Event& ev = evIt->second;
        std::array<uint32_t, 20> A{};
        A[0] = (1u << 24) | eventId;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i + 1 < 20) {
                A[i + 1] = args[i];
            }
        }
        const State::Last last = st_.last.count(ch) ? st_.last[ch] : State::Last{};
        if (last.id == ev.id && ev.maxRepeat > 0 && !(last.count < ev.maxRepeat)) {
            out.reason = "maxRepeat reached";
            return out;
        }
        const int s6 = 0;
        int extDontCare = 0;
        for (size_t j = 0; j < ev.ext.size(); ++j) {
            extDontCare |= (1 << (7 - int(j))) & 0xff;
        }
        uint32_t present = 0;
        if (ev.presence) {
            for (int i = 0; i < 20; ++i) {
                const uint32_t code = (ev.presence->codes >> (2 * i)) & 3;
                const uint32_t a = i + 1 < 20 ? A[size_t(i + 1)] : 0;
                const bool has = code == 1 ? a != 0 : code == 2 ? a != 0xffffffffu : false;
                if (has && i < 16) {
                    present |= 1u << i;
                }
            }
        }
        const auto order = recordOrder(ev);
        for (int ri : order) {
            if (!(ri < int(ev.records.size()))) {
                break;
            }
            const Record& rec = ev.records[size_t(ri)];
            const int roll = randN(rng_, st_, 100, -1);
            if (!(roll < rec.chance)) {
                continue;
            }
            bool ok = true;
            for (size_t k = 0; k < ev.cond.size(); ++k) {
                const uint32_t m = rec.condMasks[k];
                if (m != 0 && (m & A[size_t(ev.cond[k].arg)]) == 0) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                continue;
            }
            if (ev.presence && ev.presence->req[size_t(ri)] != present) {
                continue;
            }
            auto res = buildRecord(rec, A, s6, 0, extDontCare);
            if (!res) {
                continue;
            }
            auto chosen = chooseLines(rec, *res, s6);
            if (!chosen) {
                continue;
            }
            for (const auto& c : *chosen) {
                LineItem it;
                it.bankId = c.h->id;
                it.bank = c.h->name;
                it.line = c.line;
                it.variant = c.sub;
                out.items.push_back(it);
            }
            if (commit) {
                for (const auto& c : *chosen) {
                    pushHistory(st_, *c.h, c.line);
                }
                auto& l = st_.last[ch];
                if (l.id == ev.id) {
                    l.count++;
                } else {
                    l.id = ev.id;
                    l.count = 1;
                }
            }
            out.record = ri;
            out.reason = "ok";
            return out;
        }
        out.reason = "no record resolved";
        return out;
    }

private:
    struct Built {
        bool done = false;
        const Hdr* h = nullptr;
        int sub = -1;
        std::vector<int> cands;
    };
    struct Chosen {
        const Hdr* h;
        int sub;
        int line;
    };
    // 0x3D8CB0
    std::vector<int> recordOrder(const Event& ev) {
        const int n = int(ev.records.size());
        std::vector<int> w;
        int total = 0;
        for (const auto& r : ev.records) {
            w.push_back(r.weight);
            total += r.weight;
        }
        std::vector<int> order;
        while (total > 0) {
            int r = randN(rng_, st_, total, -1);
            int i = 0;
            r -= w[0];
            while (r >= 0) {
                i++;
                if (!(i < n)) {
                    break;
                }
                r -= w[size_t(i)];
            }
            order.push_back(i);
            // past the last record: the JS's total becomes NaN (w[n] undefined) and the loop ends
            if (!(i < n)) {
                break;
            }
            total -= w[size_t(i)];
            w[size_t(i)] = 0;
        }
        for (int i = 0; i < n; ++i) {
            if (ev.records[size_t(i)].weight == 0) {
                order.push_back(i);
            }
        }
        return order;
    }
    // 0x3D9088 + 0x3D88C8
    std::optional<std::vector<Built>> buildRecord(const Record& rec, const std::array<uint32_t, 20>& A, int s6, int extVal, int extDontCare) {
        const int nItems = int(rec.items.size());
        if (!(nItems < 13)) {
            return std::nullopt;
        }
        const int mode = rec.mode;
        const bool modeOk = mode == 1 ? s6 == 2 : mode == 2 ? s6 != 2 : mode == 0;
        if (!modeOk) {
            return std::nullopt;
        }
        if ((((rec.extVal ^ extVal) & rec.extMask & ~extDontCare) & 0xff) != 0) {
            return std::nullopt;
        }
        std::vector<uint32_t> recState(size_t(rec.nState), 0);
        std::vector<Built> out(static_cast<size_t>(nItems));
        int tries = 200;
        bool allOk;
        do {
            allOk = true;
            tries--;
            for (int i = 0; i < nItems; ++i) {
                const Item& item = rec.items[size_t(i)];
                bool ready = true;
                for (int j = 0; j < item.nf; ++j) {
                    if (item.param[size_t(j)] == 0xff && item.state[size_t(j)] != 0xff && (item.state[size_t(j)] & 0x80)) {
                        if (recState[size_t(item.state[size_t(j)] & 0x7f)] == 0) {
                            ready = false;
                        }
                    }
                }
                if (!ready) {
                    allOk = false;
                    continue;
                }
                if (out[size_t(i)].done) {
                    continue;
                }
                const Hdr* h = nullptr;
                int sub = -1;
                if (item.type == 0) {
                    h = bank(item.bankId);
                } else if (item.type == 1) {
                    if (item.argIdx < 20) {
                        const uint32_t want = A[size_t(item.argIdx)] & 0xffff;
                        for (const auto& x : banks_) {
                            if (x.id == item.bankId && x.sub == want) {
                                h = &x;
                            }
                        }
                    }
                } else if (item.type == 2) {
                    h = bank(item.bankId);
                    const uint32_t v = A[size_t(item.argIdx)];
                    if (h && !(h->variants != 0xffff && v < uint32_t(h->variants))) {
                        h = nullptr;
                    }
                    sub = int(A[size_t(item.argIdx)] & 0xffff);
                }
                if (!h) {
                    return std::nullopt;
                }
                auto cands = candidates(item, *h, sub, A, recState);
                if (cands.empty()) {
                    return std::nullopt;
                }
                out[size_t(i)] = {true, h, sub, cands};
            }
        } while (!allOk && tries > 0);
        if (tries == 0 && !allOk) {
            return std::nullopt;
        }
        return out;
    }
    std::vector<int> candidates(const Item& item, const Hdr& h, int sub, const std::array<uint32_t, 20>& A, std::vector<uint32_t>& recState) {
        const int F = h.F;
        for (int j = 0; j < F; ++j) {
            const int p = j < 4 ? item.param[size_t(j)] : 0;
            if (p == 0 || p == 0xff || p == 0xfe) {
                continue;
            }
            if ((h.masks[size_t(j)] & A[size_t(p)]) == 0) {
                return {};
            }
        }
        bool hasFE = false;
        for (int j = 0; j < std::max(item.nf, 0) && j < 4; ++j) {
            hasFE = hasFE || item.param[size_t(j)] == 0xfe;
        }
        std::vector<int> fresh;
        int bestR = 0;
        int bestLine = -1;
        for (int line = 0; line < h.N; ++line) {
            if (!lineMatches(item, h.keys[size_t(line)], A, recState)) {
                continue;
            }
            if (!(h.N >= line)) {
                continue;
            }
            if (sub != -1 && !(sub < h.variants)) {
                continue;
            }
            const int r = recency(st_, h, line);
            if (r > 0) {
                if (bestR < r) {
                    bestR = r;
                    bestLine = line;
                }
            } else {
                fresh.push_back(line);
                if (hasFE) {
                    produce(item, h.keys[size_t(line)], recState);
                }
            }
        }
        if (fresh.empty() && bestLine != -1) {
            fresh.push_back(bestLine);
            if (hasFE) {
                produce(item, h.keys[size_t(bestLine)], recState);
            }
        }
        return fresh;
    }
    static void produce(const Item& item, const std::vector<int>& keys, std::vector<uint32_t>& recState) {
        for (int j = 0; j < item.nf; ++j) {
            if (item.param[size_t(j)] == 0xfe && (item.state[size_t(j)] & 0x80)) {
                recState[size_t(item.state[size_t(j)] & 0x7f)] |= 1u << keys[size_t(j)];
            }
        }
    }
    // 0x3D85C8
    static bool lineMatches(const Item& item, const std::vector<int>& keys, const std::array<uint32_t, 20>& A, const std::vector<uint32_t>& recState) {
        const int nf = item.nf;
        if (!(nf < 5)) {
            return false;
        }
        for (int j = 0; j < nf; ++j) {
            const int key = size_t(j) < keys.size() ? keys[size_t(j)] : 0;
            if (!(key < 32)) {
                return false;
            }
            const uint32_t bit = 1u << key;
            if ((item.fmask[size_t(j)] & bit) == 0) {
                return false;
            }
            const int p = item.param[size_t(j)];
            if (p == 0 || p == 0xfe) {
                continue;
            }
            if (p == 0xff) {
                const int s = item.state[size_t(j)];
                if (s & 0x80) {
                    if ((recState[size_t(s & 0x7f)] & bit) == 0) {
                        return false;
                    }
                } else {
                    // file variables: none in SSX3 (fileVars[s] is undefined)
                    return false;
                }
                continue;
            }
            if ((A[size_t(p)] & bit) == 0) {
                return false;
            }
        }
        return true;
    }
    // 0x3D95B8
    std::optional<std::vector<Chosen>> chooseLines(const Record& rec, std::vector<Built>& res, int s6) {
        auto key = [](const Hdr* h) { return (h->regIndex >= 0 ? h->regIndex : int(h->id)) & 0xffff; };
        if (s6 != 1 && rec.nState == 0) {
            std::vector<Chosen> pick;
            for (const auto& o : res) {
                const int n = int(o.cands.size());
                if (n == 0) {
                    return std::nullopt;
                }
                const int k = randN(rng_, st_, n, key(o.h));
                pick.push_back({o.h, o.sub, o.cands[size_t(k)]});
            }
            return pick;
        }
        for (auto& o : res) {
            for (int s0 = int(o.cands.size()); s0 >= 2; --s0) {
                const int j = randN(rng_, st_, s0, -1);
                std::swap(o.cands[size_t(s0 - 1)], o.cands[size_t(j)]);
            }
        }
        std::vector<size_t> cur(res.size(), 0);
        for (;;) {
            if (consistent(rec, res, cur)) {
                std::vector<Chosen> out;
                for (size_t i = 0; i < res.size(); ++i) {
                    out.push_back({res[i].h, res[i].sub, res[i].cands[cur[i]]});
                }
                return out;
            }
            int k = int(res.size()) - 1;
            for (;;) {
                cur[size_t(k)]++;
                if (cur[size_t(k)] < res[size_t(k)].cands.size()) {
                    break;
                }
                cur[size_t(k)] = 0;
                k--;
                if (k < 0) {
                    return std::nullopt;
                }
            }
        }
    }
    static bool consistent(const Record& rec, const std::vector<Built>& res, const std::vector<size_t>& cur) {
        std::vector<uint32_t> S(size_t(rec.nState), 0);
        std::vector<bool> done(res.size(), false);
        for (int pass = 0; pass < 64; ++pass) {
            bool again = false;
            for (size_t i = 0; i < res.size(); ++i) {
                if (done[i]) {
                    continue;
                }
                done[i] = true;
                const Item& item = rec.items[i];
                const auto& keys = res[i].h->keys[size_t(res[i].cands[cur[i]])];
                for (int j = 0; j < item.nf; ++j) {
                    const int s = item.state[size_t(j)];
                    if (s == 0xff || !(s & 0x80)) {
                        continue;
                    }
                    if (item.param[size_t(j)] == 0xff) {
                        const uint32_t v = S[size_t(s & 0x7f)];
                        if (v == 0) {
                            done[i] = false;
                            again = true;
                        } else if (!((v >> keys[size_t(j)]) & 1)) {
                            return false;
                        }
                    } else if (item.param[size_t(j)] == 0xfe) {
                        S[size_t(s & 0x7f)] = 1u << keys[size_t(j)];
                    }
                }
            }
            if (!again) {
                return true;
            }
        }
        return false;
    }

    EventFile events_;
    std::vector<Hdr> banks_;
    EaRng& rng_;
    State& st_;
};

// the pending-event scheduler (speech-timer ticks, 60 Hz)
class Scheduler {
public:
    Scheduler(Engine& engine, EaRng& rng, State& st) : engine_(engine), rng_(rng), st_(st) {
        slots.assign(16, Slot{});
        count.assign(8, 0);
        lastPosted.assign(8, -1);
        lastPostedPlain.assign(8, -1);
    }
    struct Slot {
        bool active = false;
        const Event* ev = nullptr;
        int ch = 0;
        uint32_t seq = 0;
        int64_t time = 0;
        bool queued = false;
        int link = -1;
        std::vector<uint32_t> args;
    };
    std::vector<Slot> slots;
    std::vector<int> count;
    std::vector<int> lastPosted;
    std::vector<int> lastPostedPlain;

    // 0x3D7418
    bool post(uint32_t eventId, const std::vector<uint32_t>& args, int64_t now, int ch = 0) {
        auto it = engine_.events().events.find(eventId);
        if (it == engine_.events().events.end()) {
            return false;
        }
        const Event& ev = it->second;
        const bool queued = (ev.flags & 0x20) != 0;
        int P = ev.P;
        if (queued) {
            const int a = lastPostedPlain[size_t(ch)];
            const int b = lastPosted[size_t(ch)];
            if (!(a >= 0 && a < 16 && b >= 0 && b < 16)) {
                return false;
            }
            P = slots[size_t(a)].ev->P;
        } else {
            const int roll = randN(rng_, st_, 100, -1);
            if (ev.chance < roll) {
                return false;
            }
        }
        const int s = allocSlot(P, ch, queued, now);
        if (s < 0) {
            return false;
        }
        if (now != seqTime_) {
            seq_ = 0;
        }
        seq_ = (seq_ + 1) & 0xffff;
        seqTime_ = now;
        slots[size_t(s)] = {true, &ev, ch, seq_, now, queued, -1, args};
        if (queued) {
            slots[size_t(lastPosted[size_t(ch)])].link = s;
        } else {
            count[size_t(ch)]++;
        }
        lastPosted[size_t(ch)] = s;
        if (!queued) {
            lastPostedPlain[size_t(ch)] = s;
        }
        return true;
    }
    // 0x3D7C38
    void kill(int i) {
        Slot& sl = slots[size_t(i)];
        if (sl.queued) {
            return;
        }
        sl.active = false;
        if (lastPostedPlain[size_t(sl.ch)] == i) {
            lastPostedPlain[size_t(sl.ch)] = -1;
        }
        if (lastPosted[size_t(sl.ch)] == i) {
            lastPosted[size_t(sl.ch)] = -1;
        }
        int l = sl.link;
        while (l >= 0 && l < 16) {
            Slot& q = slots[size_t(l)];
            if (!q.queued || !q.active) {
                break;
            }
            if (lastPosted[size_t(sl.ch)] == l) {
                lastPosted[size_t(sl.ch)] = -1;
            }
            q.active = false;
            l = q.link;
        }
        count[size_t(sl.ch)] = std::max(0, count[size_t(sl.ch)] - 1);
    }
    // 0x3D7EC8
    std::optional<std::vector<Resolved>> dispatch(int ch, int64_t now) {
        if (count[size_t(ch)] == 0) {
            return std::nullopt;
        }
        const Event* lastEv = nullptr;
        if (st_.last.count(ch)) {
            auto it = engine_.events().events.find(st_.last[ch].id);
            if (it != engine_.events().events.end()) {
                lastEv = &it->second;
            }
        }
        int s;
        if (lastEv && lastEv->follow) {
            s = pick(ch, now, &*lastEv->follow);
            if (s < 0) {
                s = pick(ch, now, nullptr);
            }
        } else {
            s = pick(ch, now, nullptr);
        }
        if (s < 0) {
            return std::nullopt;
        }
        const Slot chosen = slots[size_t(s)];
        for (int i = 0; i < 16; ++i) {
            if (i == s) {
                continue;
            }
            const Slot& o = slots[size_t(i)];
            if (!o.active || o.ch != chosen.ch || o.queued) {
                continue;
            }
            const bool older = o.time < chosen.time || (o.time == chosen.time && o.seq < chosen.seq);
            if (older && !(o.ev->flags & 0x04)) {
                kill(i);
            }
        }
        std::vector<Resolved> out;
        int cur = s;
        Resolved r;
        do {
            const Slot& sl = slots[size_t(cur)];
            r = engine_.resolve(sl.ev->id, sl.args, ch);
            out.push_back(r);
            cur = sl.link;
        } while (cur != -1 && !r.items.empty());
        kill(s);
        return out;
    }

private:
    // 0x3D72A0
    int allocSlot(int P, int ch, bool queued, int64_t now) {
        for (int i = 0; i < 16; ++i) {
            if (!slots[size_t(i)].active) {
                return i;
            }
        }
        const int keep = queued ? lastPostedPlain[size_t(ch)] : -1;
        for (int i = 0; i < 16; ++i) {
            const Slot& sl = slots[size_t(i)];
            if (sl.queued || sl.ev->W == 0) {
                continue;
            }
            if (!(uint32_t(sl.ev->W) < uint32_t(now - sl.time))) {
                continue;
            }
            if (i == keep) {
                continue;
            }
            kill(i);
            return i;
        }
        for (int i = 0; i < 16; ++i) {
            Slot& sl = slots[size_t(i)];
            if (sl.queued || P < sl.ev->P || sl.ch != ch || i == keep) {
                continue;
            }
            sl.active = false;
            count[size_t(ch)]--;
            return i;
        }
        return -1;
    }
    // 0x3D7760
    int pick(int ch, int64_t now, const std::vector<uint32_t>* follow) {
        int best = -1;
        int bestP = 0;
        uint32_t bestAge = 0xffffffffu;
        uint32_t bestSeq = 0;
        for (int i = 0; i < 16; ++i) {
            const Slot& sl = slots[size_t(i)];
            if (!sl.active || sl.ch != ch || sl.queued) {
                continue;
            }
            const uint32_t age = uint32_t(now - sl.time);
            if (sl.ev->W != 0 && uint32_t(sl.ev->W) < age) {
                kill(i);
                continue;
            }
            if (follow && std::find(follow->begin(), follow->end(), sl.ev->id) == follow->end()) {
                continue;
            }
            const int P = sl.ev->P;
            if (bestP < P || (P == bestP && (age < bestAge || (age == bestAge && bestSeq < sl.seq)))) {
                best = i;
                bestP = P;
                bestAge = age;
                bestSeq = sl.seq;
            }
        }
        return best;
    }

    Engine& engine_;
    EaRng& rng_;
    State& st_;
    int64_t seqTime_ = -1;
    uint32_t seq_ = 0;
};

// the game's request table (0x2B1458 / 0x2B1520 / 0x2B1720)
class RequestTable {
public:
    struct Entry {
        bool active = false;
        int ttl = 0;
        int ch = 0;
        uint32_t eventId = 0;
        int speaker = -1;
    };
    std::array<Entry, 10> e{};
    bool admit(int ch, uint32_t eventId, bool refreshDup, int speaker) {
        int free = -1;
        for (int i = 0; i < 10; ++i) {
            Entry& x = e[size_t(i)];
            if (x.active) {
                if (x.ch == ch && x.eventId == eventId) {
                    if (!refreshDup) {
                        return false;
                    }
                    x.ttl = 180;
                    return true;
                }
            } else if (free == -1) {
                free = i;
            }
        }
        if (free == -1) {
            return false;
        }
        e[size_t(free)] = {true, 180, ch, eventId, speaker};
        return true;
    }
    void tick() {
        for (auto& x : e) {
            if (x.active && --x.ttl <= 0) {
                x.active = false;
            }
        }
    }
    const Entry* lookup(int ch, uint32_t eventId) const {
        for (const auto& x : e) {
            if (x.active && x.ch == ch && x.eventId == eventId) {
                return &x;
            }
        }
        return nullptr;
    }
};

}  // namespace speech

}  // namespace audio

}  // namespace ssx_native
