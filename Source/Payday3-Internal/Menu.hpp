#pragma once

#include <map>
#include <imgui.h>
#include "Dumper-7/SDK.hpp"
#include "Config.hpp"
#include "Utils/Logging.hpp"
#include "Features/ESP/ESP.hpp"

struct CheatConfig{
    struct Aimbot_t {
        bool m_bSilentAim = false;
        bool m_bAimFix = false;
        bool m_bAimTest = false;
        float m_flAimFix = 0.5f;
        float m_flAimScalar = 10.f;

        void Draw();
    };

    Aimbot_t m_aimbot{};

    struct Visuals_t {
        void Draw();
    };

    Visuals_t m_visuals{};

    struct Misc_t {
        bool m_bClientMove = false;
        bool m_bNoSpread = true;
        bool m_bNoRecoil = true;
        bool m_bNoCameraShake = true;
        bool m_bInstantInteraction = true;
        bool m_bInstantMinigame = true;

        void Draw();
    };

    Misc_t m_misc{};

    static CheatConfig& Get(){
        static CheatConfig config{};
        return config;
    };
};

namespace Menu
{
    enum class ECallTraceArea{
        Inactive,
        UObject,
        PlayerController
    };

    inline char g_szCallTraceFilter[1024]{};
    inline bool g_bCallTraceFilterSubclasses = false;
    inline ECallTraceArea g_eCallTraceArea = ECallTraceArea::Inactive;

    inline std::string g_sCallTraceFilter{};

    struct CallTraceEntry_t{
        std::string m_sClassName{};
        std::vector<std::string> m_vecSubClasses{};
        std::map<size_t, std::string> m_mapCalledFunctions{};

        void Draw();
    };

    inline std::map<size_t, CallTraceEntry_t> g_mapCallTraces{};

    void PreDraw();
	void Draw(bool& bShowMenu);
    void PostDraw();
}
