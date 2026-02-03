#pragma once

#include <map>
#include <imgui.h>
#include "Dumper-7/SDK.hpp"
#include "Config.hpp"
#include "Utils/Logging.hpp"
#include "Features/ESP/ESP.hpp"

namespace Menu
{
    inline bool g_bClientMove = false;
    inline char g_szCallTraceFilter[1024]{};
    inline bool g_bCallTraceFilterSubclasses = false;
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
