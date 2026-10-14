#pragma once
// Rider morph parts (pv boardFlex, web/board-flex.js natively; docs/characters.md "Board flex"): the board (board_BoardFlex<X>, part
// file 2, 8 morphs), the race hands (HandsX, file 7, 18 morphs) and Stretch's SpecialA (file 46). The package's <stem>.json / .bin
// hold the deltas; the core blends the weights (30F2B0, board_morph_weights) once its parts are configured after init_animation;
// the renderer adds sum(w_i x delta_i) to the part's vertices before skinning (native/frontend/rider_renderer.cpp).
#include "json.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
int morph_part_add(int file, int slotBit, int count, const uint8_t* mirror);
void board_morph_configure(int slotBit, int count, const uint8_t* mirror);
int morph_upper_bit(int slotCount);
int board_morph_slot();
int board_morph_count();
float* board_morph_weights();
}

namespace ssx_native {

namespace flex {

// the renderer's weight columns (the board's 8, the hands' 18, SpecialA's 1)
constexpr int kColumns = 27;

struct Part {
    int file = 0;
    int column = 0;
    int morphCount = 0;
    int vertexCount = 0;
    int firstVertex = 0;
    int slotBit = -1;
    std::vector<uint8_t> mirror;
    std::vector<uint32_t> offsets;
    std::vector<float> deltas;
};
struct Layout {
    int column = 0;
    int count = 0;
};
struct Flex {
    std::vector<Part> parts;
    int slots = -1;
    std::vector<Layout> layout;
};

inline std::string readText(const std::string& path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    ok = bool(f);
    return ok ? std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()) : std::string();
}

// loadBoardFlex: the package's morph parts (none: the rest shape). root: the package folder (with a trailing slash); rig: rider.json
inline Flex load(const std::string& root, const nlohmann::json& rig) {
    struct Column {
        const char* stem;
        int file, column, count;
        std::regex part;
    };
    static const std::vector<Column> columns = {
        {"board-flex", 2, 0, 8, std::regex("BoardFlex", std::regex::icase)},
        {"hand-morphs", 7, 8, 18, std::regex("^([a-z]+_)?Hands[A-Z]?(\\.mnf)?$", std::regex::icase)},
        {"special-morphs", 46, 26, 1, std::regex("^([a-z]+_)?SpecialA(\\.mnf)?$", std::regex::icase)},
    };
    Flex out;
    out.slots = rig.contains("source_bone_slot_count") && rig["source_bone_slot_count"].is_number_integer() ? rig["source_bone_slot_count"].get<int>() : -1;
    const nlohmann::json parts = rig.value("parts", nlohmann::json::array());
    for (const auto& c : columns) {
        const nlohmann::json* part = nullptr;
        for (const auto& p : parts) {
            const std::string name = p.contains("resource") && p["resource"].is_string() ? p["resource"].get<std::string>()
                                     : p.contains("part") && p["part"].is_string()      ? p["part"].get<std::string>()
                                                                                         : std::string();
            if (std::regex_search(name, c.part)) {
                part = &p;
                break;
            }
        }
        if (!part || !((*part).value("morph_count", 0) > 0)) {
            continue;
        }
        bool ok = false;
        const std::string metaText = readText(root + c.stem + ".json", ok);
        if (!ok) {
            continue;
        }
        const std::string bin = readText(root + c.stem + ".bin", ok);
        if (!ok) {
            continue;
        }
        const auto meta = nlohmann::json::parse(metaText);
        if (meta.value("version", 0) != 1 || !(meta.value("morph_count", 0) > 0) || !(meta.value("vertex_count", 0) > 0)) {
            continue;
        }
        Part p;
        p.file = meta["file"].get<int>();
        p.morphCount = meta["morph_count"].get<int>();
        p.vertexCount = meta["vertex_count"].get<int>();
        p.firstVertex = meta["first_vertex"].get<int>();
        if (bin.size() != size_t(p.morphCount) * p.vertexCount * 12) {
            throw std::runtime_error(root + c.stem + ".bin: unexpected size");
        }
        if (meta["mirror"].size() != size_t(p.morphCount)) {
            throw std::runtime_error(root + c.stem + ".json: mirror table");
        }
        if (p.file != c.file || p.morphCount > c.count) {
            continue;
        }
        p.column = c.column;
        p.slotBit = meta.contains("slot_bit") && meta["slot_bit"].is_number_integer() ? meta["slot_bit"].get<int>() : -1;
        for (const auto& m : meta["mirror"]) {
            p.mirror.push_back(uint8_t(m.get<int>()));
        }
        p.offsets = meta["offsets"].get<std::vector<uint32_t>>();
        p.deltas.resize(bin.size() / 4);
        std::memcpy(p.deltas.data(), bin.data(), bin.size());
        out.parts.push_back(std::move(p));
    }
    return out;
}

// configureBoardFlex (after init_animation, in the rider's context): the board first (it clears the parts), then the others in file
// order; the layout is the core's weight order
inline bool configure(Flex& f) {
    if (f.parts.empty() || f.slots < 0) {
        return false;
    }
    f.layout.clear();
    const Part* board = nullptr;
    for (const auto& p : f.parts) {
        if (p.file == 2) {
            board = &p;
        }
    }
    const std::vector<uint8_t> none = {0};
    const auto& boardMirror = board ? board->mirror : none;
    const int boardBit = board && board->slotBit >= 0 ? board->slotBit : f.slots;
    board_morph_configure(boardBit, board ? int(boardMirror.size()) : 0, boardMirror.data());
    if (board) {
        f.layout.push_back({board->column, board->morphCount});
    }
    for (const auto& p : f.parts) {
        if (p.file == 2) {
            continue;
        }
        int bit = p.slotBit;
        if (bit < 0 && p.file == 7) {
            bit = morph_upper_bit(f.slots);
        }
        if (bit < 0) {
            continue;
        }
        if (morph_part_add(p.file, bit, p.morphCount, p.mirror.data())) {
            f.layout.push_back({p.column, p.morphCount});
        }
    }
    return !f.layout.empty();
}

// boardFlexUnconfigured: a fresh init_animation cleared the parts
inline bool unconfigured() {
    return board_morph_slot() < 0;
}

// the drawn pose's weights in the renderer's columns (27), from the core's (in the layout's order)
inline std::vector<float> weights(const Flex& f) {
    std::vector<float> out(kColumns, 0.f);
    int total = 0;
    for (const auto& l : f.layout) {
        total += l.count;
    }
    if (!total || board_morph_count() != total) {
        return out;
    }
    const float* live = board_morph_weights();
    if (!live) {
        return out;
    }
    int at = 0;
    for (const auto& l : f.layout) {
        for (int k = 0; k < l.count; ++k) {
            out[size_t(l.column + k)] = live[at + k];
        }
        at += l.count;
    }
    return out;
}

}  // namespace flex

}  // namespace ssx_native
