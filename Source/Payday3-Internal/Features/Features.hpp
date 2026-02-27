#pragma once

#include <imgui.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include "../Dumper-7/SDK.hpp"
#include "../Utils/Logging.hpp"

namespace Cheat {
    inline std::chrono::milliseconds g_durationPing{ 100 };
    inline bool g_bIsSoloGame = false;
    inline bool g_bIsInGame = false;
    inline int32_t g_iFireAbilityHandle = 0;
    inline int32_t g_iMethLabIndex = -1;

    inline bool g_bShouldBeAiming = true;
    struct TargetInfo_t{
        SDK::FVector m_vecTargetPosition;
        SDK::FVector m_vecAimPosition;
        SDK::FRotator m_rotAimRotation;
        float m_flPriority;
        float m_flDistance;
        int32_t m_iIndex;
        SDK::APawn* m_pEntity{};
    };

    inline std::optional<TargetInfo_t> g_stTargetInfo{};

    void OnPlayerControllerTick();
};