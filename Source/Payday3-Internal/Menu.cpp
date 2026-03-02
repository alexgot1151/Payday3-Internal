#include <map>
#include <span>
#include <array>
#include <imgui.h>
#include <imgui_internal.h>
#include "Dumper-7/SDK.hpp"
#include "Config.hpp"
#include "Utils/Logging.hpp"
#include "Features/ESP/ESP.hpp"
#include "Menu.hpp"

void MultiSelectInternal(const char* szLabel, std::string& sPreviewText, std::span<std::tuple<const char*, const char*, bool&>> aEntries){
    if(!sPreviewText.size()){
        sPreviewText = "";

        for(const auto& tupleEntry : aEntries){
            if(!std::get<2>(tupleEntry))
                continue;

            if(sPreviewText.size())
                sPreviewText += ", ";
            
            sPreviewText += std::get<1>(tupleEntry);
        }
        
        sPreviewText = std::format("{}###{}", (!sPreviewText.size()) ? "None" : sPreviewText, szLabel);
    }

    if(!ImGui::BeginCombo(szLabel, sPreviewText.c_str()))
        return;

    ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
    
    bool bChanged = false;
    for(const auto& tupleEntry : aEntries)
        bChanged |= ImGui::Selectable(std::get<0>(tupleEntry), &std::get<2>(tupleEntry));

    ImGui::PopItemFlag();
    ImGui::EndCombo();

    if(!bChanged)
        return;

    sPreviewText = "";

    for(const auto& tupleEntry : aEntries){
        if(!std::get<2>(tupleEntry))
            continue;

        if(sPreviewText.size())
            sPreviewText += ", ";
        
        sPreviewText += std::get<1>(tupleEntry);
    }
    
    sPreviewText = std::format("{}###{}", (!sPreviewText.size()) ? "None" : sPreviewText, szLabel);
};

#define MultiSelect(szLabel, aData) { \
    static std::string sMultiSelectPreview{};\
    static auto aMultiSelectData = std::to_array<std::tuple<const char*, const char*, bool&>>aData;\
    MultiSelectInternal(szLabel, sMultiSelectPreview, aMultiSelectData);\
}

void Hotkey(const char* szLabel, Menu::Hotkey_t& bind){
    const auto id = ImGui::GetID(szLabel);
    ImGui::PushID(id);

    ImGui::TextUnformatted(szLabel);

    if(!bind.m_bFixedType){
        ImGui::SameLine();
        static const char* aTypes[] = { "Off", "On", "Hold", "Hold Off", "Toggle" };
        int iItem = static_cast<int>(bind.m_eType);
        ImGui::Combo("", &iItem, aTypes, IM_ARRAYSIZE(aTypes));
        bind.m_eType = static_cast<Menu::Hotkey_t::EType>(iItem);
    }
    
    if(bind.m_eType != Menu::Hotkey_t::EType::AlwaysOff && bind.m_eType != Menu::Hotkey_t::EType::AlwaysOn){
        ImGui::SameLine();
        if(ImGui::GetActiveID() == id){
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
            ImGui::Button("...");
            ImGui::PopStyleColor();
            ImGui::SetKeyOwner(ImGuiKey_Escape, id);

            ImGui::GetCurrentContext()->ActiveIdAllowOverlap = true;
            ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
            if((!ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[0]) || bind.SetToPressedKey())
                ImGui::ClearActiveID();
        } else if(ImGui::Button(bind.ToString()))
            ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
    }

    ImGui::PopID();
}


void CheatConfig::Aimbot_t::Draw(){
    ImGui::Checkbox("Enabled", &m_bEnabled);
    if(!m_bEnabled)
        return;

    ImGui::SliderFloat("Aim FOV", &m_flAimFOV, 0.f, 180.f, "%0.0f");

    static const char* aSortingItems[]{ "Smart", "FOV", "Threat" };
    ImGui::Combo("Sorting Method", reinterpret_cast<int*>(&m_eSorting), aSortingItems, IM_ARRAYSIZE(aSortingItems));

    MultiSelect("Targets", ({
        {"Guards", "Guards", m_bGuards},
        {"Specials", "Specials", m_bSpecials},
        {"FBI Van", "Van", m_bFBIVan},
        {"Civilians", "Civs", m_bCivilians}
    }));

    ImGui::Checkbox("Aim Through Walls", &m_bThroughWalls);
    ImGui::Checkbox("Loud Only", &m_bDisableInStealth);
}

void CheatConfig::Visuals_t::Draw(){
    auto& espConfig = ESP::GetConfig();
    ImGui::Checkbox("Enable ESP", &espConfig.bESP);
    if (espConfig.bESP) {
        ImGui::Indent();

        MultiSelect("Normal Enemies", ({
            {"Box", "B", espConfig.m_stNormalEnemies.m_bBox},
            {"Health", "H", espConfig.m_stNormalEnemies.m_bHealth},
            {"Armor", "A", espConfig.m_stNormalEnemies.m_bArmor},
            {"Name", "N", espConfig.m_stNormalEnemies.m_bName},
            {"Flags", "F", espConfig.m_stNormalEnemies.m_bFlags},
            {"Skeleton", "S", espConfig.m_stNormalEnemies.m_bSkeleton},
            {"Outline", "O", espConfig.m_stNormalEnemies.m_bOutline}
        }));

        MultiSelect("Special Enemies", ({
            {"Box", "B", espConfig.m_stSpecialEnemies.m_bBox},
            {"Health", "H", espConfig.m_stSpecialEnemies.m_bHealth},
            {"Armor", "A", espConfig.m_stSpecialEnemies.m_bArmor},
            {"Name", "N", espConfig.m_stSpecialEnemies.m_bName},
            {"Flags", "F", espConfig.m_stSpecialEnemies.m_bFlags},
            {"Skeleton", "S", espConfig.m_stSpecialEnemies.m_bSkeleton},
            {"Outline", "O", espConfig.m_stSpecialEnemies.m_bOutline}
        }));

#ifdef _DEBUG
        ImGui::Checkbox("Debug Draw Bone Indices", &espConfig.bDebugDrawBoneIndices);
        ImGui::Checkbox("Debug Draw Bone Names Instead of Indices", &espConfig.bDebugDrawBoneNames);
#endif
        
#ifdef _DEBUG
        ImGui::Checkbox("Debug Skeleton", &espConfig.bDebugSkeleton);
        if (espConfig.bDebugSkeleton) {
            ImGui::Indent();
            ImGui::Checkbox("Debug Skeleton Draw Bone Indices", &espConfig.bDebugSkeletonDrawBoneIndices);
            ImGui::Checkbox("Debug Skeleton Draw Bone Names Instead of Indices", &espConfig.bDebugSkeletonDrawBoneNames);
            ImGui::Unindent();
        }
#endif
        ImGui::Unindent();
    }
#ifdef _DEBUG
    ImGui::Checkbox("Debug ESP (Show Class Names)", &espConfig.bDebugESP);
#endif
}




void CheatConfig::Misc_t::Draw(){
    Hotkey("Client Move", m_keyClientMove);
    if(m_keyClientMove.m_eType != Menu::Hotkey_t::EType::AlwaysOff){
        ImGui::Indent();
        Hotkey("Teleport", m_keyClientMoveTeleport);
        Hotkey("Move Faster", m_keyClientMoveFaster);
        ImGui::SliderFloat("Speed###ClientMoveSpeed", &m_flClientMoveBaseSpeed, 500.f, 5000.f);
        ImGui::Checkbox("Auto Teleport", &m_bClientMoveAutoTeleport);
        ImGui::Unindent();
    }
    

    MultiSelect("Removals", ({
        {"No Spread", "Spread", m_bNoSpread},
        {"No Recoil", "Recoil", m_bNoRecoil},
        {"No Fall Damage", "Fall", m_bNoFallDamage},
        {"Instant Interaction", "Interact", m_bInstantInteraction},
        {"Instant Minigame", "Minigame", m_bInstantMinigame},
        {"Instant Reload", "Reload", m_bInstantReload},
        {"Instant Melee", "Melee", m_bInstantMelee}
    }));

    MultiSelect("Camera Modifiers", ({
        {"Disable Shake", "Shake", m_bNoCameraShake},
        {"Disable Tilt", "Tilt", m_bNoCameraTilt}
    }));

    ImGui::SliderFloat("Camera FOV", &m_flCameraFOV, 0.f, 150.f, "%0.0f");

    MultiSelect("Buffs", ({
        {"Speed", "Speed", m_bSpeedBuff},
        {"Damage", "Damage", m_bDamageBuff},
        {"Armor", "Armor", m_bArmorBuff}
    }));

    ImGui::Checkbox("More Bullets", &m_bMoreBullets);
    if(m_bMoreBullets){
        ImGui::SameLine();
        ImGui::SliderInt("###More Bullets Count", &m_iMoreBullets, 1, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
    }

    ImGui::Checkbox("Super Toss", &m_bSuperToss);
    if(m_bSuperToss){
        ImGui::SameLine();
        ImGui::SliderFloat("###Super Toss Speed", &m_flSuperToss, 1000.f, 5000.f);
    }
}

namespace Menu
{
    void CallTraceEntry_t::Draw()
    {
        if(!g_sCallTraceFilter.empty() && !m_sClassName.contains(g_sCallTraceFilter)){
            if(!g_bCallTraceFilterSubclasses)
                return;
            
            if(std::find_if(m_vecSubClasses.begin(), m_vecSubClasses.end(), [&](const std::string& str) {
                return str.contains(g_sCallTraceFilter);
            }) == m_vecSubClasses.end())
                return;
        }

        if(ImGui::CollapsingHeader(m_sClassName.c_str()))
        {
            ImGui::PushID(m_sClassName.c_str());

            if(m_vecSubClasses.size()){
                if(ImGui::TreeNode("Sub Classes"))
                {
                    for(const auto& str : m_vecSubClasses)
                        ImGui::Text("%s", str.c_str());
                    ImGui::TreePop();
                }
            }
            
            if(ImGui::TreeNode("Called Functions"))
            {
                for(const auto& pairEntry : m_mapCalledFunctions)
                    ImGui::Text("%s", pairEntry.second.c_str());
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    void PreDraw()
    {
        CheatConfig::Get().m_misc.m_keyClientMove.UpdateState();
        CheatConfig::Get().m_misc.m_keyClientMoveTeleport.UpdateState();
        CheatConfig::Get().m_misc.m_keyClientMoveFaster.UpdateState();

        SDK::UWorld* pGWorld = SDK::UWorld::GetWorld();
        if (!pGWorld)
            return;

        SDK::UGameInstance* pGameInstance = pGWorld->OwningGameInstance;
        if (!pGameInstance)
            return;

        SDK::ULocalPlayer* pLocalPlayer = pGameInstance->LocalPlayers[0];
        if (!pLocalPlayer)
            return;

        SDK::APlayerController* pPlayerController = pLocalPlayer->PlayerController;
        if (!pPlayerController)
            return;

        SDK::ULevel* pPersistentLevel = pGWorld->PersistentLevel;
        if (!pPersistentLevel)
            return;

        ESP::Render(pGWorld, pPlayerController);
        ESP::RenderDebugESP(pPersistentLevel, pPlayerController);
    }

    

	// Draw the main menu content
	void Draw(bool& bShowMenu)
	{
		if (!bShowMenu)
			return;

        

        std::string windowTitle = std::format("Omegaware Pd3 Internal - {}", CURRENT_VERSION);
        ImGui::Begin(windowTitle.c_str(), &bShowMenu, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

        if(ImGui::BeginTabBar("CheatTabs"))
        {
            if(ImGui::BeginTabItem("Aimbot")){
                CheatConfig::Get().m_aimbot.Draw();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Visuals")){
                CheatConfig::Get().m_visuals.Draw();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Misc")){
                CheatConfig::Get().m_misc.Draw();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
		
		// Performance metrics
		//ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);


        ImGui::End();


        ImGui::Begin("Call Traces",  &bShowMenu);
        static const char* aCallTraceItems[] = { "Inactive", "UObject", "PlayerController" };
        if(ImGui::Combo("Call Trace Area", reinterpret_cast<int*>(&g_eCallTraceArea), aCallTraceItems, IM_ARRAYSIZE(aCallTraceItems)))
            g_mapCallTraces.clear();
        
        ImGui::InputText("##Filter", g_szCallTraceFilter, sizeof(g_szCallTraceFilter));
        ImGui::SameLine();
        ImGui::Checkbox("##Filter Use Subclasses", &g_bCallTraceFilterSubclasses);

        g_sCallTraceFilter = g_szCallTraceFilter;

        ImGui::Separator();
        for (auto& pairEntry : g_mapCallTraces)
            pairEntry.second.Draw();
        ImGui::End();
	}

    void PostDraw() {}
}
