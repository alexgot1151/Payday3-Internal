#pragma once

#include "../Features.hpp"

namespace Cheat::ClientMove{
    inline bool g_bInClientMove{};

    bool ShouldSendMovePacket();

    void OnPlayerControllerTick(SDK::ASBZPlayerCharacter* pLocalPlayer, SDK::USBZPlayerMovementComponent* pMovementComponent);
};