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

import Utils.Console;
import Utils.Logging;
import Hook.Dx12Hook;
import Menu;

#include "Dumper-7/SDK.hpp"

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

static std::chrono::milliseconds g_durationPing{ 100 };

bool Init() 
{
	Globals::g_hBaseModule = GetModuleHandleA(NULL);

	Globals::g_upConsole = std::make_unique<Console>(Globals::g_bDebug, "Payday3-Internal Debug Console");
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

	size_t iNameHash = std::hash<std::string>{}(pObject->Class->Name.GetRawString());
	size_t iFuncHash = std::hash<std::string>{}(pFunction->GetName());

	std::string sClassName = pObject->GetName();
	std::string sFnName = pFunction->GetName();

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
		
	static auto nameBlueprintUpdateCamera = SDK::UKismetStringLibrary::Conv_StringToName(L"BlueprintUpdateCamera");
	static auto nameServerUpdateCamera = SDK::UKismetStringLibrary::Conv_StringToName(L"ServerUpdateCamera");
	static auto nameReceiveTick = SDK::UKismetStringLibrary::Conv_StringToName(L"ReceiveTick");
	static auto nameServerMovePacked = SDK::UKismetStringLibrary::Conv_StringToName(L"ServerMovePacked");

	if (pFunction->Name == nameBlueprintUpdateCamera){
		auto& params = *reinterpret_cast<SDK::Params::PlayerCameraManager_BlueprintUpdateCamera*>(pParams);
		UObjectProcessEvent_o(pObject, pFunction, pParams);

		return;
	}

	if (pFunction->Name == nameServerUpdateCamera){
		UObjectProcessEvent_o(pObject, pFunction, pParams);
		return;
	}

	if (sFnName.contains("ClientPlayForceFeedback_Internal")) {
		return;
	}

	if(pObject->IsA(SDK::ASBZKeypadBase::StaticClass()))
	{
		//std::cout << reinterpret_cast<const SDK::ASBZKeypadBase*>(pObject)->Code << "\n";
		UObjectProcessEvent_o(pObject, pFunction, pParams);
		return;
	}

	if(pObject->IsA(SDK::ACH_PlayerBase_C::StaticClass()) && pFunction->Name == nameServerMovePacked)
	{

		SDK::ASBZPlayerCharacter* pLocalPlayer = GetLocalPlayer();
		if (pLocalPlayer && pLocalPlayer->Interactor)
		{
			auto pInteractor = pLocalPlayer->Interactor;
			auto pInteraction = pInteractor->GetCurrentInteraction();
			if(pInteraction){
				static std::chrono::time_point<std::chrono::steady_clock> timeInteractLast = std::chrono::steady_clock::now();

				bool bSkipInstantInteraction = false;
				// Bags get us kicked ;|
				if(auto pOwner = pInteraction->GetOwner(); pOwner)
					bSkipInstantInteraction = pOwner->IsA(SDK::ASBZBagItem::StaticClass());
				

				if(!bSkipInstantInteraction && std::chrono::steady_clock::now() - timeInteractLast > g_durationPing){
					pInteractor->Server_CompleteInteraction(pInteraction, pInteractor->InteractId);
					pInteractor->Multicast_CompletedInteraction(pInteraction, false);

					// Dirty hack to prevent completing the interaction multiple times.
					timeInteractLast = std::chrono::steady_clock::now();
				}
			}
		}
	}

	if(pObject->IsA(SDK::ACH_PlayerBase_C::StaticClass()) || pObject->IsA(SDK::APlayerController::StaticClass()) || pObject->IsA(SDK::APlayerCameraManager::StaticClass()) || pObject->IsA(SDK::UCharacterMovementComponent::StaticClass()))
	{
		if(sFnName.contains("ServerMovePacked")){
			if(!Menu::g_bClientMove) 
				UObjectProcessEvent_o(pObject, pFunction, pParams);

			return;
		}
	}

	if(pObject->IsA(SDK::ASBZPlayerState::StaticClass())){
		if(sFnName.contains("Multicast_SetMiniGameState"))
		{
			UObjectProcessEvent_o(pObject, pFunction, pParams);

			SDK::ASBZPlayerCharacter* pLocalPlayer = GetLocalPlayer();
			if(pLocalPlayer && pLocalPlayer->PlayerState)
				pLocalPlayer->SBZPlayerState->Server_SetMiniGameState(SDK::EPD3MiniGameState::Success);
			

			return;
		}
	}

	UObjectProcessEvent_o(pObject, pFunction, pParams);
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
				MH_EnableHook(pFn);
		}

		SDK::UGameInstance* pGameInstance = pGWorld->OwningGameInstance;
		if (!pGameInstance)
			continue;

		SDK::ULocalPlayer* pLocalPlayer = pGameInstance->LocalPlayers[0];
		if (!pLocalPlayer)
			continue;

		SDK::ASBZPlayerController* pLocalPlayerController = reinterpret_cast<SDK::ASBZPlayerController*>(pLocalPlayer->PlayerController);
		if (!pLocalPlayerController || !pLocalPlayerController->IsA(SDK::ASBZPlayerController::StaticClass()))
			continue;

		
		SDK::ASBZPlayerCharacter* pLocalPlayerPawn = reinterpret_cast<SDK::ASBZPlayerCharacter*>(pLocalPlayerController->AcknowledgedPawn);
		if (!pLocalPlayerPawn || !pLocalPlayerPawn->IsA(SDK::ASBZPlayerCharacter::StaticClass()))
			continue;

		SDK::USBZPlayerMovementComponent* pMovementComponent = reinterpret_cast<SDK::USBZPlayerMovementComponent*>(pLocalPlayerPawn->GetComponentByClass(SDK::USBZPlayerMovementComponent::StaticClass()));
		if (!pMovementComponent)
			continue;

		if(pLocalPlayerPawn->SBZPlayerState)
			g_durationPing = std::chrono::milliseconds(static_cast<int>(pLocalPlayerPawn->SBZPlayerState->GetPingInMilliseconds() * 2.f));

		if(Menu::g_bClientMove){
			if (pLocalPlayerPawn->GetActorEnableCollision())
				pLocalPlayerPawn->SetActorEnableCollision(false);

			pMovementComponent->MovementMode = SDK::EMovementMode::MOVE_Flying;
		}
		else if(!pLocalPlayerPawn->GetActorEnableCollision()){
			pLocalPlayerPawn->SetActorEnableCollision(true);
			pMovementComponent->MovementMode = SDK::EMovementMode::MOVE_Walking;
		}

		

		//bool LineOfSightTo(const class AActor* Other, const struct FVector& ViewPoint, bool bAlternateChecks) const;
		
		pLocalPlayerPawn->CarryTiltDegrees = 0.0f;
		pLocalPlayerPawn->CarryTiltSpeed = 0.0f;
		pLocalPlayerPawn->CarryAdditionalTiltDegrees = 0.0f;

		//pLocalPlayerPawn->K2_SetActorLocation(pLocalPlayerPawn->K2_GetActorLocation() + (SDK::UKismetMathLibrary::GetForwardVector(pLocalPlayerPawn->K2_GetActorRotation()).GetNormalized() * 100.0f), false, nullptr, true);
		//pLocalPlayerPawn->Client_Teleport(, 0.f);

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
