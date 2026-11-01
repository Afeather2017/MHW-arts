// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L46-L46, L52-L59, L8280-L8302, L8462-L8743. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

PresentFn g_originalPresent = nullptr;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
std::once_flag g_imguiOnce;
std::atomic_bool g_visible = true;
bool g_f8WasDown = false;
bool g_f7WasDown = false;
bool g_f6WasDown = false;
bool g_f9WasDown = false;

bool InitializeRenderer(IDXGISwapChain* swapChain) {
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_device)))) {
        Log("DX11 device acquisition failed");
        return false;
    }

    g_device->GetImmediateContext(&g_context);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    if (!ImGui_ImplDX11_Init(g_device, g_context)) {
        Log("ImGui DX11 initialization failed");
        return false;
    }

    Log("DX11 renderer initialized");
    return true;
}

void DrawGauge(IDXGISwapChain* swapChain) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc))) {
        return;
    }

    RECT client{};
    if (!GetClientRect(desc.OutputWindow, &client)) {
        return;
    }

    const float width = static_cast<float>(client.right - client.left);
    const float height = static_cast<float>(client.bottom - client.top);
    if (width <= 0.0f || height <= 0.0f) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(width, height);
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    const bool f8Down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    if (f8Down && !g_f8WasDown) {
        g_visible = !g_visible.load();
    }
    g_f8WasDown = f8Down;

#ifdef VALOR_PROBE
    const bool f7Down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    if (f7Down && !g_f7WasDown) {
        LoadStateAddress();
        g_stateLoggerEnabled = true;
        Log("State logger reloaded with F7");
    }
    g_f7WasDown = f7Down;
    PollStateLogger();
    PollDiffCapture();
    PollPropHistogram();
    PollReactionWatch();
    PollPoolDiff();
    PollAtkTrace();
    MinerCaptureFrame();
    // F12 arms ATKSCAN/ATKTRACE. Bare F9 is taken by the CSharp loader and
    // Shift+F9 missed presses in the field too; F10/F11 are diff capture and
    // the property histogram. Note: F12 is Steam's default screenshot key -
    // a capture popup alongside the arm is expected and harmless.
    const bool atkKey = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
    if (atkKey && !g_atkKeyWasDown) {
        AtkScanArm();
    }
    g_atkKeyWasDown = atkKey;
    const bool f5Down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (f5Down && !g_f9WasDown) {
        MinerHandleF9();
    }
    g_f9WasDown = f5Down;
    PollChipGuardHotkey();
    PollGuardRollback();
    const bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    if (f6Down && !g_f6WasDown) {
        TryPropertyWrite();
    }
    g_f6WasDown = f6Down;
#else
    PollValorLifecycle();
    PollArtQuestGate();
    PollModTriggers();
    PollGaugeLock();
    PollSheathWatch();
    PollHpLock();
    PollHpWatch();
    PollStaminaControl();
    PollGuardRollback();

    // Counter window: a spirit-key press while the window is open fires 刚1.
    const bool windowOpen = std::chrono::steady_clock::now() < g_counterWindowUntil;
    const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (windowOpen && ctrlDown && !g_counterCtrlWasDown) {
        g_forceSet = g_forceCfgSet.load(std::memory_order_relaxed);
        g_forceId = g_forceCfgId.load(std::memory_order_relaxed);
        if (!g_forceArmed.exchange(true)) {
            Log("COUNTER INPUT: spirit press inside the counter window - force armed");
        }
    }
    g_counterCtrlWasDown = ctrlDown;

    const bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    const bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (f6Down && !g_f6WasDown && shiftDown) {
        // Hot reload: force-action.txt (+ the gauge/sheath sub-configs) is
        // otherwise only read once at plugin init.
        LoadForceActionConfig();
        LoadChipGuardConfig();
        Log("Force action config reloaded (Shift+F6)");
    } else if (f6Down && !g_f6WasDown && ModWeaponActive()) {
        if (!g_forceArmed.exchange(true)) {
            std::ostringstream m;
            m << "FORCE armed: the next Long Sword transition executes 3:"
              << g_forceId;
            Log(m.str());
        }
    }
    g_f6WasDown = f6Down;

    // Gauge-lock experiment toggle (F5). Only meaningful when a lock value is
    // configured in ValorGauge.gauge-lock.txt.
    const bool f5Down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (f5Down && !g_gaugeLockKeyWasDown &&
        g_gaugeLockValue.load(std::memory_order_relaxed) > 0.0f) {
        const bool newEnabled =
            !g_gaugeLockEnabled.load(std::memory_order_relaxed);
        g_gaugeLockEnabled.store(newEnabled, std::memory_order_relaxed);
        std::ostringstream m;
        m << "GAUGELOCK " << (newEnabled ? "ON" : "OFF")
          << " (F5): target weapon+0x"
          << (g_gaugeLockTarget.load(std::memory_order_relaxed) == 2 ? "2368"
                                                                     : "2374")
          << " <- " << g_gaugeLockValue.load(std::memory_order_relaxed)
          << " per frame";
        Log(m.str());
    }
    g_gaugeLockKeyWasDown = f5Down;

    // FX test trigger: F7 fires the configured player-emitter notify once;
    // Shift+F7 reloads ValorGauge.fx-test.txt (sweep candidate ids).
    const bool f7Down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    if (f7Down && !g_fxKeyWasDown) {
        if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) {
            LoadFxTestConfig();
        } else {
            g_fxFireFunc.store(g_fxCfgFunc, std::memory_order_release);
            g_fxFireId.store(g_fxCfgId, std::memory_order_relaxed);
            std::ostringstream m;
            m << "FXTEST: queued func=" << g_fxCfgFunc << " id=0x" << std::hex
              << g_fxCfgId;
            Log(m.str());
        }
    }
    g_fxKeyWasDown = f7Down;
#endif

    if (g_visible) {
        std::optional<ActionInfo> displayedAction;
        {
            std::scoped_lock lock(g_actionMutex);
            displayedAction = g_lastLongSwordAction;
        }
        char actionText[64]{};
        if (displayedAction) {
            snprintf(actionText, sizeof(actionText), "Action %d:%d",
                     displayedAction->actionSet, displayedAction->actionId);
        } else {
            snprintf(actionText, sizeof(actionText), "Action --:--");
        }

        ImDrawList* draw = ImGui::GetForegroundDrawList();
#ifdef VALOR_PROBE
        draw->AddText(ImGui::GetFont(), 22.0f, ImVec2(0.0f, 0.0f),
                      IM_COL32(255, 55, 55, 255), actionText);
        const std::string diffStatus = DiffStatusText();
        if (!diffStatus.empty()) {
            draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 26.0f),
                          IM_COL32(80, 255, 80, 255), diffStatus.c_str());
        }

        char rxnText[128]{};
        int off = snprintf(rxnText, sizeof(rxnText), "7C0=%u gate=%u nohit=%.2f",
                           g_rxnStandby, g_rxnGate, g_rxnNohit);
        for (int wi = 0; wi < g_watchCount && off > 0 && off < (int)sizeof(rxnText); ++wi) {
            off += snprintf(rxnText + off, sizeof(rxnText) - off, " p%u=[%d %d %d]",
                            g_watchIds[wi], static_cast<int>(g_watchCur[wi][0]),
                            static_cast<int>(g_watchCur[wi][1]),
                            static_cast<int>(g_watchCur[wi][2]));
        }
        draw->AddText(ImGui::GetFont(), 16.0f, ImVec2(0.0f, 48.0f),
                      g_rxnStandby != 0 ? IM_COL32(90, 220, 255, 255)
                                        : IM_COL32(170, 170, 170, 200),
                      rxnText);
#else
        if (ModWeaponActive()) {
            draw->AddText(ImGui::GetFont(), 22.0f, ImVec2(0.0f, 0.0f),
                          IM_COL32(255, 55, 55, 255), actionText);
            if (windowOpen) {
                draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 70.0f),
                              IM_COL32(80, 220, 255, 255), "COUNTER WINDOW");
            }
            if (g_forceArmed.load()) {
                draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 92.0f),
                              IM_COL32(255, 220, 80, 255), "FORCE ARMED");
            }
            const float lockValue =
                g_gaugeLockValue.load(std::memory_order_relaxed);
            if (lockValue > 0.0f) {
                char lockText[112]{};
                snprintf(lockText, sizeof(lockText),
                         "GLOCK t=0x%s %.2f %s | 74:%.4f x%u 68:%.4f x%u",
                         g_gaugeLockTarget.load(std::memory_order_relaxed) == 2
                             ? "2368"
                             : "2374",
                         lockValue,
                         g_gaugeLockEnabled.load(std::memory_order_relaxed)
                             ? "ON"
                             : "OFF",
                         g_gaugeWatchDrift[0], g_gaugeWatchFrames[0],
                         g_gaugeWatchDrift[1], g_gaugeWatchFrames[1]);
                draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 114.0f),
                              g_gaugeLockEnabled.load(std::memory_order_relaxed)
                                  ? IM_COL32(80, 255, 120, 255)
                                  : IM_COL32(170, 170, 170, 200),
                              lockText);
            }
        }
        // SHEATHIN line (2026-11-02): 纳刀输入检测实时显示 — 稳定对 (逻辑 id) +
        // 位集 nibble (h/b/e/b2) + 闸门剩余。检测到输入=绿, 闸门生效=黄。
        {
            int sheathId = 0;
            std::uint32_t nib = 0;
            std::int64_t remainMs = 0;
            if (SheathGateStatus(&sheathId, &nib, &remainMs)) {
                const bool detected =
                    nib != 0 && nib != 0xFFFFFFFFu;
                char shText[96]{};
                snprintf(shText, sizeof(shText),
                         "SHEATHIN id=%d h=%u b=%u e=%u%s GATE %.1fs",
                         sheathId, nib & 1u, (nib >> 1) & 1u, (nib >> 2) & 1u,
                         (nib & 8u) ? " b2" : "",
                         remainMs > 0 ? remainMs / 1000.0 : 0.0);
                draw->AddText(
                    ImGui::GetFont(), 18.0f, ImVec2(0.0f, 136.0f),
                    detected ? IM_COL32(80, 255, 120, 255)
                             : (remainMs > 0 ? IM_COL32(255, 220, 80, 255)
                                             : IM_COL32(160, 160, 160, 200)),
                    shText);
            }
        }
        // INPUTBITS line (2026-11-02): 游戏视角的实时按住位 (逻辑 id 列表) —
        // R 按住条件监测 (user 提案, 局内映射 R={1,6}, 纳刀=24 闪现即消费)。
        // R 位亮 = 行尾标 R 且转红色。
        {
            std::uint32_t inIds[12] = {};
            const int n = SheathHeldIds(inIds, 12);
            if (n > 0) {
                bool rHeld = false;
                char inText[128]{};
                int off2 = snprintf(inText, sizeof(inText), "IN n=%d [", n);
                for (int i = 0; i < n && off2 > 0 && off2 < (int)sizeof(inText);
                     ++i) {
                    if (inIds[i] == 1 || inIds[i] == 6) rHeld = true;
                    off2 += snprintf(inText + off2, sizeof(inText) - off2,
                                     "%s%u", i ? "," : "", inIds[i]);
                }
                if (off2 > 0 && off2 < (int)sizeof(inText)) {
                    snprintf(inText + off2, sizeof(inText) - off2, "]%s",
                             rHeld ? " R" : "");
                    draw->AddText(ImGui::GetFont(), 18.0f, ImVec2(0.0f, 158.0f),
                                  rHeld ? IM_COL32(255, 90, 90, 255)
                                        : IM_COL32(120, 220, 255, 255),
                                  inText);
                }
            }
        }
        // Mode line (2026-10-25): the in-game HUD gauge color never repaints
        // on mid-quest level writes, so this overlay line is the source of
        // truth - phase label (CHARGE = white charge phase, VALOR = red mode,
        // PASS = yellow fold-back transient) + charge bar (weapon+0x2374)
        // percent, color-coded by the live spirit level. Works from the hub
        // on (DisplayWeapon falls back to the manager chain).
        if (std::uint8_t* modeWeapon = DisplayWeapon()) {
            if (IsReadable(modeWeapon + 0x2370, sizeof(float) * 2)) {
                const auto level = *reinterpret_cast<
                    const volatile std::int32_t*>(modeWeapon + 0x2370);
                const float rawBar = *reinterpret_cast<const volatile float*>(
                    modeWeapon + 0x2374);
                const float bar = std::clamp(rawBar, 0.0f, 1.0f);
                const char* mode = "BAD";
                ImU32 color = IM_COL32(255, 55, 55, 255);
                switch (level) {
                    case 1:
                        mode = "CHARGE";
                        color = IM_COL32(235, 235, 235, 255);
                        break;
                    case 2:
                        mode = "PASS";
                        color = IM_COL32(255, 215, 80, 255);
                        break;
                    case 3:
                        mode = "VALOR";
                        color = IM_COL32(255, 70, 70, 255);
                        break;
                    case 0:
                        mode = "NONE";
                        color = IM_COL32(160, 160, 160, 255);
                        break;
                    default:  // struct-shift alarm: level out of 0..3
                        break;
                }
                char modeText[48]{};
                snprintf(modeText, sizeof(modeText), "SPIRIT lv%d %s %d%%",
                         level, mode,
                         static_cast<int>(bar * 100.0f + 0.5f));
                draw->AddText(ImGui::GetFont(), 20.0f, ImVec2(0.0f, 28.0f),
                              color, modeText);
            }
        }
        // ART line (战技槽, 2026-10-26): the external gauge as text for now -
        // a drawn bar is a later milestone. Blue while charging (the future
        // 蓝条 color), green READY at full. Hidden outside combat zones -
        // quests AND training area / exploration (InCombatZone); PollArtQuestGate
        // wipes the gauge there.
        if (g_artEnabled.load(std::memory_order_relaxed) && InCombatZone()) {
            const float art = g_artGauge.load(std::memory_order_relaxed);
            const int pct = static_cast<int>(art + 0.5f);
            char artText[32]{};
            snprintf(artText, sizeof(artText),
                     pct >= 100 ? "ART %d%% READY" : "ART %d%%", pct);
            draw->AddText(ImGui::GetFont(), 20.0f, ImVec2(0.0f, 52.0f),
                          pct >= 100 ? IM_COL32(80, 255, 120, 255)
                                     : IM_COL32(90, 180, 255, 255),
                          artText);
        }
#endif
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

HRESULT __stdcall PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    std::call_once(g_imguiOnce, [swapChain]() { InitializeRenderer(swapChain); });
    if (g_device && g_context) {
        DrawGauge(swapChain);
    }
    return g_originalPresent(swapChain, syncInterval, flags);
}
