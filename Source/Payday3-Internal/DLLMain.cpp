#include <Windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <string>
#include <utility>
#include <stacktrace>
#include <MinHook.h>
#pragma comment(lib, "version.lib")

#include "config.hpp"

#include "Utils/Console.hpp"
#include "Utils/Logging.hpp"
#include "Utils/Dx12Hook.hpp"
#include "Menu.hpp"
#include "Dumper-7/SDK.hpp"
#include "Features/Features.hpp"

#undef min
#undef max


namespace Globals {
#ifdef _DEBUG
	static constexpr bool g_bDebug = true;
#else
	static constexpr bool g_bDebug = false;
#endif
	
	static HMODULE g_hModule;
	static HMODULE g_hBaseModule;
	std::unique_ptr<Console> g_upConsole;
}

bool Init() 
{
	Globals::g_hBaseModule = GetModuleHandleA(NULL);

	std::string consoleTitle = "Payday3-Internal Debug Console " + std::string(CURRENT_VERSION);
	Globals::g_upConsole = std::make_unique<Console>(Globals::g_bDebug, consoleTitle.c_str());
	if (!Globals::g_upConsole)
		return false;

	SDK::UWorld* pGWorld = SDK::UWorld::GetWorld();
	if (!pGWorld) {
		bool bOriginalVisibility = Globals::g_upConsole->GetVisibility();
		Globals::g_upConsole->SetVisibility(true);

		Utils::LogError("Failed to get GWorld pointer!\n Will wait for 30 seconds before aborting...");
		std::this_thread::sleep_for(std::chrono::seconds(3));
		Globals::g_upConsole->SetVisibility(bOriginalVisibility);
	}

	std::chrono::time_point startTime = std::chrono::high_resolution_clock::now();
	while (!pGWorld) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		pGWorld = SDK::UWorld::GetWorld();

		std::chrono::time_point currentTime = std::chrono::high_resolution_clock::now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
		if (elapsedTime >= 30) {
			Globals::g_upConsole->SetVisibility(true);
			Utils::LogError("Timeout while waiting for GWorld pointer!");
			return false;
		}
	}

	int32_t iFramerateLimit = SDK::USBZSettingsFunctionsVideo::GetFramerateLimit(pGWorld);
	Utils::LogDebug("GWorld pointer acquired: " + std::to_string(reinterpret_cast<uint64_t>(pGWorld)));

	if (MH_Initialize() != MH_OK) {
		Globals::g_upConsole->SetVisibility(true);
		Utils::LogError("Failed to initialize MinHook library!");
		return false;
	}

    if (!Dx12Hook::Initialize()) {
        Globals::g_upConsole->SetVisibility(true);
        Utils::LogError("Failed to initialize DirectX 12 hook!");
        return false;
    }

	SDK::USBZSettingsFunctionsVideo::SetFramerateLimit(pGWorld, iFramerateLimit);
	return true;
}

#include "Dumper-7/SDK/Engine_parameters.hpp"
#include "Dumper-7/SDK/Starbreeze_parameters.hpp"

SDK::ASBZPlayerCharacter* GetLocalPlayer()
{
	SDK::UWorld* pGWorld = SDK::UWorld::GetWorld();
	if (!pGWorld)
		return{};

	SDK::UGameInstance* pGameInstance = pGWorld->OwningGameInstance;
	if (!pGameInstance)
		return{};

	SDK::ULocalPlayer* pLocalPlayer = pGameInstance->LocalPlayers[0];
	if (!pLocalPlayer)
		return{};

	SDK::ASBZPlayerController* pLocalPlayerController = reinterpret_cast<SDK::ASBZPlayerController*>(pLocalPlayer->PlayerController);
	if (!pLocalPlayerController || !pLocalPlayerController->IsA(SDK::ASBZPlayerController::StaticClass()))
		return{};

	SDK::ASBZPlayerCharacter* pLocalPlayerPawn = reinterpret_cast<SDK::ASBZPlayerCharacter*>(pLocalPlayerController->AcknowledgedPawn);
	if (!pLocalPlayerPawn || !pLocalPlayerPawn->IsA(SDK::ASBZPlayerCharacter::StaticClass()))
		return{};

	return pLocalPlayerPawn;
}

using UObjectProcessEvent_t = void(*)(const SDK::UObject*, class SDK::UFunction*, void*);
UObjectProcessEvent_t UObjectProcessEvent_o = nullptr;
void UObjectProcessEvent_hk(const SDK::UObject* pObject, class SDK::UFunction* pFunction, void* pParams)
{
	if(pObject->IsA(SDK::UKismetStringLibrary::StaticClass()))
	{
		UObjectProcessEvent_o(pObject, pFunction, pParams);
		return;
	}

	std::string sClassName = pObject->GetName();
	std::string sFnName = pFunction->GetName();

	if(Menu::g_eCallTraceArea == Menu::ECallTraceArea::UObject){
		size_t iNameHash = std::hash<std::string>{}(pObject->Class->Name.GetRawString());
		size_t iFuncHash = std::hash<std::string>{}(pFunction->GetName());

		if (auto itrEntry = Menu::g_mapCallTraces.find(iNameHash); itrEntry != Menu::g_mapCallTraces.end()) {
			if (!itrEntry->second.m_mapCalledFunctions.contains(iFuncHash))
				itrEntry->second.m_mapCalledFunctions.try_emplace(iFuncHash, sFnName);
		}
		else {
			Menu::CallTraceEntry_t entry{
				.m_sClassName = pObject->Class->Name.GetRawString(),
			};

			entry.m_mapCalledFunctions.try_emplace(iFuncHash, sFnName);

			auto pStruct = static_cast<const SDK::UStruct*>(pObject->Class);
			for (pStruct = static_cast<const SDK::UStruct*>(pStruct->SuperStruct); pStruct != nullptr; pStruct = static_cast<const SDK::UStruct*>(pStruct->SuperStruct))
				entry.m_vecSubClasses.push_back(pStruct->Name.GetRawString());
			
			Menu::g_mapCallTraces.try_emplace(iNameHash, std::move(entry));
		}
	}

	static auto nameBlueprintUpdateCamera = SDK::UKismetStringLibrary::Conv_StringToName(L"BlueprintUpdateCamera");
	static auto nameServerUpdateCamera = SDK::UKismetStringLibrary::Conv_StringToName(L"ServerUpdateCamera");
	
	static auto nameServerMovePacked = SDK::UKismetStringLibrary::Conv_StringToName(L"ServerMovePacked");
	static auto nameGA_Fire_C = SDK::UKismetStringLibrary::Conv_StringToName(L"GA_Fire_C");
	static auto nameK2_CommitExecute = SDK::UKismetStringLibrary::Conv_StringToName(L"K2_CommitExecute");

	if (sFnName.contains("ClientPlayForceFeedback_Internal")) {
		return;
	}

	if(pObject->IsA(SDK::ASBZKeypadBase::StaticClass()))
	{
		auto pKeypad = reinterpret_cast<SDK::ASBZKeypadBase*>(const_cast<SDK::UObject*>(pObject));
		
		uint8_t iActiveKey = 0;
		switch(pKeypad->Inputs)
		{
		case(0):
			iActiveKey = pKeypad->Code / 1000;
			break;

		case(1):
			iActiveKey = (pKeypad->Code / 100) % 10;
			break;
			
		case(2):
			iActiveKey = (pKeypad->Code / 10) % 10;
			break;

		case(3):
			iActiveKey = pKeypad->Code % 10;
			break;

		default:
			iActiveKey = (pKeypad->GuessedCode != pKeypad->Code) ? 10 : 11;
			break;
		}

		for (int i = 0; i < pKeypad->KeypadInteractableComponentArray.Num(); i++)
		{
			auto pKey = pKeypad->KeypadInteractableComponentArray[i];
			if(!pKey)
				continue;
			
			pKey->SetLocalEnabled(i == static_cast<int>(iActiveKey));
		}

		UObjectProcessEvent_o(pObject, pFunction, pParams);
		return;
	}

	if(pObject->IsA(SDK::ACH_PlayerBase_C::StaticClass()) || pObject->IsA(SDK::APlayerController::StaticClass()) || pObject->IsA(SDK::APlayerCameraManager::StaticClass()) || pObject->IsA(SDK::UCharacterMovementComponent::StaticClass()))
	{
		if(pFunction->Name == nameServerMovePacked){
			if(!CheatConfig::Get().m_misc.m_bClientMove) 
				UObjectProcessEvent_o(pObject, pFunction, pParams);

			return;
		}
	}

	UObjectProcessEvent_o(pObject, pFunction, pParams);
}

UObjectProcessEvent_t UObjectProcessEventPlayer_o = nullptr;
void UObjectProcessEventPlayer_hk(const SDK::UObject* pObject, class SDK::UFunction* pFunction, void* pParams)
{
	std::string sClassName = pObject->GetName();
	std::string sFnName = pFunction->GetName();

	if(Menu::g_eCallTraceArea == Menu::ECallTraceArea::PlayerController){
		size_t iNameHash = std::hash<std::string>{}(pObject->Class->Name.GetRawString());
		size_t iFuncHash = std::hash<std::string>{}(pFunction->GetName());

		if (auto itrEntry = Menu::g_mapCallTraces.find(iNameHash); itrEntry != Menu::g_mapCallTraces.end()) {
			if (!itrEntry->second.m_mapCalledFunctions.contains(iFuncHash))
				itrEntry->second.m_mapCalledFunctions.try_emplace(iFuncHash, sFnName);
		}
		else {
			Menu::CallTraceEntry_t entry{
				.m_sClassName = pObject->Class->Name.GetRawString(),
			};

			entry.m_mapCalledFunctions.try_emplace(iFuncHash, sFnName);

			auto pStruct = static_cast<const SDK::UStruct*>(pObject->Class);
			for (pStruct = static_cast<const SDK::UStruct*>(pStruct->SuperStruct); pStruct != nullptr; pStruct = static_cast<const SDK::UStruct*>(pStruct->SuperStruct))
				entry.m_vecSubClasses.push_back(pStruct->Name.GetRawString());
			
			Menu::g_mapCallTraces.try_emplace(iNameHash, std::move(entry));
		}
	}

	static auto nameReceiveTick = SDK::UKismetStringLibrary::Conv_StringToName(L"ReceiveTick");
	if(pObject->IsA(SDK::ACH_PlayerBase_C::StaticClass()) && pFunction->Name == nameReceiveTick)
		Cheat::OnPlayerControllerTick();

	UObjectProcessEventPlayer_o(pObject, pFunction, pParams);
}

static SDK::FVector g_vecOriginalLocation{};
static SDK::FRotator g_rotOriginalRotation{};
static SDK::FRotator g_rotSilentAimRotation{};

using ULocalPlayerGetViewPoint_t = void(*)(SDK::ULocalPlayer*, SDK::FMinimalViewInfo*);
ULocalPlayerGetViewPoint_t ULocalPlayerGetViewPoint_o = nullptr;
void ULocalPlayerGetViewPoint_hk(SDK::ULocalPlayer* _this, SDK::FMinimalViewInfo* OutViewInfo)
{
	ULocalPlayerGetViewPoint_o(_this, OutViewInfo);

	OutViewInfo->Location = g_vecOriginalLocation;
	OutViewInfo->Rotation = g_rotOriginalRotation;
}

using APlayerControllerGetPlayerViewPoint_t = void(*)(SDK::APlayerController*, SDK::FVector*, SDK::FRotator*);
APlayerControllerGetPlayerViewPoint_t APlayerControllerGetPlayerViewPoint_o = nullptr;
void APlayerControllerGetPlayerViewPoint_hk(SDK::APlayerController* _this, SDK::FVector* out_Location, SDK::FRotator* out_Rotation)
{
	APlayerControllerGetPlayerViewPoint_o(_this, out_Location, out_Rotation);

	g_vecOriginalLocation = *out_Location;
	g_rotOriginalRotation = *out_Rotation;

	if(!CheatConfig::Get().m_aimbot.m_bSilentAim)
	{
		g_rotSilentAimRotation = *out_Rotation;
		return;
	}
	
	SDK::FRotator rotCurrent = *out_Rotation;
	SDK::FRotator rotGoal = SDK::FRotator(0.f, 0.f, 0.f);


	SDK::FRotator rotOut{};
	rotOut.Yaw = rotGoal.Yaw - ((rotGoal.Yaw - rotCurrent.Yaw) * 0.49f);
	rotOut.Pitch = 0.f;

	if(Cheat::g_bIsAimbotTargetAvailible){
		*out_Location = Cheat::g_vecAimbotTargetLocation;
		*out_Rotation = SDK::UKismetMathLibrary::FindLookAtRotation(*out_Location, Cheat::g_vecAimbotTargetLocation);
	}
}

void MainLoop()
{	
	while (!GetAsyncKeyState(UNLOAD_KEY) && !GetAsyncKeyState(UNLOAD_KEY_ALT))
    {
		if (GetAsyncKeyState(CONSOLE_KEY) & 0x1)
			Globals::g_upConsole->ToggleVisibility();

		// Do frame independent work here
		SDK::UWorld* pGWorld = SDK::UWorld::GetWorld();
		if (!pGWorld)
			continue;

		if (!UObjectProcessEvent_o)
		{
			auto pFn = reinterpret_cast<void*>(SDK::InSDKUtils::GetVirtualFunction<UObjectProcessEvent_t>(pGWorld, SDK::Offsets::ProcessEventIdx)); 
			if (MH_CreateHook(pFn, reinterpret_cast<void*>(&UObjectProcessEvent_hk), reinterpret_cast<void**>(&UObjectProcessEvent_o)) == MH_OK)
				Utils::LogHook("UObjectProcessEvent", MH_EnableHook(pFn));
		}

		SDK::UGameInstance* pGameInstance = pGWorld->OwningGameInstance;
		if (!pGameInstance)
			continue;

		SDK::ULocalPlayer* pLocalPlayer = pGameInstance->LocalPlayers[0];
		if (!pLocalPlayer)
			continue;

		if (!ULocalPlayerGetViewPoint_o)
		{
			auto pFn = reinterpret_cast<void*>(SDK::InSDKUtils::GetVirtualFunction<ULocalPlayerGetViewPoint_t>(pLocalPlayer, 0x50));
			if (MH_CreateHook(pFn, reinterpret_cast<void*>(&ULocalPlayerGetViewPoint_hk), reinterpret_cast<void**>(&ULocalPlayerGetViewPoint_o)) == MH_OK)
				Utils::LogHook("ULocalPlayerGetViewPoint", MH_EnableHook(pFn));
		}
	
		SDK::APlayerController* pLocalPlayerControllerBase = pLocalPlayer->PlayerController;
		if (!pLocalPlayerControllerBase)
			continue;

		if (!APlayerControllerGetPlayerViewPoint_o)
		{
			auto pFn = reinterpret_cast<void*>(SDK::InSDKUtils::GetVirtualFunction<APlayerControllerGetPlayerViewPoint_t>(pLocalPlayerControllerBase, 0xED));
			if (MH_CreateHook(pFn, reinterpret_cast<void*>(&APlayerControllerGetPlayerViewPoint_hk), reinterpret_cast<void**>(&APlayerControllerGetPlayerViewPoint_o)) == MH_OK)
				Utils::LogHook("APlayerControllerGetPlayerViewPoint", MH_EnableHook(pFn));
		}

		if(!UObjectProcessEventPlayer_o)
		{
			auto pFn = reinterpret_cast<void*>(SDK::InSDKUtils::GetVirtualFunction<UObjectProcessEvent_t>(pLocalPlayerControllerBase, SDK::Offsets::ProcessEventIdx)); 
			if (MH_CreateHook(pFn, reinterpret_cast<void*>(&UObjectProcessEventPlayer_hk), reinterpret_cast<void**>(&UObjectProcessEventPlayer_o)) == MH_OK)
				Utils::LogHook("UObjectProcessEventPlayer", MH_EnableHook(pFn));
		}

		SDK::ASBZPlayerController* pLocalPlayerController = reinterpret_cast<SDK::ASBZPlayerController*>(pLocalPlayerControllerBase);
		if (!pLocalPlayerController || !pLocalPlayerController->IsA(SDK::ASBZPlayerController::StaticClass()))
			continue;
		
		SDK::ASBZPlayerCharacter* pLocalPlayerPawn = reinterpret_cast<SDK::ASBZPlayerCharacter*>(pLocalPlayerController->AcknowledgedPawn);
		if (!pLocalPlayerPawn || !pLocalPlayerPawn->IsA(SDK::ASBZPlayerCharacter::StaticClass()))
			continue;

		SDK::USBZPlayerMovementComponent* pMovementComponent = reinterpret_cast<SDK::USBZPlayerMovementComponent*>(pLocalPlayerPawn->GetComponentByClass(SDK::USBZPlayerMovementComponent::StaticClass()));
		if (!pMovementComponent)
			continue;

		if(SDK::USBZOnlineFunctionLibrary::IsSoloGame(pGWorld) && 0)
		{

			if(pLocalPlayerPawn->FPCameraAttachment){
				auto pCameraComponent = pLocalPlayerPawn->FPCameraAttachment;
				if(pCameraComponent->EquippedWeaponData && pCameraComponent->EquippedWeaponData->IsA(SDK::USBZRangedWeaponData::StaticClass())){
					auto pRangedWeaponData = reinterpret_cast<SDK::USBZRangedWeaponData*>(pCameraComponent->EquippedWeaponData);
					if(pRangedWeaponData->FireData && pRangedWeaponData->FireData->IsA(SDK::USBZPlayerWeaponFireData::StaticClass())){
						auto pFireData = reinterpret_cast<SDK::USBZPlayerWeaponFireData*>(pRangedWeaponData->FireData);
						pFireData->AmmoLoadedMax = pFireData->AmmoPerReload = 999.f;
						pFireData->AmmoInventoryMax = 99999.f;
					}
				}

				if(pCameraComponent->EquippedWeapon && pCameraComponent->EquippedWeapon->IsA(SDK::ASBZWeapon::StaticClass())){
					auto pWeapon = pCameraComponent->EquippedWeapon;
					//pWeapon->bIsReloading = false;
				}
			}

			if(pLocalPlayerPawn->SBZPlayerState){
				auto pPlayerState = pLocalPlayerPawn->SBZPlayerState;
				pPlayerState->ReloadEndTime = 0.f;
			}

		}

		//bool LineOfSightTo(const class AActor* Other, const struct FVector& ViewPoint, bool bAlternateChecks) const;
		
		pLocalPlayerPawn->CarryTiltDegrees = 0.0f;
		pLocalPlayerPawn->CarryTiltSpeed = 10000.0f;
		//pLocalPlayerPawn->CarryAdditionalTiltDegrees = 0.0f;

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

bool VerifyGameVersion()
{
	char szBlock[MAX_PATH];
	GetModuleFileNameA(Globals::g_hBaseModule, szBlock, MAX_PATH);
	Utils::LogDebug(std::string("Module Name: ") + szBlock);

	Globals::g_upConsole->SetVisibility(true);

	// Get current module file version and compare with target version
	std::vector<BYTE> vecVersionData(GetFileVersionInfoSizeA(szBlock, NULL));
	if(!vecVersionData.size() || !GetFileVersionInfoA(szBlock, 0, vecVersionData.size(), vecVersionData.data())){
		Utils::LogError("Failed to open game to find version info!");
		return false;
	}

	std::pair<WORD, WORD>* lpTranslate{};
	UINT iTranslations{};
	if(!VerQueryValueA(vecVersionData.data(), "\\VarFileInfo\\Translation", reinterpret_cast<void**>(&lpTranslate), &iTranslations) || !lpTranslate || !iTranslations){
		Utils::LogError("Failed to query for game translations!");
		return false;
	}

	memset(szBlock, 0, sizeof(szBlock));
	snprintf(szBlock, sizeof(szBlock), "\\StringFileInfo\\%04x%04x\\ProductVersion", lpTranslate[0].first, lpTranslate[0].second);

	void* lpBuffer{};
	UINT iBytes{};

	if(!VerQueryValueA(vecVersionData.data(), szBlock, &lpBuffer, &iBytes) || !iBytes || !lpBuffer){
		Utils::LogError("Failed to find game version!");
		return false;
	}

	if(std::string(static_cast<char*>(lpBuffer)) != TARGET_VERSION) {
		Utils::LogError(std::string("Game version mismatch! Expected ") + TARGET_VERSION + ", got " + static_cast<char*>(lpBuffer));
		return false;
	}

	return true;
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
	bool bInitSuccess = Init();
	bool bOriginalVisibility = Globals::g_upConsole->GetVisibility();
	bInitSuccess &= VerifyGameVersion();

	(bInitSuccess) ? Utils::LogDebug("Initialization successful") : Utils::LogError("Initialization failed!");

	std::this_thread::sleep_for(std::chrono::seconds(5));
	Globals::g_upConsole->SetVisibility(bOriginalVisibility);

	MainLoop();
	Utils::LogDebug("Unloading...");

	// Shutdown DirectX 12 hook
	if (Dx12Hook::IsInitialized())
	{
		Utils::LogDebug("Shutting down DirectX 12 hook...");
		Dx12Hook::Shutdown();
	}

	// Uninitialize MinHook
	MH_STATUS mhStatus = MH_Uninitialize();
	if (mhStatus == MH_OK)
		Utils::LogDebug("MinHook uninitialized successfully");
	else
		Utils::LogHook("MinHook Uninitialize", mhStatus);

	// Give time for cleanup to complete
	std::this_thread::sleep_for(std::chrono::seconds(3));
	
	// Destroy console last
	Globals::g_upConsole.get()->Destroy();

	std::this_thread::sleep_for(std::chrono::seconds(1));
    FreeLibraryAndExitThread(Globals::g_hModule, 0);
    return TRUE;
}

LONG WINAPI ExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
	std::stringstream ssStackTrace;
	ssStackTrace << std::endl;
	auto stackTrace = std::stacktrace::current();
	for (const auto& frame : stackTrace) {
		ssStackTrace << frame << std::endl;
	}
	Utils::LogError(ssStackTrace.str());

	if (!Globals::g_bDebug)
		exit(EXIT_FAILURE);

	return EXCEPTION_EXECUTE_HANDLER;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReasonForCall, LPVOID lpReserved)
{
	if (ulReasonForCall != DLL_PROCESS_ATTACH)
		return TRUE;

	Globals::g_hModule = hModule;

	SetUnhandledExceptionFilter(ExceptionHandler);
	DisableThreadLibraryCalls(hModule);
	HANDLE hThread = CreateThread(NULL, 0, MainThread, hModule, 0, NULL);
	if (hThread)
		CloseHandle(hThread);

	return TRUE;
}
