#pragma once
// What the page's audio modules get from the browser, natively: the asset files (fetch of /assets/AUDIO/..., cached as the
// director's json() / bytes() caches; read synchronously, so a JS `await` on them resolves at once), the timer queue
// (setTimeout: ordered by due time, then by posting order; run on the game thread), the clock (performance.now) and Math.random.
#include "json.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx_native {

namespace audio {

using Bytes = std::shared_ptr<const std::vector<uint8_t>>;
using Json = nlohmann::ordered_json;
using Position = std::shared_ptr<std::array<double, 3>>;

inline Position position(double x, double y, double z) {
    return std::make_shared<std::array<double, 3>>(std::array<double, 3>{x, y, z});
}


// /assets/AUDIO/<path>
class Files {
public:
    explicit Files(std::string root) : root_(std::move(root)) {}
    const std::string& root() const {
        return root_;
    }
    bool exists(const std::string& path) const {
        std::ifstream f(root_ + "/" + path, std::ios::binary);
        return bool(f);
    }
    std::shared_ptr<const Json> json(const std::string& path) {
        auto it = json_.find(path);
        if (it != json_.end()) {
            return it->second;
        }
        auto b = bytes(path);
        auto j = std::make_shared<const Json>(Json::parse(std::string(b->begin(), b->end())));
        json_[path] = j;
        return j;
    }
    Bytes bytes(const std::string& path) {
        auto it = bytes_.find(path);
        if (it != bytes_.end()) {
            return it->second;
        }
        std::ifstream f(root_ + "/" + path, std::ios::binary | std::ios::ate);
        if (!f) {
            throw std::runtime_error(path + ": 404");
        }
        // one read of the whole file
        const std::streamoff size = f.tellg();
        f.seekg(0);
        auto v = std::make_shared<std::vector<uint8_t>>(size_t(std::max<std::streamoff>(0, size)));
        if (!v->empty()) {
            f.read(reinterpret_cast<char*>(v->data()), std::streamsize(v->size()));
        }
        bytes_[path] = v;
        return v;
    }
    // the director's byte-cache trim (leaveWorld: songs only while in use)
    void dropBytes(const std::string& prefix) {
        for (auto it = bytes_.begin(); it != bytes_.end();) {
            if (it->first.rfind(prefix, 0) == 0) {
                it = bytes_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::string root_;
    std::map<std::string, std::shared_ptr<const Json>> json_;
    std::map<std::string, Bytes> bytes_;
};

// a JS Map: insertion-ordered (iteration order matters where each entry draws randoms or starts voices)
template <class K, class V>
class OrderedMap {
public:
    using Item = std::pair<K, V>;
    V* get(const K& k) {
        for (auto& i : items_) {
            if (i.first == k) {
                return &i.second;
            }
        }
        return nullptr;
    }
    const V* get(const K& k) const {
        for (const auto& i : items_) {
            if (i.first == k) {
                return &i.second;
            }
        }
        return nullptr;
    }
    bool has(const K& k) const {
        return get(k) != nullptr;
    }
    // set: a new key goes last, an existing one keeps its place
    V& set(const K& k, V v) {
        if (V* x = get(k)) {
            *x = std::move(v);
            return *x;
        }
        items_.emplace_back(k, std::move(v));
        return items_.back().second;
    }
    bool erase(const K& k) {
        for (auto it = items_.begin(); it != items_.end(); ++it) {
            if (it->first == k) {
                items_.erase(it);
                return true;
            }
        }
        return false;
    }
    void clear() {
        items_.clear();
    }
    size_t size() const {
        return items_.size();
    }
    bool empty() const {
        return items_.empty();
    }
    std::vector<Item>& items() {
        return items_;
    }
    const std::vector<Item>& items() const {
        return items_;
    }
    typename std::vector<Item>::iterator begin() {
        return items_.begin();
    }
    typename std::vector<Item>::iterator end() {
        return items_.end();
    }
    typename std::vector<Item>::const_iterator begin() const {
        return items_.begin();
    }
    typename std::vector<Item>::const_iterator end() const {
        return items_.end();
    }

private:
    std::vector<Item> items_;
};

// performance.now() in ms
inline double nowMs() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return duration<double, std::milli>(steady_clock::now() - t0).count();
}

// setTimeout / clearTimeout
class Timers {
public:
    using Id = uint64_t;
    Id post(double ms, std::function<void()> fn) {
        const Id id = ++next_;
        list_.push_back({id, nowMs() + std::max(0.0, ms), std::move(fn)});
        return id;
    }
    void clear(Id id) {
        for (auto& t : list_) {
            if (t.id == id) {
                t.fn = nullptr;
            }
        }
    }
    // every due timer, in due order (posting order on ties); timers posted while running wait for the next run
    void run() {
        const double t = nowMs();
        std::vector<Timer> due;
        for (auto it = list_.begin(); it != list_.end();) {
            if (it->at <= t) {
                due.push_back(std::move(*it));
                it = list_.erase(it);
            } else {
                ++it;
            }
        }
        std::stable_sort(due.begin(), due.end(), [](const Timer& a, const Timer& b) { return a.at < b.at; });
        for (auto& d : due) {
            if (d.fn) {
                d.fn();
            }
        }
    }

private:
    struct Timer {
        Id id;
        double at;
        std::function<void()> fn;
    };
    std::vector<Timer> list_;
    Id next_ = 0;
};

// Math.random; lcg(seed): a reproducible stream for the parity checks (x = x x 1103515245 + 12345 mod 2^31, / 2^31: the page's
// tests pass the same generator as `random`)
class Random {
public:
    Random() : gen_(std::random_device{}()) {}
    static Random lcg(uint32_t seed) {
        Random r;
        r.lcg_ = true;
        r.state_ = seed & 0x7fffffffu;
        return r;
    }
    double operator()() {
        if (lcg_) {
            state_ = uint32_t((uint64_t(state_) * 1103515245u + 12345u) & 0x7fffffffu);
            return double(state_) / 2147483648.0;
        }
        return std::uniform_real_distribution<double>(0, 1)(gen_);
    }

private:
    std::mt19937 gen_;
    bool lcg_ = false;
    uint32_t state_ = 0;
};

}  // namespace audio

}  // namespace ssx_native
