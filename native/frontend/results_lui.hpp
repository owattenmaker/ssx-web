#pragma once
// The in-race cards and results from the original OV.LUI records (web/results-lui.js ResultsLui, 1:1; UI/results-screens.json):
// OV_darkblue (the panel: its open playback 0..60, then the header lights' loop 65..499 from tick 50), 43final_standings (the
// results), 61toptimes (the records), 40race_pre (the race card) on the UI canvas, each content screen at its frame-30 layout plus
// one focus frame.
#include "lui_screen.hpp"
#include <memory>

namespace ssx_native {

namespace fe {

class ResultsLui {
public:
    static constexpr double kSy = 448.0 / 480;

    bool load(const nlohmann::json& data, hud::HudImages& images, hud::UiText& text) {
        data_ = data;
        if (!data_.is_object() || !data_.contains("screens")) {
            return false;
        }
        for (auto it = data_["screens"].begin(); it != data_["screens"].end(); ++it) {
            auto lui = std::make_unique<LuiScreen>(it.value(), images, text);
            lui->shapeScale = true;
            lui->unionFlat = true;
            lui->keepLead = true;
            lui->flagWrap = true;
            lui->ps2Wrap = true;
            lui_[it.key()] = std::move(lui);
        }
        return ready();
    }
    bool ready() const {
        return lui_.count("OV_darkblue") && lui_.count("43final_standings");
    }
    void setKeyboard(bool on) {
        for (auto& [k, l] : lui_) {
            l->keyboard = on;
        }
    }
    // clock(): frames since the panel opened (a panel not drawn for 15 frames opens again)
    double clock(double nowFrames) {
        if (nowFrames - lastDraw_ > 15) {
            openAt_ = nowFrames;
        }
        lastDraw_ = nowFrames;
        return nowFrames - openAt_;
    }

    // OV_darkblue on its own timeline (panelTimeline) with the header lights (their vertex colours alone: alpha override 255)
    void panelLights(HudCanvas& c, double t) {
        LuiScreen& lui = *lui_.at("OV_darkblue");
        static const std::set<std::string> lights = {"0760d5c2", "0760d5c3", "0760d5c4", "0760d5c5", "0760d5c6", "0760d5c7"};
        const bool byName = lui.screen().value("state0", "") == "byName";
        const auto events = panelTimeline(lui.screen()["events"], t);
        c.save();
        c.scale(1, kSy);
        lui.draw(c, events, t, [&](const nlohmann::json& e) -> std::optional<LuiScreen::Override> {
            const std::string n = e.value("name", "");
            if (lights.count(n)) {
                LuiScreen::Override o;
                o.alpha = 255;
                return o;
            }
            if (!byName && (n == "06c3ae11" || n == "06c3ae12")) {
                LuiScreen::Override o;
                o.props = n == "06c3ae11" ? Props{{0, 433}, {1, 57}} : Props{{0, 449}, {1, 63}};
                return o;
            }
            return std::nullopt;
        });
        c.restore();
    }
    // any content screen at its frame-30 layout plus one focus frame
    bool screen(HudCanvas& c, const std::string& key, double focus, const LuiScreen::OverrideFn& override) {
        auto it = lui_.find(key);
        if (it == lui_.end()) {
            return false;
        }
        std::vector<LuiScreen::Event> events;
        for (const auto& ev : it->second->screen()["events"]) {
            const double f = ev.value("frame", -1.0);
            if (f == 30 || f == focus) {
                events.push_back({&ev, f});
            }
        }
        c.save();
        c.scale(1, kSy);
        it->second->draw(c, events, 1000, override);
        c.restore();
        return true;
    }

    struct Row {
        int rank = 0;
        std::string name, value;
        bool human = false;
    };
    struct Medal {
        int row = -1;
        int kind = -1;
    };
    struct Results {
        std::string title, sub;
        std::array<std::string, 3> heads;
        std::vector<Row> rows;
        std::optional<Medal> medal;
        std::string message;
        std::vector<std::string> items;
        int index = 0;
        std::function<bool(int)> disabled;
        bool noItems = false;
    };
    // 43final_standings: heads, rows in rank order, the medal of the human's row, the message, the menu and its focus
    void results(HudCanvas& c, double t, const Results& r) {
        panelLights(c, t);
        static const std::string track = "01abc714", titleEl = "007b0b25", help = "0c37a134", dataEl = "0bcd9da1", chat = "088e0500", ps2x = "00077698";
        static const std::vector<std::string> items = {"0263fe50", "0263fe51", "0263fe52", "0263fe53", "0263fe54"};
        static const std::vector<std::string> medals = {"0006e624", "07a03cc2", "06996605"};
        static const std::vector<std::string> ranks = {"007884e1", "007884e2", "007884e3", "007884e4", "007884e5", "007884e6"};
        static const std::vector<std::string> riders = {"078fac51", "078fac52", "078fac53", "078fac54", "078fac55", "078fac56"};
        static const std::vector<std::string> values = {"07aa5c21", "07aa5c22", "07aa5c23", "07aa5c24", "07aa5c25", "07aa5c26"};
        static const double medalSy[4] = {120.5, 0.5, 40.5, 80.5};
        const double focus = 40 + 10 * std::max(0, std::min(4, r.index));
        auto find = [](const std::vector<std::string>& l, const std::string& n) {
            auto it = std::find(l.begin(), l.end(), n);
            return it == l.end() ? -1 : int(it - l.begin());
        };
        auto textOf = [](const std::string& s) {
            LuiScreen::Override o;
            o.text = s;
            return o;
        };
        auto hiddenO = [] {
            LuiScreen::Override o;
            o.hidden = true;
            return o;
        };
        screen(c, "43final_standings", focus, [&](const nlohmann::json& e) -> std::optional<LuiScreen::Override> {
            const std::string n = e.value("name", "");
            if (n == track) {
                return textOf(r.title);
            }
            if (n == titleEl) {
                return textOf(r.sub);
            }
            if (n == dataEl) {
                LuiScreen::Override o;
                o.alpha = 255;
                return o;
            }
            if (n == chat) {
                return hiddenO();
            }
            if (n == help) {
                return r.message.empty() ? hiddenO() : textOf(r.message);
            }
            if (n == "0007884b") {
                return textOf(r.heads[0]);
            }
            if (n == "078fac93") {
                return textOf(r.heads[1]);
            }
            if (n == "061a88b5") {
                return textOf(r.heads[2]);
            }
            int k = find(ranks, n);
            if (k >= 0) {
                return size_t(k) < r.rows.size() ? textOf(std::to_string(r.rows[size_t(k)].rank)) : hiddenO();
            }
            for (int col = 0; col < 2; ++col) {
                k = find(col == 0 ? riders : values, n);
                if (k >= 0) {
                    if (size_t(k) >= r.rows.size()) {
                        return hiddenO();
                    }
                    const Row& row = r.rows[size_t(k)];
                    LuiScreen::Override o = textOf(col == 0 ? row.name : row.value);
                    if (row.human) {
                        o.props = {{14, 255}, {15, 204}, {16, 153}};
                    }
                    return o;
                }
            }
            k = find(medals, n);
            if (k >= 0) {
                if (k != 0 || !r.medal || r.medal->kind < 0 || r.medal->row < 0) {
                    return hiddenO();
                }
                LuiScreen::Override o;
                nlohmann::json sp = e["sprite"];
                sp["sy"] = r.medal->kind >= 0 && r.medal->kind < 4 ? medalSy[r.medal->kind] : sp.value("sy", 0.0);
                o.sprite = sp;
                o.props = {{1, -8.0 + 20 * r.medal->row}};
                return o;
            }
            k = find(items, n);
            if (k >= 0) {
                if (r.noItems || size_t(k) >= r.items.size()) {
                    return hiddenO();
                }
                LuiScreen::Override o = textOf(r.items[size_t(k)]);
                if (r.disabled && r.disabled(k) && k != r.index) {
                    o.alpha = 128;
                }
                return o;
            }
            if (n == ps2x || n == "069d01a0") {
                if (r.noItems || size_t(r.index) >= r.items.size()) {
                    return hiddenO();
                }
            }
            return std::nullopt;
        });
    }

    struct Record {
        std::optional<int> rank;
        std::string name, rider, value;
        bool player = false;
        bool focus = false;
    };
    struct Records {
        std::string title;
        bool timed = true;
        std::vector<Record> rows;
        std::optional<std::string> message;
        std::vector<std::string> items;
        int index = 0;
        std::function<bool(int)> disabled;
        std::optional<std::string> subtitle;
    };
    // 61toptimes: title, Top 5 Record Times / Scores, rows (5), the new-record message, Continue / Save Records (or Return)
    bool records(HudCanvas& c, double t, const Records& r) {
        if (!lui_.count("61toptimes")) {
            return false;
        }
        panelLights(c, t);
        static const std::vector<std::string> itemNames = {"065b08f5", "084d7133"};
        static const std::vector<std::vector<std::string>> cols = {{"00000331", "00000332", "00000333", "00000334", "00000335"},
                                                                   {"07483831", "07483832", "07483833", "07483834", "07483835"},
                                                                   {"07e0ab31", "07e0ab32", "07e0ab33", "07e0ab34", "07e0ab35"},
                                                                   {"07b03831", "07b03832", "07b03833", "07b03834", "07b03835"}};
        auto find = [](const std::vector<std::string>& l, const std::string& n) {
            auto it = std::find(l.begin(), l.end(), n);
            return it == l.end() ? -1 : int(it - l.begin());
        };
        auto colour = [](int rr, int g, int b) { return Props{{14, double(rr)}, {15, double(g)}, {16, double(b)}}; };
        return screen(c, "61toptimes", r.index == 1 ? 45 : 40, [&](const nlohmann::json& e) -> std::optional<LuiScreen::Override> {
            const std::string n = e.value("name", "");
            LuiScreen::Override o;
            if (n == "07a198f8") {
                o.text = r.title;
                return o;
            }
            if (n == "0c850ae4" || n == "0c9904a3") {
                const bool times = n == "0c850ae4";
                if (r.timed != times) {
                    o.hidden = true;
                    return o;
                }
                if (r.subtitle) {
                    o.text = *r.subtitle;
                    return o;
                }
                return std::nullopt;
            }
            if (n == "0007b035") {
                if (r.timed) {
                    return std::nullopt;
                }
                o.hidden = true;
                return o;
            }
            if (n == "077605b3") {
                if (!r.timed) {
                    return std::nullopt;
                }
                o.hidden = true;
                return o;
            }
            if (n == "07a12f51" || n == "07a12f52") {
                const bool forTimes = n == "07a12f51";
                if (r.message && r.timed == forTimes) {
                    o.text = *r.message;
                    return o;
                }
                o.hidden = true;
                return o;
            }
            if (n == "0f55f740") {
                if (r.items.empty()) {
                    o.hidden = true;
                } else {
                    o.alpha = 255;
                }
                return o;
            }
            int k = find(itemNames, n);
            if (k >= 0) {
                if (size_t(k) >= r.items.size()) {
                    o.hidden = true;
                    return o;
                }
                o.text = r.items[size_t(k)];
                o.props = k == r.index ? colour(255, 255, 255) : colour(101, 184, 201);
                if (r.disabled && r.disabled(k) && k != r.index) {
                    o.alpha = 128;
                }
                return o;
            }
            for (size_t col = 0; col < cols.size(); ++col) {
                k = find(cols[col], n);
                if (k < 0) {
                    continue;
                }
                if (size_t(k) >= r.rows.size()) {
                    o.hidden = true;
                    return o;
                }
                const Record& row = r.rows[size_t(k)];
                if (col == 0) {
                    if (row.rank) {
                        o.text = std::to_string(*row.rank);
                    }
                } else {
                    o.text = col == 1 ? row.name : col == 2 ? row.rider : row.value;
                }
                if (row.focus) {
                    o.props = colour(255, 255, 255);
                } else if (row.player) {
                    o.props = colour(250, 135, 18);
                }
                return o;
            }
            return std::nullopt;
        });
    }

    struct CardRider {
        std::string name;
        bool human = false;
        bool note = false;
    };
    struct RaceCard {
        std::string title, sub;
        std::optional<int> round;
        std::string objective;
        std::vector<CardRider> riders;
        std::string recordLabel;
        std::optional<std::string> record;
        std::string continueLabel;
    };
    // 40race_pre: the race round card (a Single Event sets 'title'; a career round its tab)
    bool raceCard(HudCanvas& c, double t, const RaceCard& r) {
        if (!lui_.count("40race_pre")) {
            return false;
        }
        panelLights(c, t);
        static const std::vector<std::string> tabs = {"085c309c", "04df1f42", "070c569c"};
        static const std::vector<std::string> rows = {"078fac51", "078fac52", "078fac53", "078fac54", "078fac55", "078fac56"};
        const std::string tab = !r.round ? std::string() : *r.round == 3 ? tabs[0] : *r.round == 2 ? tabs[2] : tabs[1];
        return screen(c, "40race_pre", 30, [&](const nlohmann::json& e) -> std::optional<LuiScreen::Override> {
            const std::string n = e.value("name", "");
            LuiScreen::Override o;
            if (n == "01abc714") {
                o.text = r.title;
                return o;
            }
            if (n == "007b0b25") {
                if (!tab.empty()) {
                    o.hidden = true;
                } else {
                    o.text = r.sub;
                }
                return o;
            }
            if (std::find(tabs.begin(), tabs.end(), n) != tabs.end()) {
                if (n == tab) {
                    return std::nullopt;
                }
                o.hidden = true;
                return o;
            }
            if (n == "01233101") {
                if (r.objective.empty()) {
                    o.hidden = true;
                } else {
                    o.text = r.objective;
                }
                return o;
            }
            if (n == "069c32c4") {
                if (!r.objective.empty()) {
                    return std::nullopt;
                }
                o.hidden = true;
                return o;
            }
            if (n == "0d0ecebc" || n == "0b65b0d5") {
                if (!r.record) {
                    o.hidden = true;
                } else {
                    o.text = n == "0d0ecebc" ? r.recordLabel : *r.record;
                }
                return o;
            }
            if (n == "0263fe50") {
                o.text = r.continueLabel;
                return o;
            }
            auto it = std::find(rows.begin(), rows.end(), n);
            if (it != rows.end()) {
                const size_t k = size_t(it - rows.begin());
                if (k >= r.riders.size()) {
                    o.hidden = true;
                    return o;
                }
                o.text = r.riders[k].name;
                if (r.riders[k].human) {
                    o.props = {{14, 252}, {15, 177}, {16, 101}};
                }
                if (r.riders[k].note) {
                    o.alpha = 128;
                    o.props[6] = 400;
                }
                return o;
            }
            return std::nullopt;
        });
    }

    // panelTimeline: the open playback 0..60, then the loop 65..500 from tick 50 (436 frames a turn), each event on t's clock
    static std::vector<LuiScreen::Event> panelTimeline(const nlohmann::json& events, double t) {
        const double mainStop = 60, loopAt = 50, loopFrom = 65, loopTo = 500, turnLen = loopTo - loopFrom + 1;
        std::vector<LuiScreen::Event> out;
        for (const auto& ev : events) {
            const double f = ev.value("frame", 0.0);
            if (f <= mainStop && f <= t) {
                out.push_back({&ev, f});
            }
        }
        if (t >= loopAt) {
            const double turn = std::floor((t - loopAt) / turnLen);
            const double at = loopFrom + (t - loopAt) - turn * turnLen;
            auto tick = [&](double frame, double k) { return loopAt + (frame - loopFrom) + k * turnLen; };
            std::vector<const nlohmann::json*> loop;
            for (const auto& ev : events) {
                const double f = ev.value("frame", 0.0);
                if (f >= loopFrom && f < loopTo) {
                    loop.push_back(&ev);
                }
            }
            if (turn > 0) {
                for (const auto* ev : loop) {
                    if (ev->value("frame", 0.0) > at) {
                        out.push_back({ev, tick(ev->value("frame", 0.0), turn - 1)});
                    }
                }
            }
            for (const auto* ev : loop) {
                if (ev->value("frame", 0.0) <= at) {
                    out.push_back({ev, tick(ev->value("frame", 0.0), turn)});
                }
            }
        }
        std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
        return out;
    }

private:
    nlohmann::json data_;
    std::map<std::string, std::unique_ptr<LuiScreen>> lui_;
    double openAt_ = 0;
    double lastDraw_ = -1e9;
};

}  // namespace fe

}  // namespace ssx_native
