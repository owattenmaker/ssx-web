// Lit instances' light-cache rows (engine/lit_instance_lighting.hpp): the bank the cache's painter wrapper gives at each position
// (2F5400: Lighting reference 3 of the payload at x, y (vt+0x140), else reference 0 (vt+0x128), else the course default gp+0x12D4;
// engine/painter_tree.hpp for the payload) plus up to 4 local lights.
// lit_instance_rows CATALOG.json TREE.json INPUT.json > OUT.json
// INPUT: {"painter": {"tree": {...}, "entries": [{"references": [4 names]}], "default_reference": name} | null,
//         "banks": {name: [[r, g, b, a] x 10]}, "queries": [{"id", "position": [x, y, z], "bank"?: rows}]}
// OUT: [{"id", "bank": name, "rows": [[...] x 10], "lights": [4 ids]}]
#include "lit_instance_lighting.hpp"
#include "painter_driver.hpp"
#include "painter_tree.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using json = nlohmann::json;
static json load(const char* path) { std::ifstream f(path); if (!f) throw std::runtime_error(std::string("Cannot read ") + path); return json::parse(f); }
int main(int argc, char** argv) {
  try {
    if (argc != 4) { std::cerr << "usage: lit_instance_rows CATALOG TREE INPUT\n"; return 2; }
    const auto world = ssx::originalLitInstanceLightAsset(load(argv[1]), load(argv[2]));
    const json input = load(argv[3]);
    const json& painter = input.at("painter");
    std::vector<std::array<uint16_t, 4>> nodes; ssx::OriginalPainterTree tree; uint32_t outside = 0xffffffffu;
    std::vector<std::array<std::string, 4>> entries; std::string fallback;
    if (!painter.is_null()) {
      const auto& t = painter.at("tree"); nodes = t.at("nodes").get<std::vector<std::array<uint16_t, 4>>>();
      const auto origin = t.at("origin").get<std::array<float, 2>>();
      tree = {t.at("scale").get<float>(), origin[0], origin[1], uint16_t(t.at("root").get<unsigned>()), nodes};
      outside = t.at("outside_words").get<std::array<uint32_t, 2>>()[1];
      for (const auto& e : painter.at("entries")) entries.push_back(e.at("references").get<std::array<std::string, 4>>());
      fallback = painter.at("default_reference").get<std::string>();
    }
    const json& banks = input.at("banks");
    json out = json::array();
    for (const auto& q : input.at("queries")) {
      const auto position = q.at("position").get<std::array<float, 3>>();
      std::string name;
      ssx::OriginalIrradianceCoefficients bank;
      if (q.contains("bank")) { bank = q.at("bank").get<ssx::OriginalIrradianceCoefficients>(); name = "(given)"; }
      else {
        if (painter.is_null()) throw std::runtime_error("No Lighting painter for a derived bank");
        const auto payload = ssx::originalPainterPayload(tree, position[0], position[1], outside, entries.size());
        if (payload) { const auto& refs = entries[*payload]; name = !refs[3].empty() ? refs[3] : !refs[0].empty() ? refs[0] : fallback; }
        else name = fallback;                                   // 2BE1F8 reset: empty references -> gp+0x12D4
        if (!banks.contains(name)) throw std::runtime_error("No irradiance bank " + name);
        bank = banks.at(name).get<ssx::OriginalIrradianceCoefficients>();
      }
      std::array<uint32_t, 4> chosen{};
      const auto rows = world.rows(bank, position, &chosen);
      out.push_back({{"id", q.at("id")}, {"bank", name}, {"rows", rows}, {"lights", chosen}});
    }
    std::cout << out.dump() << "\n";
  } catch (const std::exception& e) { std::cerr << e.what() << "\n"; return 1; }
}
