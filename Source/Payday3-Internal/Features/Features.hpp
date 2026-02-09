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
    inline SDK::FVector g_vecAimbotTargetLocation{};
    inline SDK::FRotator g_rotAimbotTargetRotation{};
    inline bool g_bIsAimbotTargetAvailible = false;

    void OnPlayerControllerTick();
};