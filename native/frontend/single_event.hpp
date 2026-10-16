#pragma once
// A Single Event's results (web/career.js raceResult / heatRecord / addRecord / completeEvent, web/career-ui.js finish, drawResults,
// drawRecords, resultItems, items / disabled / key / choose / back for ctm-records and ctm-results): the round's times, ranks, medal and
// top-5 record through the core's result kernel (engine/career_rules.hpp, the page's pv coreCareerResults), the records table as the
// save keeps it, and the two screens' menus with the PS2's input rules (web/menu-rules.js: the 31-frame intro lock, the wrapping menus
// that skip greyed items, web/screen-phases.js's frame counter).
// The page's online boards (pv onlineRecords: the server's top 5 decides and the records screen shows the board) are not here: native
// keeps the PS2's local table, the page's onlineRecords-off path.
#include "career_data.hpp"
#include "career_rules.hpp"
#include "ctm_pda.hpp"
#include "results_lui.hpp"
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace ssx_native {

namespace fe {

// web/career.js PLAYER_NAME: player 0's name (0x534FE0, "PLAYER %d" with 1), the records' name
inline const char* kPlayerName = "PLAYER 1";

// web/career.js RIDER_CHARACTER: a rider id's character (records' "With" column); unknown ids are Mac's (3)
inline int riderCharacter(const std::string& id) {
    static const std::vector<std::pair<std::string, int>> table = {{"moby", 0}, {"kaori", 1}, {"allegra", 2}, {"mac", 3}, {"sam", 3}, {"zoe", 4},
                                                                   {"griff", 5}, {"elise", 6}, {"nate", 7}, {"psymon", 8}, {"viggo", 9}};
    for (const auto& [k, v] : table) {
        if (k == id) {
            return v;
        }
    }
    return 3;
}

// one entry of the top-5 records (defaults 0x43FB28, 26 slots x 5; race defaults are whole seconds, the port's own entries keep ticks)
struct RecordEntry {
    double value = 0;
    std::optional<double> ticks;
    int character = 0;
    std::string name;
    bool player = false;
};

// the save's records (web/career.js save.records), the defaults until a save has them
class RecordsStore {
public:
    void reset(const nlohmann::json& defaults) {
        slots_.clear();
        for (const auto& slot : defaults) {
            std::vector<RecordEntry> list;
            for (const auto& r : slot) {
                list.push_back(entryOf(r));
            }
            slots_.push_back(list);
        }
    }
    // a saved table (save.records); false when it is not one
    bool restore(const nlohmann::json& saved) {
        if (!saved.is_array() || saved.size() != slots_.size()) {
            return false;
        }
        std::vector<std::vector<RecordEntry>> next;
        for (const auto& slot : saved) {
            if (!slot.is_array()) {
                return false;
            }
            std::vector<RecordEntry> list;
            for (const auto& r : slot) {
                if (!r.is_object() || !r.contains("value") || !r["value"].is_number()) {
                    return false;
                }
                list.push_back(entryOf(r));
            }
            next.push_back(list);
        }
        slots_ = next;
        return true;
    }
    nlohmann::json toJson() const {
        nlohmann::json out = nlohmann::json::array();
        for (const auto& slot : slots_) {
            nlohmann::json list = nlohmann::json::array();
            for (const auto& r : slot) {
                nlohmann::json e = {{"value", r.value}, {"character", r.character}, {"name", r.name}};
                if (r.ticks) {
                    e["ticks"] = *r.ticks;
                }
                if (r.player) {
                    e["player"] = true;
                }
                list.push_back(e);
            }
            out.push_back(list);
        }
        return out;
    }
    size_t size() const {
        return slots_.size();
    }
    // web/career.js records: a timed entry without ticks has its seconds' ticks; a player entry saved as 'YOU' is PLAYER 1
    std::vector<RecordEntry> records(int slot, bool timed) const {
        std::vector<RecordEntry> list;
        if (slot < 0 || size_t(slot) >= slots_.size()) {
            return list;
        }
        for (RecordEntry r : slots_[size_t(slot)]) {
            if (timed && !r.ticks) {
                r.ticks = r.value * 60;
            }
            if (r.player && r.name == "YOU") {
                r.name = kPlayerName;
            }
            list.push_back(r);
        }
        return list;
    }
    // web/career.js addRecord: the entry's index in the top 5, or -1 (0x154CD8..0x154D58 through ssx::rules::recordInsert)
    int add(int slot, double value, int character, bool timed, const std::string& name = kPlayerName) {
        std::vector<RecordEntry> list = records(slot, timed);
        if (list.empty()) {
            return -1;
        }
        std::vector<double> values;
        for (const auto& r : list) {
            values.push_back(r.value);
        }
        double secs = 0;
        const int at = ssx::rules::recordInsert(timed, value, values.data(), unsigned(values.size()), secs);
        if (at < 0) {
            return -1;
        }
        RecordEntry entry;
        entry.value = secs;
        if (timed) {
            entry.ticks = value;
        }
        entry.character = character;
        entry.name = name;
        entry.player = true;
        list.insert(list.begin() + at, entry);
        list.resize(5);
        slots_[size_t(slot)] = list;
        return at;
    }

private:
    static RecordEntry entryOf(const nlohmann::json& r) {
        RecordEntry e;
        e.value = r.value("value", 0.0);
        if (r.contains("ticks") && r["ticks"].is_number()) {
            e.ticks = r["ticks"].get<double>();
        }
        e.character = r.value("character", 0);
        e.name = r.value("name", "");
        e.player = r.value("player", false);
        return e;
    }
    std::vector<std::vector<RecordEntry>> slots_;
};

// a rider's row as the race left it (web/ai-race.js standings: the human first, then the computer riders by slot)
struct RaceStanding {
    bool human = false;
    std::string name;
    bool finished = false;
    double finishTicks = 0;
    double remaining = 0;
    double place = 0;
    double origin = 0;
    bool dnf = false;
};

// web/career-ui.js finish's result for a race: the round's outcome and the rows (standings order)
struct RaceOutcome {
    int mode = 0;
    int course = 0;
    int round = 3;
    std::vector<double> times;
    std::vector<int> rank;
    int place = 0;
    int medal = -1;
    int record = -1;
    int message = ssx::rules::kRaceMessageNone;
    bool advance = false;
    bool unopposed = false;
    struct Row {
        std::string name;
        bool human = false;
        int rank = 0;
        std::string value;
        bool dnf = false;
    };
    std::vector<Row> rows;
};

// web/career.js raceResult + completeEvent for a Single Event race (round 3, not a career event: no level change, no platinum, no
// cascade), with heatRecord's top-5 entry. raceTicks: race_result_info [4] when the results show; origin: the round's (0).
inline RaceOutcome singleEventRace(const CareerData& data, RecordsStore& records, int course, int character, const std::vector<RaceStanding>& standings,
                                   double raceTicks, double origin = 0) {
    RaceOutcome out;
    out.mode = 0;
    out.course = course;
    out.round = 3;
    const nlohmann::json& race = data.rules["race"];
    int human = 0;
    for (size_t k = 0; k < standings.size(); ++k) {
        if (standings[k].human) {
            human = int(k);
            break;
        }
    }
    std::vector<ssx::rules::RaceRider> riders;
    for (const auto& s : standings) {
        ssx::rules::RaceRider r;
        r.dnf = s.dnf;
        r.finished = s.finished;
        r.finishTicks = s.finishTicks;
        r.remaining = s.remaining;
        r.place = s.place;
        r.origin = s.origin;
        riders.push_back(r);
    }
    double margin[3] = {0, 0, 0};
    const nlohmann::json& m = race.value("level_up_margin", nlohmann::json::array());
    for (size_t k = 0; k < 3 && k < m.size(); ++k) {
        margin[k] = m[k].get<double>();
    }
    ssx::rules::RaceRound rr;
    ssx::rules::raceRound(3, false, 1, 0, race.value("dnf_ticks", 360000.0), race.value("estimate_min_speed", 30.0), margin, raceTicks, riders, human, rr);
    out.times = rr.times;
    out.rank = rr.rank;
    out.place = rr.place;
    out.advance = rr.advance;
    out.message = rr.message;
    // heatRecord (0x14AB68 slot, 26 = none): the human's time unless DNF
    const nlohmann::json& slots = data.rules["record_slots"];
    int row[4] = {0, 0, 0, 0};
    const bool hasRow = course >= 0 && size_t(course) < slots.size() && slots[size_t(course)].size() >= 4;
    if (hasRow) {
        for (int k = 0; k < 4; ++k) {
            row[k] = slots[size_t(course)][size_t(k)].get<int>();
        }
    }
    const int slot = ssx::rules::recordSlot(out.mode, course, hasRow ? row : nullptr);
    if (!standings.empty() && standings[size_t(human)].dnf) {
        out.record = -1;
    } else if (slot < 26) {
        out.record = records.add(slot, out.times[size_t(human)], character, ssx::rules::isTimed(out.mode));
    }
    // completeEvent: the placement medal (0x155328)
    out.medal = ssx::rules::eventMedal(out.mode, out.place, false, 0, 0, out.times[size_t(human)], 0);
    for (size_t k = 0; k < standings.size(); ++k) {
        RaceOutcome::Row r;
        r.name = standings[k].name;
        r.human = standings[k].human;
        r.rank = out.rank[k];
        r.dnf = standings[k].dnf;
        r.value = r.dnf ? "DNF" : raceTime(out.times[k], false);
        out.rows.push_back(r);
    }
    out.unopposed = standings.size() < 2;
    return out;
}

// The results screens of a Single Event (ctm-records, ctm-results) and the results' Restart prompt (ctm-restart: 87yndialog over the
// PDA frame), over the world behind them.
class SingleEventScreens {
public:
    enum class Screen { None, Records, Results, Restart };
    enum class Action { None, NextEvent, Restart, Replay, Quit };
    // web/menu-rules.js introLockFrames: 43final_standings / 61toptimes / 87yndialog activate at 30, input from 31
    static constexpr int kIntroLock = 31;
    // web/menu-rules.js PS2_CHOICE_OUTRO: the Yes / No popup's TransitionOut (87yndialog 50 -> 70) before its choice acts
    static constexpr int kChoiceOutro = 20;

    const CareerData* data = nullptr;
    const RecordsStore* records = nullptr;
    // the full replay (web/replay-ui.js) behind Replay: greyed without it
    bool replayAvailable = false;

    Screen screen() const {
        return screen_;
    }
    int index() const {
        return index_;
    }
    bool topTime() const {
        return topTime_;
    }
    const RaceOutcome& outcome() const {
        return outcome_;
    }
    // finish(): a new top time opens the records first (20A8F8(7)), else the results on resultsFocus
    void open(const RaceOutcome& outcome) {
        outcome_ = outcome;
        topTime_ = outcome.record >= 0;
        set(topTime_ ? Screen::Records : Screen::Results);
        if (screen_ == Screen::Results) {
            index_ = resultsFocus();
        }
    }
    // the UI frame counter (screen-phases.js step): one pass per 60 Hz pad update. A choice's exit (phase 6) reaches its Stop after
    // its frames, and the next pass (phase 7) runs the choice: its action comes back from here
    Action frame() {
        ++frame_;
        inPass_ = true;
        Action out = Action::None;
        if (exit_ && exitStopped_) {
            const int choice = *exit_;
            exit_.reset();
            exitStopped_ = false;
            out = act(choice);
        } else if (exit_ && frame_ - exitAt_ >= kChoiceOutro) {
            exitStopped_ = true;
        }
        inPass_ = false;
        return out;
    }
    // phase 5 only (no input while the intro locks or a choice's exit plays)
    bool accepts() const {
        return screen_ != Screen::None && !exit_ && frame_ - startedAt_ >= kIntroLock;
    }
    // 0x1E7558: a Single Event's results open on item 1 (Restart)
    int resultsFocus() const {
        return 1;
    }
    std::vector<std::string> items() const {
        const CareerData& d = *data;
        if (screen_ == Screen::Results) {
            return {d.t("kT_OVRCMNNextevnt", "Next event"), d.t("kT_BTNRestart", "Restart"), d.t("kT_BTNReplay", "Replay"), d.t("kT_BTNRecords", "Records"),
                    d.t("kT_BTNQuit", "Quit")};
        }
        if (screen_ == Screen::Records) {
            if (topTime_) {
                return {d.t(0x0f3ab955u, "Continue"), d.t(0x0b0bfc13u, "Save Records")};
            }
            return {d.t("kT_OVRCMNReturn", "Return")};
        }
        if (screen_ == Screen::Restart) {
            return {d.t("kT_CMNYes", "Yes"), d.t("kT_CMNNo", "No")};
        }
        return {};
    }
    bool disabled(int i) const {
        if (screen_ == Screen::Results) {
            const auto list = items();
            return i >= 0 && size_t(i) < list.size() && list[size_t(i)] == data->t("kT_BTNReplay", "Replay") && !replayAvailable;
        }
        if (screen_ == Screen::Records) {
            // Save Records: the memory card (the port saves the records at once)
            return topTime_ && i == 1;
        }
        return false;
    }
    // a menu key (KeyboardEvent.code: ArrowUp / ArrowDown, Space / Enter = Cross / Start, Escape = Triangle); repeat: the browser's own
    // auto-repeat (ignored; the menus' repeats arrive as fresh presses)
    Action key(const std::string& code, bool repeat = false) {
        if (!accepts()) {
            return Action::None;
        }
        if (code == "ArrowUp" || code == "ArrowDown") {
            if (repeat) {
                return Action::None;
            }
            // the Yes / No popup (web/ui.js ps2MenuStep: a wrapping menu)
            if (screen_ == Screen::Restart) {
                const int n = int(items().size());
                index_ = (((index_ + (code == "ArrowUp" ? -1 : 1)) % n) + n) % n;
                return Action::None;
            }
            // the OV.LUI overlay menus (0x39AB50 / 0x39AC48): the next item not greyed, round past either end (wrap flag 0x80)
            const int n = int(items().size());
            const int d = code == "ArrowUp" ? -1 : 1;
            for (int k = 1; k < n; ++k) {
                const int j = (((index_ + d * k) % n) + n) % n;
                if (!disabled(j)) {
                    index_ = j;
                    break;
                }
            }
            return Action::None;
        }
        if (repeat) {
            return Action::None;
        }
        if (code == "Space" || code == "Enter") {
            return choose(index_);
        }
        if (code == "Escape") {
            // Triangle on the 87yndialog = No
            if (screen_ == Screen::Restart) {
                return choose(1);
            }
            back();
        }
        return Action::None;
    }
    Action choose(int i) {
        if (disabled(i)) {
            return Action::None;
        }
        // a choice on the Yes / No popup plays its exit first (web/screen-phases.js leave): the action comes from frame()
        if (screen_ == Screen::Restart) {
            if (!exit_) {
                exit_ = i;
                exitAt_ = frame_;
                exitStopped_ = false;
            }
            return Action::None;
        }
        if (screen_ == Screen::Results) {
            // resultAction: 0 Next event (the event select), 1 Restart ('Are you sure?' first), 2 the full replay, 3 Records, 4 Quit
            if (i == 0) {
                return Action::NextEvent;
            }
            if (i == 1) {
                // 0x20CF9C: 'Are you sure?' first, on No
                set(Screen::Restart);
                index_ = 1;
                return Action::None;
            }
            if (i == 2) {
                return Action::Replay;
            }
            if (i == 3) {
                set(Screen::Records);
                return Action::None;
            }
            if (i == 4) {
                return Action::Quit;
            }
            return Action::None;
        }
        if (screen_ == Screen::Records) {
            if (topTime_) {
                if (i != 0) {
                    return Action::None;
                }
                topTime_ = false;
                set(Screen::Results);
                index_ = resultsFocus();
                return Action::None;
            }
            set(Screen::Results);
            index_ = resultsFocus();
        }
        return Action::None;
    }
    // Triangle: the records back to the results (a new top time's: ui.set's item 0; the Return list's: the results' focus)
    void back() {
        if (screen_ != Screen::Records) {
            return;
        }
        const bool wasTop = topTime_;
        set(Screen::Results);
        if (!wasTop) {
            index_ = resultsFocus();
        }
    }
    void close() {
        screen_ = Screen::None;
        exit_.reset();
        exitStopped_ = false;
    }

    // drawResults / drawRecords through 43final_standings / 61toptimes (web/results-lui.js), the Restart prompt over the PDA frame
    // (drawMcomm: the pause's frame without its rows, then 87yndialog); nowFrames: the wall clock in 60ths of a second (the panels'
    // clock); fixedClock >= 0: the timeline frames fixed (QA: the panels at that frame, the PDA opened and the popup set at 0)
    void draw(HudCanvas& c, ResultsLui& lui, CtmPda& pda, double nowFrames, double fixedClock = -1) {
        if (screen_ == Screen::None || !lui.ready()) {
            return;
        }
        const CareerData& d = *data;
        if (screen_ == Screen::Restart) {
            const double now = fixedClock >= 0 ? fixedClock : nowFrames;
            const std::vector<std::string> lines = splitLines({d.t("kT_BTNRestart", "Restart"), d.t(0x0ebe93e5u, "Are you sure?")});
            // mcommFrame: the PDA opens when a PDA screen follows a non-PDA one (no PDA draw for 250 ms)
            if (fixedClock >= 0) {
                pda.opening(0);
                std::string key;
                for (size_t k = 0; k < lines.size(); ++k) {
                    key += (k ? "|" : "") + lines[k];
                }
                pda.fixPopup(key, 0);
            } else if (pdaDrawnAt_ < 0 || nowFrames - pdaDrawnAt_ > 15) {
                pda.opening(nowFrames);
            }
            pdaDrawnAt_ = nowFrames;
            pda.frame(c, now);
            pda.dialog(c, lines, index_, now, d.t("kT_CMNYes", "Yes"), d.t("kT_CMNNo", "No"));
            return;
        }
        const double t = fixedClock >= 0 ? fixedClock : lui.clock(nowFrames);
        const std::string title = d.eventName(outcome_.mode, outcome_.course) + " - " + modeLabel(outcome_.mode);
        const bool timed = ssx::rules::isTimed(outcome_.mode);
        if (screen_ == Screen::Results) {
            ResultsLui::Results r;
            r.title = title;
            r.sub = d.t("kT_OVRCMNQuickPlayResults", "Single Event Results");
            r.heads = {d.t("kT_OVRCMNRanking", "Rank"), d.t(0x0b18be53u, "Riders"), timed ? d.t("kT_OVRCMNTime", "Time") : d.t("kT_OVRCMNScore", "Score")};
            std::vector<RaceOutcome::Row> sorted = outcome_.rows;
            std::stable_sort(sorted.begin(), sorted.end(), [](const RaceOutcome::Row& a, const RaceOutcome::Row& b) { return a.rank < b.rank; });
            int hr = -1;
            for (size_t k = 0; k < sorted.size(); ++k) {
                r.rows.push_back({sorted[k].rank + 1, sorted[k].name, sorted[k].value, sorted[k].human});
                if (sorted[k].human && hr < 0) {
                    hr = int(k);
                }
            }
            if (hr >= 0 && outcome_.medal >= 0 && outcome_.medal <= 3) {
                r.medal = ResultsLui::Medal{hr, outcome_.medal};
            }
            r.message = message() + (outcome_.unopposed ? " (No computer riders in this run.)" : "");
            r.items = items();
            r.index = index_;
            r.disabled = [this](int i) { return disabled(i); };
            lui.results(c, t, r);
            return;
        }
        ResultsLui::Records r;
        r.title = title;
        r.timed = timed;
        const nlohmann::json& slots = d.rules["record_slots"];
        int row[4] = {0, 0, 0, 0};
        const bool hasRow = outcome_.course >= 0 && size_t(outcome_.course) < slots.size() && slots[size_t(outcome_.course)].size() >= 4;
        if (hasRow) {
            for (int k = 0; k < 4; ++k) {
                row[k] = slots[size_t(outcome_.course)][size_t(k)].get<int>();
            }
        }
        const int slot = ssx::rules::recordSlot(outcome_.mode, outcome_.course, hasRow ? row : nullptr);
        if (slot < 26) {
            for (const auto& e : records->records(slot, timed)) {
                ResultsLui::Record rec;
                rec.name = e.name;
                rec.rider = e.character >= 0 && size_t(e.character) < d.characterFirst.size() ? d.characterFirst[size_t(e.character)] : "";
                rec.value = timed ? raceTime(e.ticks ? *e.ticks : NAN, false) : jsNumber(e.value);
                rec.player = e.player;
                r.rows.push_back(rec);
            }
        }
        if (topTime_) {
            r.message = timed ? d.t(0x0eea5fd5u, "Congratulations, you've got a top time!") : d.t(0x0ea6d945u, "Congratulations, you've got a top score!");
        }
        r.items = items();
        r.index = index_;
        r.disabled = [this](int i) { return disabled(i); };
        lui.records(c, t, r);
    }
    // drawResults' message: the round's (none in a final), else the medal's congratulations or the loss text
    std::string message() const {
        const CareerData& d = *data;
        static const char* roundKeys[5] = {nullptr, "kT_OVRCongratGoToSemi", "kT_OVRRaceObjectives7", "kT_OVRCMNSorryGetThirdSemi", "kT_OVRCMNSorryGetThird"};
        if (outcome_.message > 0 && outcome_.message < 5) {
            return d.t(roundKeys[outcome_.message]);
        }
        if (outcome_.medal < 0) {
            return d.t("kT_OVRHELPSorryLose", "Sorry, you didn't win.");
        }
        static const char* medalKeys[4] = {"kT_OVRCongratulationsPlatinum", "kT_OVRCongratulationsGold", "kT_OVRCongratulationsSilver", "kT_OVRCongratulationsBronze"};
        return d.t(medalKeys[outcome_.medal], "");
    }

private:
    // the Yes / No popup's choice at its Stop: Yes restarts the round from the results (restartToCard), No goes back to them
    Action act(int choice) {
        if (choice == 0) {
            close();
            return Action::Restart;
        }
        set(Screen::Results);
        index_ = resultsFocus();
        return Action::None;
    }
    // promptLines' strings split at their line breaks
    static std::vector<std::string> splitLines(const std::vector<std::string>& texts) {
        std::vector<std::string> out;
        for (const auto& t : texts) {
            size_t at = 0;
            for (;;) {
                const size_t nl = t.find('\n', at);
                out.push_back(t.substr(at, nl == std::string::npos ? std::string::npos : nl - at));
                if (nl == std::string::npos) {
                    break;
                }
                at = nl + 1;
            }
        }
        return out;
    }
    // web/career-ui.js MODE_LABEL
    static std::string modeLabel(int mode) {
        static const char* labels[6] = {"Race", "Slopestyle", "Super Pipe", "Big Air", "Rival", "Rival"};
        return mode >= 0 && mode < 6 ? labels[mode] : "Rival";
    }
    // String(n) for a record's value (integers)
    static std::string jsNumber(double v) {
        char buf[32];
        if (v == std::floor(v) && std::fabs(v) < 1e15) {
            std::snprintf(buf, sizeof buf, "%.0f", v);
        } else {
            std::snprintf(buf, sizeof buf, "%.17g", v);
        }
        return buf;
    }
    // ui.set: a change of screen restarts its phases (and the focus at 0)
    void set(Screen s) {
        if (s != screen_) {
            // a screen entered by a choice inside the pass builds on the next one
            startedAt_ = inPass_ ? frame_ + 1 : frame_;
        }
        screen_ = s;
        index_ = 0;
    }
    Screen screen_ = Screen::None;
    int index_ = 0;
    bool topTime_ = false;
    long frame_ = 0;
    long startedAt_ = 0;
    bool inPass_ = false;
    std::optional<int> exit_;
    long exitAt_ = 0;
    bool exitStopped_ = false;
    double pdaDrawnAt_ = -1;
    RaceOutcome outcome_;
};

}  // namespace fe

}  // namespace ssx_native
