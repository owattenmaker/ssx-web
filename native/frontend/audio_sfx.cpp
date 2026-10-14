#include "audio_sfx.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ssx_native {

namespace audio {

double curve(const Curve& c, double v) {
    for (int i = 1; i < 5; ++i) {
        if (c.x[size_t(i - 1)] <= v && v < c.x[size_t(i)]) {
            return c.y[size_t(i - 1)] + (v - c.x[size_t(i - 1)]) * (c.y[size_t(i)] - c.y[size_t(i - 1)]) / (c.x[size_t(i)] - c.x[size_t(i - 1)]);
        }
    }
    if (v < c.x[0]) {
        return c.y[0];
    }
    if (c.x[4] <= v) {
        return c.y[4];
    }
    return 0;
}

namespace curves {
const Curve loopA_vol_482AF8{{0.00024549, 0.00255361, 0.02695501, 0.1500098, 0.1999571}, {0, 101, 61, 0, 0}};
const Curve loopA_pitch_482B20{{0.05, 0.1, 0.15, 0.2, 0.25}, {1.000076, 1.2, 1.5, 2, 2}};
const Curve glide0_vol_482B78{{0.0185, 0.09460434, 0.3431811, 0.7000265, 1}, {0, 16, 28, 86, 104}};
const Curve glide0_pitch_482BA0{{0.05, 0, 0.5, 0.8054432, 0.95}, {1, 0.5, 0.5, 0.5, 0.5}};
const Curve glide1_vol_482BC8{{0.0185, 0.1, 0.5, 0.7000253, 0.9510077}, {2, 15, 32, 97, 127}};
const Curve glide1_pitch_482BF0{{0.1555642, 1, 0.5, 0.8, 0.95}, {0.9999683, 0.9358777, 0.75, 0.6496397, 0.5}};
const Curve glide2_vol_482C18{{0.0185, 0.1, 0.5, 0.6, 0.95}, {2, 15, 30, 50, 80}};
const Curve glide2_pitch_482C40{{0.05, 0.2, 0.5, 0.8, 0.95}, {1, 0.9, 0.75, 0.65, 0.5}};
const Curve carve0_vol_482C68{{0.01004235, 0.2500007, 0.4999053, 0.7587754, 1}, {0, 11, 36, 71, 107}};
const Curve carve0_pitch_482C90{{0, 0, 0.4999508, 0.8, 1}, {1, 0.5, 0.5, 0.5, 0.5}};
const Curve carve1_vol_482CB8{{0.01, 0.2500082, 0.5000058, 0.9, 1}, {0, 49, 59, 70, 127}};
const Curve carve1_pitch_482CE0{{0.05, 0.2, 0.5, 0.8, 0.95}, {0.6011547, 0.5, 0.5672877, 0.5, 0.5}};
const Curve carve2_pitch_482D08{{0.05, 0.1544433, 0.5, 0.8, 0.95}, {1, 0.5, 0.5, 1.000354, 0.5}};
const Curve landing_vol_482D30{{100, 300, 800, 1200, 2000}, {10, 28, 34, 80, 127}};
const Curve crash_vol_482DA0{{100, 800, 1200, 1400, 1600}, {127, 127, 127, 127, 127}};
const Curve contact_445898{{200, 400, 550, 800, 800}, {33, 70, 100, 127, 127}};
const Curve bump_4458C0{{100, 1000, 2000, 4000, 4000}, {20, 70, 100, 127, 127}};
const Curve t2_4458E8{{500, 1000, 1500, 2000, 2000}, {0, 10, 20, 40, 40}};
const Curve grunt_445910{{500, 1000, 1500, 2000, 2500}, {60, 75, 90, 100, 127}};
}  // namespace curves

int surfaceClass(double surface) {
    static const int cls[21] = {0, 0, 2, 1, 1, 3, 0, 0, 7, 7, 5, 4, 8, 0, 1, 0, 0, 0, 0, 7, 0};
    const int i = int(surface) + 1;
    return i >= 0 && i < 21 ? cls[i] : 0;
}

int boardFamily(int cls, double motion) {
    return cls == 0 ? 0 : cls <= 2 ? 1 : cls == 3 ? (motion == 4 ? 2 : 0) : 2;
}

namespace {
// JS ToInt32 of an integer-valued double
int32_t toInt32(double v) {
    if (!std::isfinite(v)) {
        return 0;
    }
    const double m = std::fmod(std::trunc(v), 4294967296.0);
    return int32_t(uint32_t(int64_t(m < 0 ? m + 4294967296.0 : m)));
}
}  // namespace

int rndRange(int x, Random& random) {
    return toInt32((std::floor(random() * 0x8000) - 0x4000) * x) >> 14;
}

Decoded spuResample(const Decoded& d, int outRate) {
    static const int16_t g[512] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 7, 7, 8,
        9, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 24, 25, 27, 28, 30, 32, 33, 35, 37, 39, 41, 44, 46, 48, 51, 53, 56, 58, 61, 64, 67,
        70, 73, 77, 80, 84, 87, 91, 95, 99, 103, 107, 111, 116, 120, 125, 130, 135, 140, 145, 150, 156, 161, 167, 173, 179, 186, 192, 199, 205,
        212, 219, 227, 234, 242, 250, 257, 266, 274, 283, 291, 300, 309, 319, 328, 338, 348, 358, 369, 379, 390, 401, 412, 424, 436, 448, 460,
        473, 485, 498, 512, 525, 539, 553, 567, 582, 597, 612, 627, 643, 659, 675, 692, 708, 726, 743, 761, 779, 797, 816, 835, 854, 874, 894,
        914, 935, 956, 977, 999, 1020, 1043, 1066, 1089, 1112, 1136, 1160, 1184, 1209, 1234, 1260, 1286, 1312, 1339, 1366, 1394, 1422, 1450, 1479,
        1508, 1537, 1567, 1598, 1628, 1660, 1691, 1723, 1756, 1789, 1822, 1856, 1890, 1924, 1959, 1995, 2031, 2067, 2104, 2141, 2179, 2217, 2256,
        2295, 2334, 2374, 2415, 2456, 2497, 2539, 2582, 2624, 2668, 2712, 2756, 2801, 2846, 2892, 2938, 2985, 3032, 3079, 3128, 3176, 3225, 3275,
        3325, 3376, 3427, 3479, 3531, 3584, 3637, 3691, 3745, 3799, 3855, 3910, 3967, 4023, 4081, 4138, 4197, 4255, 4315, 4374, 4435, 4495, 4557,
        4619, 4681, 4744, 4807, 4871, 4935, 5000, 5065, 5131, 5197, 5264, 5332, 5399, 5468, 5536, 5606, 5676, 5746, 5817, 5888, 5959, 6032, 6104,
        6177, 6251, 6325, 6400, 6475, 6550, 6626, 6702, 6779, 6856, 6934, 7012, 7091, 7170, 7249, 7329, 7409, 7490, 7571, 7653, 7735, 7817, 7900,
        7983, 8066, 8150, 8234, 8319, 8404, 8489, 8575, 8661, 8748, 8834, 8922, 9009, 9097, 9185, 9273, 9362, 9451, 9541, 9630, 9720, 9811, 9901,
        9992, 10083, 10174, 10266, 10358, 10450, 10542, 10635, 10727, 10820, 10913, 11007, 11100, 11194, 11288, 11382, 11476, 11571, 11665, 11760,
        11855, 11950, 12045, 12140, 12236, 12331, 12427, 12522, 12618, 12714, 12809, 12905, 13001, 13097, 13193, 13289, 13385, 13481, 13577,
        13673, 13769, 13865, 13961, 14056, 14152, 14248, 14343, 14439, 14534, 14630, 14725, 14820, 14915, 15010, 15104, 15199, 15293, 15387,
        15481, 15575, 15669, 15762, 15855, 15948, 16041, 16133, 16226, 16317, 16409, 16500, 16592, 16682, 16773, 16863, 16953, 17042, 17131,
        17220, 17308, 17396, 17484, 17571, 17658, 17744, 17830, 17916, 18001, 18086, 18170, 18254, 18337, 18420, 18502, 18584, 18665, 18746,
        18826, 18905, 18985, 19063, 19141, 19219, 19295, 19372, 19447, 19522, 19597, 19671, 19744, 19816, 19888, 19959, 20030, 20100, 20169,
        20238, 20306, 20373, 20439, 20505, 20570, 20634, 20698, 20760, 20822, 20884, 20944, 21004, 21063, 21121, 21178, 21235, 21290, 21345,
        21399, 21452, 21505, 21556, 21607, 21657, 21706, 21754, 21801, 21848, 21893, 21938, 21982, 22025, 22066, 22107, 22148, 22187, 22225,
        22262, 22299, 22334, 22369, 22402, 22435, 22467, 22498, 22527, 22556, 22584, 22611, 22637, 22662, 22686, 22709, 22731, 22752, 22772,
        22791, 22809, 22826, 22842, 22857, 22872, 22885, 22897, 22908, 22918, 22927, 22935, 22942, 22948, 22953, 22957, 22960, 22962, 22963};
    if (!(d.sampleRate > 0) || !d.length) {
        return d;
    }
    const int64_t length = int64_t(d.length);
    const bool looped = d.loopStart >= 0 && d.loopEnd > d.loopStart;
    // the SPU pitch register (4.12): Math.round
    const int64_t step = int64_t(std::floor(double(d.sampleRate) * 4096 / outRate + 0.5));
    const int64_t n = (length * 4096) / step;
    Decoded out = d;
    out.sampleRate = outRate;
    out.length = size_t(n);
    out.data.clear();
    for (const auto& src : d.data) {
        auto s = [&](int64_t k) -> int32_t {
            if (k < 0) {
                return 0;
            }
            if (k >= length || (looped && k > d.loopEnd)) {
                if (!looped) {
                    return 0;
                }
                k = d.loopStart + ((k - d.loopStart) % (d.loopEnd - d.loopStart + 1));
            }
            // Math.round(src[k] x 32768)
            return int32_t(std::floor(double(src[size_t(k)]) * 32768 + 0.5));
        };
        std::vector<float> o(static_cast<size_t>(n));
        int64_t pos = 0;
        for (int64_t j = 0; j < n; ++j, pos += step) {
            const int64_t k = pos >> 12;
            const int i = int((pos >> 4) & 0xff);
            int32_t v = ((g[255 - i] * s(k - 1)) >> 15) + ((g[511 - i] * s(k)) >> 15) + ((g[256 + i] * s(k + 1)) >> 15) + ((g[i] * s(k + 2)) >> 15);
            v = std::clamp(v, -32768, 32767);
            o[size_t(j)] = float(double(v) / 32768);
        }
        out.data.push_back(std::move(o));
    }
    auto scale = [&](int64_t x) { return x >= 0 ? int64_t(std::floor(double(x) * 4096 / double(step) + 0.5)) : x; };
    out.loopStart = scale(d.loopStart);
    out.loopEnd = looped ? std::min<int64_t>(n - 1, scale(d.loopEnd + 1) - 1) : d.loopEnd;
    return out;
}

namespace {

// web/sfx.js parsePatchTags: the patch parameters (< 0x80, <= 4 bytes, signed big-endian) with their file addresses
std::vector<std::map<int, std::pair<double, size_t>>> parsePatchTags(const std::vector<uint8_t>& bnk, size_t headerOffset) {
    size_t p = headerOffset + 4;
    std::vector<std::map<int, std::pair<double, size_t>>> patches;
    std::map<int, std::pair<double, size_t>> cur;
    for (;;) {
        if (p >= bnk.size()) {
            break;
        }
        const int t = bnk[p++];
        if (t == 0xfc || t == 0xfd) {
            continue;
        }
        if (t == 0xff) {
            break;
        }
        if (t == 0xfe) {
            patches.push_back(cur);
            cur.clear();
            continue;
        }
        uint32_t len = bnk[p++];
        if (len == 0xff) {
            len = (uint32_t(bnk[p]) << 24) | (uint32_t(bnk[p + 1]) << 16) | (uint32_t(bnk[p + 2]) << 8) | uint32_t(bnk[p + 3]);
            p += 4;
        }
        if (t < 0x80 && len <= 4) {
            double v = 0;
            for (uint32_t i = 0; i < len; ++i) {
                v = v * 256 + bnk[p + i];
            }
            if (len && (bnk[p] & 0x80)) {
                v -= std::pow(2.0, 8.0 * len);
            }
            cur[t] = {v, p};
        }
        p += len;
    }
    patches.push_back(cur);
    return patches;
}

int32_t i32le(const std::vector<uint8_t>& b, size_t a) {
    return int32_t(uint32_t(b[a]) | (uint32_t(b[a + 1]) << 8) | (uint32_t(b[a + 2]) << 16) | (uint32_t(b[a + 3]) << 24));
}

// the exported bank JSON's patch as audio_decode's SampleInfo
SampleInfo patchInfo(const Json& p) {
    SampleInfo s;
    s.version = p.value("version", 0);
    s.codec = p.value("codec", 5);
    s.channels = p.value("channels", 1);
    s.sampleRate = p.value("sampleRate", 22050);
    s.sampleCount = p.value("sampleCount", int64_t(0));
    s.loopStart = p.value("loopStart", int64_t(-1));
    s.loopEnd = p.value("loopEnd", int64_t(-1));
    for (const auto& o : p["dataOffsets"]) {
        s.dataOffsets.push_back(o.get<int64_t>());
    }
    if (p.contains("params")) {
        for (auto it = p["params"].begin(); it != p["params"].end(); ++it) {
            Tag t;
            if (it.value().is_number()) {
                t.value = it.value().get<double>();
            }
            s.params[int(std::stol(it.key(), nullptr, 0))] = t;
        }
    }
    return s;
}

std::shared_ptr<wa::Buffer> toBuffer(const Decoded& d) {
    auto b = std::make_shared<wa::Buffer>();
    b->sampleRate = float(d.sampleRate);
    b->length = std::max<size_t>(1, d.length);
    for (const auto& c : d.data) {
        std::vector<float> v = c;
        v.resize(b->length, 0.f);
        // never a NaN into the mix
        for (float& x : v) {
            if (!std::isfinite(x)) {
                x = 0;
            }
        }
        b->channels.push_back(std::move(v));
    }
    if (b->channels.empty()) {
        b->channels.push_back(std::vector<float>(b->length, 0.f));
    }
    return b;
}

}  // namespace

// ---- banks ----

Sfx::Bank* Sfx::loadBank(int slot, const std::string& name) {
    auto it = banks_.find(slot);
    if (it != banks_.end() && it->second.name == name) {
        return &it->second;
    }
    keep(name);
    if (it != banks_.end()) {
        evictLater(it->second.name);
    }
    Bank bank;
    bank.name = name;
    try {
        bank.json = files_.json("banks/" + name + ".json");
        bank.bnk = files_.bytes("banks/" + bank.json->value("file", name + ".bnk"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Bank %s unavailable: %s\n", name.c_str(), e.what());
        bank.json = nullptr;
        bank.bnk = nullptr;
    }
    banks_[slot] = std::move(bank);
    return &banks_[slot];
}

void Sfx::unloadBank(int slot) {
    auto it = banks_.find(slot);
    if (it == banks_.end()) {
        return;
    }
    const std::string name = it->second.name;
    banks_.erase(it);
    evictLater(name);
}

Sfx::Bank* Sfx::bankOf(int slot) {
    auto it = banks_.find(slot);
    return it == banks_.end() ? nullptr : &it->second;
}

void Sfx::keep(const std::string& name) {
    auto it = evictTimers_.find(name);
    if (it != evictTimers_.end()) {
        timers_.clear(it->second);
        evictTimers_.erase(it);
    }
}

void Sfx::evictLater(const std::string& name) {
    if (name.empty()) {
        return;
    }
    keep(name);
    evictTimers_[name] = timers_.post(evictMs_, [this, name] {
        evictTimers_.erase(name);
        for (const auto& b : banks_) {
            if (b.second.name == name) {
                return;
            }
        }
        const std::string pre = name + "/";
        for (auto it = buffers_.begin(); it != buffers_.end();) {
            it = it->first.rfind(pre, 0) == 0 ? buffers_.erase(it) : std::next(it);
        }
    });
}

std::vector<Sfx::LayerInfo> Sfx::layersOf(Bank& bank, int sound) {
    std::vector<LayerInfo> out;
    if (!bank.json || !bank.bnk) {
        return out;
    }
    const auto& entries = (*bank.json)["entries"];
    if (sound < 0 || size_t(sound) >= entries.size() || entries[size_t(sound)].is_null()) {
        return out;
    }
    const Json& entry = entries[size_t(sound)];
    if (!entry.contains("patches") || entry["patches"].empty()) {
        return out;
    }
    auto t = bank.tags.find(sound);
    if (t == bank.tags.end()) {
        t = bank.tags.emplace(sound, parsePatchTags(*bank.bnk, entry.value("headerOffset", size_t(0)))).first;
    }
    for (size_t i = 0; i < entry["patches"].size(); ++i) {
        LayerInfo l;
        l.patch = &entry["patches"][i];
        l.tags = i < t->second.size() ? t->second[i] : std::map<int, std::pair<double, size_t>>{};
        l.index = int(i);
        out.push_back(std::move(l));
    }
    return out;
}

std::shared_ptr<const wa::Buffer> Sfx::bufferFor(Bank& bank, int sound, const LayerInfo& layer) {
    const std::string key = bank.name + "/" + std::to_string(sound) + "/" + std::to_string(layer.index);
    auto it = buffers_.find(key);
    if (it != buffers_.end()) {
        return it->second;
    }
    const SampleInfo info = patchInfo(*layer.patch);
    Decoded d = decodeBankPatch(bank.bnk->data(), bank.bnk->size(), info);
    // hardware voices: the SPU's Gaussian interpolation
    auto b = toBuffer(info.codec == kVag ? spuResample(d) : d);
    buffers_[key] = b;
    return b;
}

std::shared_ptr<const wa::Buffer> Sfx::lfoBuffer(const std::string& key, const std::vector<int>& table, const std::function<double(int)>& map) {
    auto it = lfoBuffers_.find(key);
    if (it != lfoBuffers_.end()) {
        return it->second;
    }
    const int rate = 8000;
    const int step = rate / 100;
    auto b = std::make_shared<wa::Buffer>();
    b->sampleRate = float(rate);
    b->length = table.size() * size_t(step);
    b->channels.assign(1, std::vector<float>(b->length, 0.f));
    for (size_t i = 0; i < table.size(); ++i) {
        const double v = map(table[i]);
        std::fill(b->channels[0].begin() + long(i) * step, b->channels[0].begin() + long(i + 1) * step, std::isfinite(v) ? float(v) : 0.f);
    }
    lfoBuffers_[key] = b;
    return b;
}

void Sfx::warm(const std::vector<int>& slots) {
    for (int s : slots) {
        Bank* bank = bankOf(s);
        if (!bank || !bank->ready()) {
            continue;
        }
        const size_t n = (*bank->json)["entries"].size();
        for (size_t sound = 0; sound < n; ++sound) {
            for (const auto& l : layersOf(*bank, int(sound))) {
                try {
                    bufferFor(*bank, int(sound), l);
                } catch (...) {
                }
            }
        }
    }
}

// ---- the listener ----

void Sfx::setListener(const std::array<double, 16>* inverse, const std::array<double, 3>& origin) {
    hasListener_ = inverse != nullptr;
    if (inverse) {
        inverse_ = *inverse;
        origin_ = origin;
    }
}

Spatial Sfx::spatial(const std::array<double, 3>* position, double vanish) const {
    Spatial s;
    if (!position || !hasListener_) {
        return s;
    }
    const auto& m = inverse_;
    const auto& o = origin_;
    const double x = (*position)[0] / 100 - o[0];
    const double y = (*position)[2] / 100 - o[1];
    const double z = -(*position)[1] / 100 - o[2];
    const double vx = m[0] * x + m[4] * y + m[8] * z + m[12];
    const double vy = m[1] * x + m[5] * y + m[9] * z + m[13];
    const double vz = m[2] * x + m[6] * y + m[10] * z + m[14];
    const double px = vx;
    const double py = -vy + 0.25;
    const double pz = -vz - 1;
    const double d = std::sqrt(px * px + py * py + pz * pz);
    const double k = std::max(d - 0.5, 0.0);
    s.g = k >= vanish ? 0 : std::pow((vanish - k) / vanish, 2);
    s.az = std::atan2(px, pz);
    s.d = d;
    s.hasD = true;
    return s;
}

double Sfx::panValue(double patchPanRad, double az) const {
    if (mono_) {
        return 0;
    }
    double a = patchPanRad + az;
    a = std::atan2(std::sin(a), std::cos(a));
    if (a > M_PI / 2) {
        a = M_PI - a;
    } else if (a < -M_PI / 2) {
        a = -M_PI - a;
    }
    return a / (M_PI / 2);
}

// ---- voices ----

std::vector<std::shared_ptr<HwVoice>> Sfx::allocate(const std::string& pool, int n, double priority) {
    auto& list = used_[pool];
    const size_t cap = pool == "spu" ? 48 : 8;
    if (list.size() + size_t(n) > cap) {
        std::vector<std::shared_ptr<HwVoice>> victims;
        for (const auto& h : list) {
            if (!h->freed && h->priority <= std::min(priority, 100.0)) {
                victims.push_back(h);
            }
        }
        std::stable_sort(victims.begin(), victims.end(), [](const auto& a, const auto& b) { return a->priority < b->priority || (a->priority == b->priority && a->serial < b->serial); });
        size_t vi = 0;
        while (list.size() + size_t(n) > cap && vi < victims.size()) {
            auto v = victims[vi++];
            auto owner = v->layer.lock();
            auto voice = v->voice.lock();
            if (!owner) {
                release(*v);
                continue;
            }
            for (const auto& h : owner->hw) {
                release(*h);
            }
            try {
                owner->source->stop();
            } catch (...) {
            }
            owner->done = true;
            if (voice) {
                bool all = true;
                for (const auto& l : voice->layers_) {
                    all = all && l->done;
                }
                if (all) {
                    finish(*voice);
                }
            }
        }
        if (list.size() + size_t(n) > cap) {
            return {};
        }
    }
    std::vector<std::shared_ptr<HwVoice>> got;
    for (int i = 0; i < n; ++i) {
        auto h = std::make_shared<HwVoice>();
        h->pool = pool;
        h->priority = priority;
        h->serial = ++serial_;
        list.push_back(h);
        got.push_back(h);
    }
    return got;
}

void Sfx::release(HwVoice& h) {
    if (h.freed) {
        return;
    }
    h.freed = true;
    auto& list = used_[h.pool];
    list.erase(std::remove_if(list.begin(), list.end(), [&](const auto& x) { return x.get() == &h; }), list.end());
}

std::shared_ptr<Voice> Sfx::play(PlayOptions o) {
    if (!(o.delayMs > 0)) {
        return startVoice(o);
    }
    // 2ADCA0: the voice is requested (and its hardware voices allocated) when the timer fires
    auto handle = std::make_shared<Voice>();
    handle->sfx_ = this;
    handle->deferred_ = true;
    handle->deferredBase_ = o.volume;
    // the timer keeps the handle (the page's setTimeout closure): a request whose caller drops the handle still plays
    timers_.post(o.delayMs, [this, h = handle, o]() mutable {
        if (h->cancelled_) {
            return;
        }
        o.delayMs = 0;
        o.volume = h->deferredBase_;
        h->real_ = startVoice(o);
        if (!h->real_) {
            h->cancelled_ = true;
        }
    });
    return handle;
}

std::shared_ptr<Voice> Sfx::startVoice(const PlayOptions& o) {
    wa::Context* context = engine_.unlocked() && engine_.live() ? engine_.context() : nullptr;
    Bank* bank = bankOf(o.slot);
    if (!context || !bank || !bank->ready()) {
        return nullptr;
    }
    const auto layers = layersOf(*bank, o.sound);
    if (layers.empty()) {
        return nullptr;
    }
    // the layers' PCM before the context's lock (a first decode does not hold the audio thread)
    std::vector<std::shared_ptr<const wa::Buffer>> pre;
    if (startAfterDecode_) {
        for (const auto& l : layers) {
            try {
                pre.push_back(bufferFor(*bank, o.sound, l));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "Sound %s/%d failed: %s\n", bank->name.c_str(), o.sound, e.what());
                pre.push_back(nullptr);
            }
        }
    }
    std::lock_guard<std::recursive_mutex> lock(context->mutex());
    const double when = context->currentTime();
    auto voice = std::make_shared<Voice>();
    voice->sfx_ = this;
    voice->slot = o.slot;
    voice->sound = o.sound;
    voice->bus = o.bus;
    voice->tag = o.tag;
    voice->owner = o.owner;
    if (o.position && o.posStatic) {
        voice->position_ = std::make_shared<std::array<double, 3>>(*o.position);
    } else {
        voice->position_ = o.position;
        voice->posRef_ = o.position;
    }
    voice->base_ = o.volume;
    voice->bend_ = o.bend;
    voice->callback_ = o.callback;
    voice->out_ = context->createGain();
    voice->started_ = when;
    voice->wheel_ = o.wheel;
    voice->vanish_ = o.vanish;
    voice->out_->connect(engine_.bus(o.bus, o.speaker));
    Random& random = random_;
    for (size_t li = 0; li < layers.size(); ++li) {
        const LayerInfo& layer = layers[li];
        const Json& patch = *layer.patch;
        auto T = [&](int t, double d) {
            auto it = layer.tags.find(t);
            return it == layer.tags.end() ? d : it->second.first;
        };
        std::shared_ptr<const wa::Buffer> buffer;
        if (startAfterDecode_) {
            buffer = pre[li];
            if (!buffer) {
                continue;
            }
        } else {
            try {
                buffer = bufferFor(*bank, o.sound, layer);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "Sound %s/%d failed: %s\n", bank->name.c_str(), o.sound, e.what());
                continue;
            }
        }
        const int codec = patch.value("codec", 5);
        auto hw = allocate(codec == 5 ? "spu" : "iop", patch.value("channels", 1), T(0x06, 0));
        if (hw.empty()) {
            // 3BB588: a layer without voices stops the layers already started; the request fails
            for (auto& l : voice->layers_) {
                for (auto& h : l->hw) {
                    release(*h);
                }
                try {
                    l->source->stop();
                } catch (...) {
                }
                for (auto& s : l->lfo) {
                    try {
                        s->stop();
                    } catch (...) {
                    }
                }
                l->done = true;
                cutLayer(*l);
            }
            voice->layers_.clear();
            break;
        }
        auto source = context->createBufferSource();
        source->buffer = buffer;
        const int64_t loopStart = patch.value("loopStart", int64_t(-1));
        const int64_t loopEnd = patch.value("loopEnd", int64_t(-1));
        const double sampleRate = patch.value("sampleRate", 22050.0);
        if (loopStart >= 0 && loopEnd > loopStart) {
            source->loop = true;
            source->loopStart = double(loopStart) / sampleRate;
            source->loopEnd = double(loopEnd + 1) / sampleRate;
        }
        const double pv = std::trunc((std::min(std::max(T(0x0e, 127) + rndRange(int(T(0x0f, 0)), random), 0.0), 127.0) * o.velocity) / 127);
        const double cents = T(0x10, 0) + (o.note - T(0x07, 60)) * 100 + rndRange(int(T(0x11, 0)), random);
        const double wheelRange = T(0x0a, 0) * 100;
        const double panIndex = std::min(std::max(T(0x0c, 64) + rndRange(int(T(0x0d, 0)), random), 0.0), 127.0);
        const double patchPan = (std::sin((((panIndex - 64) / 64) * M_PI) / 2) * M_PI) / 2;
        source->detune.setValue(cents + (toInt32((o.wheel - 64) * wheelRange) >> 6));
        source->playbackRate.setValue(std::max(o.bend, 0.0) / 0x1000);
        auto g = context->createGain();
        g->gain.setValue(pv / 127);
        auto eg = context->createGain();
        double stopAt = -1;
        // envelope 0x19: {s32 ticks, s32 level} x count (tag 0x09), 100 Hz, start level tag 0x1C
        auto env = layer.tags.find(0x19);
        if (env != layer.tags.end()) {
            const auto& bnk = *bank->bnk;
            const int count = std::max(int(T(0x09, 1)), 1);
            const size_t base = size_t(int64_t(env->second.second) + int64_t(env->second.first));
            auto& p = eg->gain;
            double t = when;
            double level = T(0x1c, 127) / 127;
            p.setValueAtTime(level, when);
            bool held = false;
            for (int j = 0; j < count; ++j) {
                const int32_t ticks = i32le(bnk, base + 8 * size_t(j));
                const int32_t lv = i32le(bnk, base + 8 * size_t(j) + 4);
                if (ticks <= 0 || ticks >= 0x7fffffff) {
                    if (ticks <= 0) {
                        level = lv / 127.0;
                        p.setValueAtTime(level, t);
                        continue;
                    }
                    held = true;
                    break;
                }
                t += ticks / 100.0;
                level = lv / 127.0;
                p.linearRampToValueAtTime(level, t);
            }
            // every segment finite: the hard stop after the last one (3B55F0)
            bool finite = true;
            for (int j = 0; j < count; ++j) {
                finite = finite && i32le(bnk, base + 8 * size_t(j)) < 0x7fffffff;
            }
            (void)held;
            if (finite) {
                stopAt = t;
            }
        }
        source->connect(g);
        g->connect(eg);
        std::shared_ptr<wa::Node> node = eg;
        auto entry = std::make_shared<Voice::Layer>();
        auto table = [&](int tag, int lengthTag) -> std::vector<int> {
            auto it = layer.tags.find(tag);
            if (it == layer.tags.end()) {
                return {};
            }
            int n = int(T(lengthTag, 0)) & 0xff;
            if (!n) {
                n = 256;
            }
            const size_t base = size_t(int64_t(it->second.second) + int64_t(it->second.first));
            std::vector<int> values(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) {
                values[size_t(i)] = int8_t((*bank->bnk)[base + size_t(i)]);
            }
            return values;
        };
        const std::string lkey = bank->name + "/" + std::to_string(o.sound) + "/" + std::to_string(layer.index);
        const auto volTable = table(0x1d, 0x1e);
        if (!volTable.empty()) {
            // volume LFO: gain x tbl / 127, 10 ms a step from index 0
            auto lg = context->createGain();
            lg->gain.setValue(0);
            auto ls = context->createBufferSource();
            ls->buffer = lfoBuffer(lkey + "/v", volTable, [](int v) { return v / 127.0; });
            ls->loop = true;
            ls->connect(lg->gain);
            node->connect(lg);
            node = lg;
            ls->start(when);
            entry->lfo.push_back(ls);
            entry->lg = lg;
        }
        const auto pitchTable = table(0x20, 0x21);
        if (!pitchTable.empty()) {
            // pitch LFO: (tbl - 64) x depth >> 6 cents, random start when tag 0x23 != 0
            const int depth = int(T(0x22, 0));
            auto ps = context->createBufferSource();
            ps->buffer = lfoBuffer(lkey + "/p/" + std::to_string(depth), pitchTable, [depth](int v) { return double(((v - 64) * depth) >> 6); });
            ps->loop = true;
            const double offset = T(0x23, 0) != 0 ? std::floor(random() * double(pitchTable.size())) / 100 : 0;
            ps->connect(source->detune);
            ps->start(when, offset);
            entry->lfo.push_back(ps);
        }
        auto pan = context->createStereoPanner();
        node->connect(pan);
        pan->connect(voice->out_);
        voice->priority_ = std::max(voice->priority_, T(0x06, 0));
        entry->source = source;
        entry->pan = pan;
        entry->patchPan = patchPan;
        entry->cents = cents;
        entry->wheelRange = wheelRange;
        entry->hw = hw;
        entry->nodes = {source, g, eg, pan};
        if (entry->lg) {
            entry->nodes.push_back(entry->lg);
        }
        for (auto& h : hw) {
            h->layer = entry;
            h->voice = voice;
        }
        voice->layers_.push_back(entry);
        source->start(when);
        if (stopAt >= 0) {
            try {
                source->stop(stopAt);
            } catch (...) {
            }
        }
        std::weak_ptr<Voice::Layer> wl = entry;
        std::weak_ptr<Voice> wv = voice;
        source->onended = [this, wl, wv] {
            auto l = wl.lock();
            auto v = wv.lock();
            if (!l || !v) {
                return;
            }
            l->done = true;
            for (auto& h : l->hw) {
                release(*h);
            }
            bool all = true;
            for (const auto& x : v->layers_) {
                all = all && x->done;
            }
            if (all) {
                finish(*v);
            }
        };
    }
    if (voice->layers_.empty()) {
        voice->out_->disconnect();
        return nullptr;
    }
    if (logging_) {
        const Spatial s = spatial(voice->position_.get(), o.vanish);
        log_.push_back({std::round(context->currentTime() * 100) / 100, o.slot, o.sound, o.bus, o.volume, o.position ? std::round(s.g * 100) / 100 : 1.0,
                        voice->layers_.size()});
        if (log_.size() > 400) {
            log_.erase(log_.begin());
        }
    }
    update(*voice, true);
    voices_.insert(voice);
    return voice;
}

void Sfx::finish(Voice& v) {
    if (v.ended_) {
        return;
    }
    v.ended_ = true;
    auto self = v.shared_from_this();
    voices_.erase(self);
    for (auto& l : v.layers_) {
        for (auto& h : l->hw) {
            release(*h);
        }
    }
    for (auto& l : v.layers_) {
        for (auto& s : l->lfo) {
            try {
                s->stop();
            } catch (...) {
            }
        }
    }
    // 50 ms on: the whole chain lets go
    timers_.post(50, [this, self] {
        if (self->out_) {
            self->out_->disconnect();
        }
        for (auto& l : self->layers_) {
            cutLayer(*l);
        }
        self->layers_.clear();
        self->out_ = nullptr;
    });
}

void Sfx::cutLayer(Voice::Layer& l) {
    for (auto& n : l.nodes) {
        if (n) {
            n->disconnect();
        }
    }
    for (auto& s : l.lfo) {
        s->disconnect();
    }
}

void Sfx::fadeOut(Voice& v, double seconds) {
    if (v.ended_ || v.fading_ || !v.out_) {
        return;
    }
    v.fading_ = true;
    wa::Context* c = engine_.context();
    const double t = c->currentTime();
    auto& p = v.out_->gain;
    p.cancelScheduledValues(t);
    p.setValueAtTime(p.value(), t);
    p.linearRampToValueAtTime(0, t + seconds);
    for (auto& l : v.layers_) {
        try {
            l->source->stop(t + seconds + 0.01);
        } catch (...) {
        }
    }
    auto self = v.shared_from_this();
    timers_.post((seconds + 0.05) * 1000, [this, self] { finish(*self); });
}

void Sfx::update(Voice& v, bool immediate) {
    wa::Context* c = engine_.context();
    if (!c || v.fading_ || !v.out_) {
        return;
    }
    const auto* pos = v.posRef_ ? v.posRef_.get() : v.position_.get();
    const Spatial s = pos ? spatial(pos, v.vanish_) : Spatial{};
    const double vol = std::min(std::max(std::trunc(std::min(std::max(v.base_, 0.0), 127.0) * s.g), 0.0), 127.0);
    if (immediate || std::abs(vol - v.sent_) > 4) {
        // 2ABF60: changes of 4 or less are not sent
        v.sent_ = vol;
        if (immediate) {
            v.out_->gain.setValueAtTime(vol / 127, v.started_);
        } else {
            v.out_->gain.setTargetAtTime(vol / 127, c->currentTime(), 0.004);
        }
    }
    for (auto& l : v.layers_) {
        if (l->pan) {
            const double p = panValue(l->patchPan, pos ? s.az : 0);
            if (immediate) {
                l->pan->pan.setValue(p);
            } else {
                l->pan->pan.setTargetAtTime(p, c->currentTime(), 0.004);
            }
        }
    }
}

void Sfx::tick(double dt) {
    std::vector<std::shared_ptr<Voice>> list(voices_.begin(), voices_.end());
    for (const auto& v : list) {
        if (v->ended_ || v->paused_) {
            continue;
        }
        if (v->callback_ && dt > 0) {
            v->callback_(*v, dt);
        }
        if (v->posRef_ || v->position_) {
            // 2AC868: the doppler ratio 1.0 after the callback (positional voices)
            v->setBend(0x1000);
        }
        update(*v);
    }
}

void Sfx::pauseAll(bool on, const std::function<bool(const Voice&)>& filter) {
    wa::Context* c = engine_.context();
    const double t = c ? c->currentTime() : 0;
    for (const auto& v : voices_) {
        if ((!filter || filter(*v)) && !v->fading_ && v->paused_ != on && v->out_) {
            v->paused_ = on;
            for (auto& l : v->layers_) {
                l->source->playbackRate.setValueAtTime(on ? 0 : v->bend_ / 0x1000, t);
            }
            auto& g = v->out_->gain;
            g.cancelScheduledValues(t);
            g.setValueAtTime(g.value(), t);
            if (on) {
                g.setTargetAtTime(0, t, 0.002);
            } else {
                v->sent_ = -1;
                update(*v, false);
            }
        }
    }
}

void Sfx::stopAll(const StopFilter& f) {
    std::vector<std::shared_ptr<Voice>> list(voices_.begin(), voices_.end());
    for (const auto& v : list) {
        if ((!f.slot || v->slot == *f.slot) && (!f.tag || v->tag == *f.tag) && (!f.owner || v->owner == f.owner) && !(f.keep && f.keep(*v))) {
            fadeOut(*v, f.fade);
        }
    }
}

// ---- the handle ----

void Voice::setVolume(double v) {
    if (deferred_) {
        deferredBase_ = v;
        if (real_) {
            real_->setVolume(v);
        }
        return;
    }
    base_ = v;
}

void Voice::setBend(double b) {
    if (deferred_) {
        if (real_) {
            real_->setBend(b);
        }
        return;
    }
    // 2ABE78: clamped 0..0x3FFF, changes <= 0xFF ignored
    b = std::min(std::max(double(int32_t(b)), 0.0), double(0x3fff));
    if (std::abs(b - bend_) <= 0xff) {
        return;
    }
    bend_ = b;
    wa::Context* c = sfx_->engine_.context();
    for (auto& l : layers_) {
        l->source->playbackRate.setValueAtTime(b / 0x1000, c->currentTime());
    }
}

void Voice::setWheel(double b) {
    if (deferred_) {
        if (real_) {
            real_->setWheel(b);
        }
        return;
    }
    // 3BC6B8: the bend-wheel byte on every layer
    const int w = int(std::min(std::max(double(int32_t(b)), 0.0), 127.0));
    if (w == wheel_) {
        return;
    }
    wheel_ = w;
    wa::Context* c = sfx_->engine_.context();
    for (auto& l : layers_) {
        if (l->wheelRange != 0) {
            l->source->detune.setValueAtTime(l->cents + (toInt32((w - 64) * l->wheelRange) >> 6), c->currentTime());
        }
    }
}

void Voice::stop(double seconds) {
    if (deferred_) {
        cancelled_ = true;
        if (real_) {
            real_->stop(seconds);
        }
        return;
    }
    sfx_->fadeOut(*this, seconds <= 0 ? 0.25 : seconds);
}

bool Voice::playing() const {
    if (deferred_) {
        return !cancelled_ && (!real_ || real_->playing());
    }
    return !ended_ && !fading_;
}

}  // namespace audio

}  // namespace ssx_native
