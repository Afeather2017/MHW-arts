#pragma once
#include <cstdint>
#include <string>

// Probe-build research tools: F5 clang miner, F10 struct diff capture,
// F12 ATK-row scan + guard-page access trace, prop write test. All
// definitions are #ifdef VALOR_PROBE; the overlay only calls them from the
// probe branch.

// F5 paired-scenario state mining (whiff runs vs clang runs -> ranked
// candidate offsets).
void MinerCaptureFrame();
void MinerHandleF9();

// F12 ATKSCAN/ATKTRACE (cAttackParamPl rows, PAGE_GUARD access trace).
extern bool g_atkKeyWasDown;
void AtkScanArm();
void PollAtkTrace();

// F10 two-phase struct diff capture (baseline -> armed).
void PollDiffCapture();
std::string DiffStatusText();

// Shift+F6 property write test (0xDE-family counter row).
void TryPropertyWrite();
void LoadPropWriteConfig();

// hex/float formatting helpers shared by the diff report.
std::string FormatHex32(std::uint32_t value);
std::string FormatFloat32(std::uint32_t raw);
