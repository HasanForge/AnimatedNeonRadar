// AnimatedNeonRadar.cpp
//
// Standalone "Animated Neon Radar" plugin for GTA Vice City, built against
// dk22pac/plugin-sdk (https://github.com/dk22pac/plugin-sdk). This file
// intentionally contains ONLY the radar logic: no money/health/armor/ammo/
// weapon-icon/star code from the original multi-feature HUD mod.
//
// Target toolchain: i686-w64-mingw32-g++, C++17, no MSVC-only headers or
// macros (no <tchar.h>, no TCHAR, no GetPrivateProfileString).

#include "plugin.h"
#include "CTimer.h"
#include "CTxdStore.h"
#include "CSprite2d.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>

using namespace plugin;

namespace {

// ---------------------------------------------------------------------------
// Minimal INI reader.
//
// Replaces the original GetPrivateProfileString() calls with a small,
// type-safe, portable parser so the plugin has no dependency on the Win32
// profile APIs. Only the "[SETTINGS]" section and the handful of keys this
// plugin cares about are parsed.
// ---------------------------------------------------------------------------
class IniFile {
public:
    explicit IniFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open())
            return; // Missing file -> every Get* call below falls back to its default.

        std::string line;
        std::string section;
        while (std::getline(file, line)) {
            Trim(line);
            if (line.empty() || line.front() == ';' || line.front() == '#')
                continue;

            if (line.front() == '[' && line.back() == ']') {
                section = line.substr(1, line.size() - 2);
                continue;
            }

            const auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            Trim(key);
            Trim(value);
            values_[section + '.' + key] = std::move(value);
        }
    }

    float GetFloat(const std::string& section, const std::string& key, float fallback) const {
        const std::string* raw = Find(section, key);
        if (!raw) return fallback;
        try { return std::stof(*raw); } catch (...) { return fallback; }
    }

    int GetInt(const std::string& section, const std::string& key, int fallback) const {
        const std::string* raw = Find(section, key);
        if (!raw) return fallback;
        try { return std::stoi(*raw); } catch (...) { return fallback; }
    }

private:
    const std::string* Find(const std::string& section, const std::string& key) const {
        const auto it = values_.find(section + '.' + key);
        return it == values_.end() ? nullptr : &it->second;
    }

    static void Trim(std::string& s) {
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) { s.clear(); return; }
        const auto last = s.find_last_not_of(" \t\r\n");
        s = s.substr(first, last - first + 1);
    }

    std::unordered_map<std::string, std::string> values_;
};

struct RadarConfig {
    float posX        = 0.01f;
    float posY         = 0.72f;
    float sizeX         = 265.0f;
    float sizeY         = 220.0f;
    bool  enabled       = true;
    int   animSpeedMs   = 40;
};

RadarConfig LoadConfig() {
    const IniFile ini("./hud.ini");
    RadarConfig cfg;
    cfg.posX       = ini.GetFloat("SETTINGS", "Custom Radar X Pos", cfg.posX);
    cfg.posY       = ini.GetFloat("SETTINGS", "Custom Radar Y Pos", cfg.posY);
    cfg.sizeX      = ini.GetFloat("SETTINGS", "Custom Radar Size X", cfg.sizeX);
    cfg.sizeY      = ini.GetFloat("SETTINGS", "Custom Radar Size Y", cfg.sizeY);
    cfg.enabled    = ini.GetInt("SETTINGS", "Custom Radar", 1) != 0;
    cfg.animSpeedMs = ini.GetInt("SETTINGS", "Animation Speed", 40);
    return cfg;
}

// ---------------------------------------------------------------------------
// Memory addresses (GTA:VC 1.0), carried over unchanged from the original
// reference code.
// ---------------------------------------------------------------------------
constexpr unsigned int kAddrScreenWidth  = 0xA0FD04; // int, current resolution width
constexpr unsigned int kAddrScreenHeight = 0xA0FD08; // int, current resolution height
constexpr unsigned int kAddrRadarMaxSize = 0x697C18; // float, stock radar draw size
constexpr unsigned int kAddrRadarVisible = 0x86963A; // char/bool, whether HUD wants the radar drawn
constexpr float        kStockRadarSize   = 116.0f;   // original game's default radar size
constexpr int          kFrameCount       = 120;       // Neon1..Neon120 / Neon1a..Neon120a

// ---------------------------------------------------------------------------
// Animated neon radar. All state lives inside this class (no free-standing
// globals), and it owns exactly one responsibility: drawing the animated
// radar overlay in place of the stock radar.
// ---------------------------------------------------------------------------
class AnimatedNeonRadar {
public:
    AnimatedNeonRadar() {
        config_ = LoadConfig();
        Events::drawRadarMapEvent += [this] { OnDrawRadar(); };
    }

private:
    void EnsureTxdLoaded() {
        if (txdSlot_ != -1)
            return;
        txdSlot_ = CTxdStore::AddTxdSlot("new_hud");
        CTxdStore::LoadTxd(txdSlot_, "models\\new_hud.txd");
    }

    void OnDrawRadar() {
        EnsureTxdLoaded();

        CTxdStore::AddRef(txdSlot_);
        CTxdStore::PushCurrentTxd();
        CTxdStore::SetCurrentTxd(txdSlot_);

        if (patch::GetChar(kAddrRadarVisible) != 0)
            DrawFrame();

        CTxdStore::PopCurrentTxd();
    }

    void DrawFrame() {
        if (!config_.enabled) {
            // Custom radar turned off in hud.ini -> let the stock radar draw normally.
            patch::SetFloat(kAddrRadarMaxSize, kStockRadarSize);
            return;
        }

        // Push the stock radar's draw size far off-screen so only our overlay shows.
        patch::SetFloat(kAddrRadarMaxSize, 9999.0f);

        AdvanceFrame();

        const float screenW = static_cast<float>(patch::GetInt(kAddrScreenWidth));
        const float screenH = static_cast<float>(patch::GetInt(kAddrScreenHeight));
        const float x0 = screenW * config_.posX;
        const float y0 = screenH * config_.posY;
        const float x1 = x0 + config_.sizeX;
        const float y1 = y0 + config_.sizeY;

        char diffuseName[16];
        char alphaName[16];
        std::snprintf(diffuseName, sizeof(diffuseName), "Neon%d", frame_);
        std::snprintf(alphaName, sizeof(alphaName), "Neon%da", frame_);

        // Only touch the texture when the frame actually changed; avoids
        // redundant SetTexture() calls / state switches every render.
        if (frame_ != lastBoundFrame_) {
            texture_.SetTexture(diffuseName, alphaName);
            lastBoundFrame_ = frame_;
        }

        texture_.Draw(
            CRect(x0, y0, x1, y1),
            CRGBA(255, 255, 255, 255),
            0.015f, 0.015f, 1.0f, 0.0f, 0.015f, 1.0f, 1.0f, 1.0f);
    }

    void AdvanceFrame() {
        const int now = CTimer::m_snTimeInMilliseconds;
        if (now > lastFrameTime_ + config_.animSpeedMs) {
            frame_ = (frame_ % kFrameCount) + 1;
            lastFrameTime_ = now;
        }
    }

    RadarConfig config_;
    CSprite2d   texture_;
    int         txdSlot_        = -1;
    int         frame_          = 1;
    int         lastBoundFrame_ = -1;
    int         lastFrameTime_  = 0;
};

AnimatedNeonRadar gAnimatedNeonRadar;

} // namespace
