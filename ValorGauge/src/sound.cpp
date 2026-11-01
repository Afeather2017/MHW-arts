// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L4881-L4900, L4911-L5068. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

#ifndef VALOR_PROBE
// ACTIONSND (2026-10-22): play a custom WAV (winmm PlaySound) at a configured
// TIML frame of a configured action. Timeline = the motion clock (anim+0x10C,
// TIML 60fps frames, resets to 0 on every motion restart): arming happens
// while the clock is still before the cue point, the clock going backwards
// means the same action re-cast (re-arm), and the sound fires only if the
// action is STILL the current one at the cue frame - a knockback/flinch out
// of the move cancels it (无双斩 can be interrupted mid-swing).
// Config nativePC/plugins/ValorGauge.action-sound.txt, one rule per line:
//   <set> <id> <frame60> <wavPath>    frame cue (TIML frames at 60fps)
//   <set> <id> enter <wavPath>        fire the moment the action starts
// The frame cue rides the motion clock, so hitstop (卡肉) delays it with the
// animation. The enter cue instead fires the first frame the configured
// action becomes the current one (phase 2) - real-time, hitstop-immune. The
// defense-frame counter system rides this: the FSM auto-transitions into the
// 35-40 family the instant the defense triggers, so an enter rule on the
// counter move plays at exactly that moment (motion clock never involved).
// Path relative to the game root (the DLL's CWD). Missing file = the built-in
// default (3:108 @ 50, the 气刃无双斩 MHGU swoosh; user pinned fsm id 108
// 2026-10-22 - the gauge-rules comment that calls 107 无双斩 is stale) so
// deploying the wav alone works.

std::atomic<std::shared_ptr<std::vector<ActionSoundRule>>> g_actionSounds;

// SND_FILENAME|SND_ASYNC: winmm copies the name internally (only SND_MEMORY
// buffers must outlive playback), but a config reload can swap the rules
// vector while the play call runs - hand it a process-lifetime copy instead.
const char* StableActionSoundPath(const std::string& path) {
    static std::mutex mutex;
    static std::unordered_set<std::string> keep;
    std::scoped_lock lock(mutex);
    return keep.insert(path).first->c_str();
}

void LoadActionSoundConfig() {
    auto rules = std::make_shared<std::vector<ActionSoundRule>>();
    bool filePresent = false;
    std::ifstream input("nativePC/plugins/ValorGauge.action-sound.txt");
    std::string line;
    while (std::getline(input, line)) {
        filePresent = true;
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        std::istringstream ss(line);
        ActionSoundRule rule;
        std::string path;
        std::string trigger;
        if (!(ss >> rule.set >> rule.id >> trigger)) {
            continue;
        }
        if (trigger == "enter") {
            rule.onEnter = true;
            if (!(ss >> path)) {
                continue;
            }
        } else {
            // legacy line: the trigger token is the TIML frame number
            try {
                rule.frame = std::stof(trigger);
            } catch (const std::exception&) {
                continue;
            }
            if (!(ss >> path)) {
                continue;
            }
        }
        rule.path = path;
        rules->push_back(std::move(rule));
    }
    if (!filePresent) {
        ActionSoundRule def;
        def.set = 3;
        def.id = 108;
        def.frame = 50.0f;
        def.path = "nativePC/plugins/ValorGauge.sounds/swo_fx.wav";
        rules->push_back(def);
    }
    std::ostringstream msg;
    msg << "Action sounds: " << rules->size() << " rule(s)";
    for (const auto& rule : *rules) {
        msg << " [" << rule.set << ':' << rule.id << ' '
            << (rule.onEnter ? "enter" : "frame") << ' ';
        if (!rule.onEnter) {
            msg << '@' << rule.frame << "f ";
        }
        msg << rule.path
            << (std::filesystem::exists(rule.path) ? "" : " MISSING") << "]";
    }
    g_actionSounds.store(rules);
    Log(msg.str());
}

void PollActionSounds(const void* entity) {
    const auto rules = g_actionSounds.load();
    if (!rules || rules->empty()) {
        return;
    }
    void* ctrl = nullptr;
    {
        std::scoped_lock lock(g_actionMutex);
        ctrl = g_lockedPlayerController;
    }
    if (!ctrl || !ModWeaponActive()) {
        return;
    }
    if (entity != static_cast<const void*>(static_cast<std::uint8_t*>(ctrl) - 0x61C8)) {
        return;
    }
    auto* bytes = static_cast<std::uint8_t*>(ctrl);
    const std::int32_t phase = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xA8);
    const std::int32_t set = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xAC);
    const std::int32_t id = *reinterpret_cast<volatile std::int32_t*>(bytes + 0xB0);
    float mot = -1.0f;
    const auto player = static_cast<std::uint8_t*>(ctrl) - 0x61C8;
    if (IsReadable(player + 0x468, sizeof(void*))) {
        void* anim = *reinterpret_cast<void* const*>(player + 0x468);
        if (anim && IsReadable(static_cast<std::uint8_t*>(anim) + 0x10C, 4)) {
            mot = *reinterpret_cast<volatile float*>(static_cast<std::uint8_t*>(anim) + 0x10C);
        }
    }
    for (auto& rule : *rules) {
        const bool matches =
            set == rule.set && id == rule.id && phase == 2 &&
            (rule.onEnter || mot >= 0.0f);
        if (!matches) {
            if (rule.armed && !rule.fired) {
                std::ostringstream m;
                m << "ACTIONSND cancel act=" << rule.set << ':' << rule.id
                  << " left the action at mot=" << mot << " (cue "
                  << rule.frame << "f not reached)";
                Log(m.str());
            }
            rule.armed = false;
            rule.fired = false;
            continue;
        }
        if (rule.onEnter) {
            // the action becoming current IS the trigger (e.g. the FSM
            // auto-transition landing into the 35-40 counter the instant the
            // defense frames trigger): fire on the first observed frame,
            // once per occurrence - fired resets above when the action leaves
            if (!rule.fired) {
                rule.fired = true;
                PlaySoundA(StableActionSoundPath(rule.path), nullptr,
                           SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
                std::ostringstream m;
                m << "ACTIONSND enter fire act=" << set << ':' << id
                  << " -> " << rule.path;
                Log(m.str());
            }
            continue;
        }
        if (!rule.armed) {
            // arm only while the clock is still before the cue so a poll that
            // first observes the action late cannot fire a stale sound
            if (mot <= rule.frame) {
                rule.armed = true;
                rule.fired = false;
                rule.armMot = mot;
            }
            continue;
        }
        if (mot < rule.armMot - 1.0f) {
            // same action re-cast: the motion clock restarted from 0
            rule.armMot = mot;
            rule.fired = false;
        }
        if (!rule.fired && mot >= rule.frame) {
            rule.fired = true;
            PlaySoundA(StableActionSoundPath(rule.path), nullptr,
                       SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
            std::ostringstream m;
            m << "ACTIONSND fire act=" << set << ':' << id << " mot=" << mot
              << " -> " << rule.path;
            Log(m.str());
        }
    }
}

#endif // !VALOR_PROBE
