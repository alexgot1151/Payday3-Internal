#pragma once

#include <imgui.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cmath>
#include "../../Dumper-7/SDK.hpp"
#include "../../Utils/Logging.hpp"

namespace ESP
{
    // ESP Configuration
    
    struct EnemyESP{
        bool m_bBox = false;
        bool m_bHealth = false;
        bool m_bArmor = false;
        bool m_bName = false;
        bool m_bFlags = true;
        bool m_bSkeleton = false;
        bool m_bOutline = true;
    };

    struct CivilianESP{
        bool m_bBox = true;
        bool m_bFlags = true;
        bool m_bSkeleton = false;
        bool m_bOutline = true;
        bool m_bOnlyWhenSpecial = true;
    };

    struct Config {
        bool bESP = false;

        EnemyESP m_stNormalEnemies{};
        EnemyESP m_stSpecialEnemies{};
        CivilianESP m_stCivilians{};

        bool bDebugSkeleton = false;
        bool bDebugDrawBoneIndices = false;
        bool bDebugDrawBoneNames = false;
        bool bDebugSkeletonDrawBoneIndices = false;
        bool bDebugSkeletonDrawBoneNames = false;
        bool bDebugESP = false;
    };

    inline Config& GetConfig() {
        static Config config{};
        return config;
    }

    void Render(SDK::UWorld* pGWorld, SDK::APlayerController* pPlayerController);
    void RenderDebugESP(SDK::ULevel* pPersistentLevel, SDK::APlayerController* pPlayerController);
}

namespace LootESP
{
    struct Config {
        bool bLootESP = false;
    };

    inline Config& GetConfig() {
        static Config config{};
        return config;
    }
}
