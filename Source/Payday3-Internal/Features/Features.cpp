#include "Features.hpp"
#include "../Menu.hpp" 
#include "../Dumper-7/SDK.hpp"
#include "../Utils/Logging.hpp"

#undef min
#undef max

void InstantInteraction(SDK::USBZPlayerInteractorComponent* pInteractor){
    static std::chrono::time_point<std::chrono::steady_clock> timeInteractLast = std::chrono::steady_clock::now();

    if(std::chrono::steady_clock::now() - timeInteractLast <= Cheat::g_durationPing || !pInteractor)
        return;

    auto pInteraction = pInteractor->GetCurrentInteraction();
    if(!pInteraction)
        return;

    if(auto pOwner = pInteraction->GetOwner(); pOwner && pOwner->IsA(SDK::ASBZBagItem::StaticClass()))
        return;
        

    pInteractor->Server_CompleteInteraction(pInteraction, pInteractor->InteractId);
    if(!Cheat::g_bIsSoloGame) // Makes the instant interaction look smoother in multiplayer
        pInteractor->Multicast_CompletedInteraction(pInteraction, false);

    // Dirty hack to prevent completing the interaction multiple times.
    timeInteractLast = std::chrono::steady_clock::now();
}

void InstantMinigame(SDK::ASBZPlayerState* pPlayerState){
    if(pPlayerState->MiniGameState != SDK::EPD3MiniGameState::None)
        pPlayerState->Server_SetMiniGameState(SDK::EPD3MiniGameState::Success);
}

struct WeaponDataBackupEntry_t{

    // Recoil
    float m_flViewSpeedDeflect;
    float m_flGunSpeedDeflect;

    // Spread
    float m_flInnerClusterSpreadMultiplier;
    float m_flFireSpreadStart;
    float m_flFireSpreadMinCap;
    float m_flFireSpreadCap;
    float m_flFireSpreadIncrease;
};

bool g_bDidBackupWeaponData = false;
static std::map<uint64_t, WeaponDataBackupEntry_t> g_mapWeaponDataBackup{};

bool BackupWeaponData(SDK::USBZGameInstance* pGame){
    auto pDatabase = pGame->GlobalItemDatabase;
    if(!pDatabase)
        return false;

    SDK::USBZWeaponDatabase* aDatabases[3]{
        pDatabase->PrimaryWeapons.Get(),
        pDatabase->SecondaryWeapons.Get(),
        pDatabase->OverkillWeapons.Get()
    };

    if(!aDatabases[0] || !aDatabases[1] || !aDatabases[2])
        return false;

    for(int iDatabase = 0; iDatabase < 3; ++iDatabase){
        for(int i = 0; i < aDatabases[iDatabase]->Weapons.Num(); i++){
            if(!aDatabases[iDatabase]->Weapons.IsValidIndex(i))
                return false;

            auto pWeaponData = reinterpret_cast<SDK::USBZRangedWeaponData*>(aDatabases[iDatabase]->Weapons[i]);
            if(!pWeaponData)
                return false;

            if(!pWeaponData->IsA(SDK::USBZRangedWeaponData::StaticClass()))
                continue;

            if(!pWeaponData->SpreadData || !pWeaponData->RecoilData)
                return false;

            g_mapWeaponDataBackup.try_emplace(std::hash<std::string>{}(pWeaponData->Name.ToString().substr(14)), WeaponDataBackupEntry_t{
                .m_flViewSpeedDeflect = pWeaponData->RecoilData->ViewKick.SpeedDeflect,
                .m_flGunSpeedDeflect = pWeaponData->RecoilData->GunKickXY.SpeedDeflect,
                .m_flInnerClusterSpreadMultiplier = pWeaponData->SpreadData->InnerClusterSpreadMultiplier,
                .m_flFireSpreadStart = pWeaponData->SpreadData->FireSpreadStart,
                .m_flFireSpreadMinCap = pWeaponData->SpreadData->FireSpreadMinCap,
                .m_flFireSpreadCap = pWeaponData->SpreadData->FireSpreadCap,
                .m_flFireSpreadIncrease = pWeaponData->SpreadData->FireSpreadIncrease
            });
        }
    }

    return true;
}

void RestoreWeaponSpread(SDK::USBZRangedWeaponData* pWeaponData){
    if(!pWeaponData || !g_bDidBackupWeaponData)
        return;

    auto pEquippable = pWeaponData->EquippableClass.Get();
    if(!pEquippable)
        return;

    std::string sName = pEquippable->Name.ToString().substr(16);
    sName.resize(sName.size() - 2);

    auto itrEntry = g_mapWeaponDataBackup.find(std::hash<std::string>{}(sName));
    if(itrEntry == g_mapWeaponDataBackup.end() || !pWeaponData->SpreadData)
        return;
    
    pWeaponData->SpreadData->InnerClusterSpreadMultiplier = itrEntry->second.m_flInnerClusterSpreadMultiplier;
    pWeaponData->SpreadData->FireSpreadStart = itrEntry->second.m_flFireSpreadStart;
    pWeaponData->SpreadData->FireSpreadMinCap = itrEntry->second.m_flFireSpreadMinCap;
    pWeaponData->SpreadData->FireSpreadCap = itrEntry->second.m_flFireSpreadCap;
    pWeaponData->SpreadData->FireSpreadIncrease = itrEntry->second.m_flFireSpreadIncrease;
}

void RestoreWeaponRecoil(SDK::USBZRangedWeaponData* pWeaponData){
    if(!pWeaponData || !g_bDidBackupWeaponData)
        return;

    auto pEquippable = pWeaponData->EquippableClass.Get();
    if(!pEquippable)
        return;

    std::string sName = pEquippable->Name.ToString().substr(16);
    sName.resize(sName.size() - 2);

    auto itrEntry = g_mapWeaponDataBackup.find(std::hash<std::string>{}(sName));
    if(itrEntry == g_mapWeaponDataBackup.end() || !pWeaponData->RecoilData)
        return;
    
    pWeaponData->RecoilData->ViewKick.SpeedDeflect = itrEntry->second.m_flViewSpeedDeflect;
    pWeaponData->RecoilData->GunKickXY.SpeedDeflect = itrEntry->second.m_flGunSpeedDeflect;
}

void OverrideMethLabInteractables(){
    if(Cheat::g_iMethLabIndex <= 0 || Cheat::g_iMethLabIndex > SDK::UObject::GObjects->Num())
        return;

    auto pLab = reinterpret_cast<SDK::ASBZCookingStation*>(SDK::UObject::GObjects->GetByIndex(Cheat::g_iMethLabIndex));
    if(!pLab || !pLab->IsA(SDK::ASBZCookingStation::StaticClass()))
        return;

    int iIngredientNum = reinterpret_cast<uint64_t*>(pLab->Pad_540)[2];
    int iActiveIngredient = -1;
    
    static uint64_t iIngredientCode = 0;
    uint64_t iCurrentIngredientCode = reinterpret_cast<uint64_t**>(pLab->Pad_540)[3][0];
    if(iIngredientCode != iCurrentIngredientCode){
        std::cout << std::hex << iCurrentIngredientCode << std::dec << '\n';
        iIngredientCode = iCurrentIngredientCode;
    }
    switch(iCurrentIngredientCode){
    case 0x1: //1 - 213
        iActiveIngredient = (iIngredientNum == 0) ? 1 : (iIngredientNum == 1) ? 0 : 2;
        break;
    case 0x2: //2 - 312
        iActiveIngredient = (iIngredientNum == 0) ? 2 : (iIngredientNum == 1) ? 0 : 1;
        break;
    case 0x100000000: //100000000 - 123
        iActiveIngredient = (iIngredientNum == 0) ? 0 : (iIngredientNum == 1) ? 1 : 2;
        break;
    case 0x100000002: //100000002 - 321
        iActiveIngredient = (iIngredientNum == 0) ? 2 : (iIngredientNum == 1) ? 1 : 0;
        break;
    case 0x200000000: //200000000 - 132
        iActiveIngredient = (iIngredientNum == 0) ? 0 : (iIngredientNum == 1) ? 2 : 1;
        break;
    case 0x200000001: //200000001 - 231
        iActiveIngredient = (iIngredientNum == 0) ? 1 : (iIngredientNum == 1) ? 2 : 0;
        break;
    default:
        break;
    }

    for(int i = 0; i < pLab->IngredientInteractableArray.Num(); ++i){
        auto pInteractable = pLab->IngredientInteractableArray[i];
        pInteractable->SetLocalEnabled(i == iActiveIngredient);
    }
}



void Cheat::OnPlayerControllerTick(){
    SDK::UWorld* pGWorld = SDK::UWorld::GetWorld();
	if (!pGWorld)
		return;

    auto pGameInstance = reinterpret_cast<SDK::USBZGameInstance*>(pGWorld->OwningGameInstance);
	if (!pGameInstance || !pGameInstance->IsA(SDK::USBZGameInstance::StaticClass()))
		return;

    SDK::USBZWorldRuntime* pWorldRuntime = reinterpret_cast<SDK::USBZWorldRuntime*>(SDK::USBZWorldRuntime::GetWorldRuntime(pGWorld));
    if (!pWorldRuntime)
        return;

    //OverrideMethLabInteractables();

    g_bIsSoloGame = SDK::USBZOnlineFunctionLibrary::IsSoloGame(pGWorld);

    if(!g_bDidBackupWeaponData)
        g_bDidBackupWeaponData = BackupWeaponData(pGameInstance);

	SDK::ULocalPlayer* pULocalPlayer = pGameInstance->LocalPlayers[0];
	if (!pULocalPlayer)
		return;

	auto pLocalPlayerController = reinterpret_cast<SDK::ASBZPlayerController*>(pULocalPlayer->PlayerController);
	if (!pLocalPlayerController || !pLocalPlayerController->IsA(SDK::ASBZPlayerController::StaticClass()))
		return;
    
    //pLocalPlayerController->ServerChangeName(SDK::FString(L"Omegaware PD3"));

	auto pLocalPlayer = reinterpret_cast<SDK::ASBZPlayerCharacter*>(pLocalPlayerController->AcknowledgedPawn);
	if (!pLocalPlayer || !pLocalPlayer->IsA(SDK::ASBZPlayerCharacter::StaticClass()))
		return;

    if(CheatConfig::Get().m_misc.m_bInstantInteraction)
        InstantInteraction(pLocalPlayer->Interactor);

	if (pLocalPlayer->SBZPlayerState)
	{
		auto pPlayerState = pLocalPlayer->SBZPlayerState;
        g_durationPing = std::chrono::milliseconds(std::min(static_cast<int>(pPlayerState->GetPingInMilliseconds() * 2.f), 30));

		if (CheatConfig::Get().m_misc.m_bInstantMinigame)
			InstantMinigame(pPlayerState);
	}

    if (pLocalPlayer->PlayerAbilitySystem)
    {
        static SDK::FGameplayTag tagDummy{
            SDK::UKismetStringLibrary::Conv_StringToName(L"")
        };

        auto pAbilitySystem = pLocalPlayer->PlayerAbilitySystem;
        auto& aAbilities = pAbilitySystem->ActivatableAbilities.Items;

        static bool bDidHaveSpeedBuff = false;
        bool bSpeedBuff = CheatConfig::Get().m_misc.m_bSpeedBuff;
        if(bSpeedBuff)
            pAbilitySystem->Server_SetSpeedBuffTime(tagDummy, 99999.f);
        else if(bDidHaveSpeedBuff)
            pAbilitySystem->Server_ResetSpeedBuffTime();
        bDidHaveSpeedBuff = bSpeedBuff;

        static bool bDidHaveDamageBuff = false;
        bool bDamageBuff = CheatConfig::Get().m_misc.m_bDamageBuff;
        if(bDamageBuff)
            pAbilitySystem->Server_SetDamageBuffTime(tagDummy, 99999.f);
        else if(bDidHaveDamageBuff)
            pAbilitySystem->Server_ResetDamageBuffTime();
        bDidHaveDamageBuff = bDamageBuff;

        static bool bDidHaveArmorBuff = false;
        bool bArmorBuff = CheatConfig::Get().m_misc.m_bArmorBuff;
        if(bArmorBuff)
            pAbilitySystem->Server_SetMitigationBuffTime(tagDummy, 99999.f);
        else if(bDidHaveArmorBuff)
            pAbilitySystem->Server_ResetMitigationBuffTime();
        bDidHaveArmorBuff = bArmorBuff;

        //pAbilitySystem->Server
        static auto nameDefault__GA_Fire_C = SDK::UKismetStringLibrary::Conv_StringToName(L"Default__GA_Fire_C");
        static auto nameDefault__GA_Reload_C = SDK::UKismetStringLibrary::Conv_StringToName(L"Default__GA_Reload_C");
        static auto nameDefault__GA_Run_C = SDK::UKismetStringLibrary::Conv_StringToName(L"Default__GA_Run_C");
        for (int i = 0; i < aAbilities.Num(); ++i) {
            if (!aAbilities.IsValidIndex(i))
                continue;

            auto pAbility = aAbilities[i].Ability;
            if (!pAbility)
                continue;

            pAbilitySystem->Client_UnblockAbility(aAbilities[i].Handle);

            if (pAbility->Name == nameDefault__GA_Fire_C){
                Cheat::g_iFireAbilityHandle = aAbilities[i].Handle.Handle;
            }
            if (pAbility->Name == nameDefault__GA_Run_C){
                //pAbilitySystem->ServerSetInputPressed(aAbilities[i].Handle);
                //pAbilitySystem->ClientTryActivateAbility(aAbilities[i].Handle);
            }

            if (pAbility->Name == nameDefault__GA_Reload_C){
                
                //pAbilitySystem->ServerCancelAbility(aAbilities[i].Handle, pAbility->CurrentActivationInfo);
            }

            //GetCurrentMontage()
            //pAbilitySystem->ServerSetInputPressed(aAbilities[i].Handle);
            //pAbilitySystem->ClientTryActivateAbility(aAbilities[i].Handle);
        }
    }

	if (pLocalPlayer->FPCameraAttachment && pLocalPlayer->FPCameraAttachment->EquippedWeaponData){
		if (pLocalPlayer->FPCameraAttachment->EquippedWeaponData->IsA(SDK::USBZRangedWeaponData::StaticClass())){
			auto pWeaponData = reinterpret_cast<SDK::USBZRangedWeaponData*>(pLocalPlayer->FPCameraAttachment->EquippedWeaponData);
		
			if (pWeaponData->SpreadData){
                // No Spread
				auto pSpreadData = pWeaponData->SpreadData;
                if(CheatConfig::Get().m_misc.m_bNoSpread)
                    pSpreadData->InnerClusterSpreadMultiplier = pSpreadData->FireSpreadStart = pSpreadData->FireSpreadMinCap = pSpreadData->FireSpreadCap = pSpreadData->FireSpreadIncrease = 0.f;
                else if(pSpreadData->InnerClusterSpreadMultiplier == 0.f && pSpreadData->FireSpreadStart == 0.f && pSpreadData->FireSpreadMinCap == 0.f && pSpreadData->FireSpreadCap == 0.f && pSpreadData->FireSpreadIncrease == 0.f)
                    RestoreWeaponSpread(pWeaponData);				
			}

            if(pWeaponData->RecoilData){
                // No Recoil
                auto pRecoilData = pWeaponData->RecoilData;
                if(CheatConfig::Get().m_misc.m_bNoRecoil)
                    pRecoilData->ViewKick.SpeedDeflect = pRecoilData->GunKickXY.SpeedDeflect = 0.f;
                else if(pRecoilData->ViewKick.SpeedDeflect == 0.f && pRecoilData->GunKickXY.SpeedDeflect == 0.f)
                    RestoreWeaponRecoil(pWeaponData);
            }
		}
	}

    SDK::USBZPlayerMovementComponent* pMovementComponent = reinterpret_cast<SDK::USBZPlayerMovementComponent*>(pLocalPlayer->GetComponentByClass(SDK::USBZPlayerMovementComponent::StaticClass()));
    if (!pMovementComponent)
        return;

    //pMovementComponent->Server_StartTraversal()
    if(CheatConfig::Get().m_misc.m_keyClientMove.GetState()){
        if (pLocalPlayer->GetActorEnableCollision())
            pLocalPlayer->SetActorEnableCollision(false);

        pMovementComponent->MovementMode = SDK::EMovementMode::MOVE_Flying;
        pMovementComponent->BrakingDecelerationFlying = 10000.f;
        pMovementComponent->MaxFlySpeed = 10000.f;
        pMovementComponent->Velocity = pMovementComponent->Acceleration;
    }
    else if(!pLocalPlayer->GetActorEnableCollision()){
        pLocalPlayer->SetActorEnableCollision(true);
        pMovementComponent->MovementMode = SDK::EMovementMode::MOVE_Walking;
    }

    if(CheatConfig::Get().m_misc.m_bNoCameraShake)
	    pLocalPlayerController->PlayerCameraManager->StopAllCameraShakes(true);
}