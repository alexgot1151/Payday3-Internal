#include <map>
#include <imgui.h>
#include <imgui_internal.h>
#include "Dumper-7/SDK.hpp"
#include "Config.hpp"
#include "Utils/Logging.hpp"
#include "Features/ESP/ESP.hpp"
#include "Menu.hpp"

std::string CreateMultiSelectPreviewText(const std::vector<std::pair<bool, const char*>>& vecInput, const char* szUnique){
    std::string sReturned = "";

    static auto fnAppend = [](std::string& sText, bool bAppend, const char* szAppendText) {
        if (!bAppend)
            return;

        if (sText.size())
            sText += ", ";

        sText += szAppendText;
    };

    for(const auto& pairEntry : vecInput)
        fnAppend(sReturned, pairEntry.first, pairEntry.second);
    
    return std::format("{}###{}", (!sReturned.size()) ? "None" : sReturned, szUnique);
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

    static std::string sTargetsPreview = CreateMultiSelectPreviewText({
        { m_bGuards, "Guards" },
        { m_bSpecials, "Specials" },
        { m_bFBIVan, "FBI Van" },
        { m_bCivilians, "Civilians" },
    }, "Targets");

    if(ImGui::BeginCombo("Targets", sTargetsPreview.c_str()))
    {
        ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
        
        if (ImGui::Selectable("Guards", &m_bGuards) ||
            ImGui::Selectable("Specials", &m_bSpecials) ||
            ImGui::Selectable("FBI Van", &m_bFBIVan) ||
            ImGui::Selectable("Civilians", &m_bCivilians)) {
            sTargetsPreview = CreateMultiSelectPreviewText({
                { m_bGuards, "Guards" },
                { m_bSpecials, "Specials" },
                { m_bFBIVan, "FBI Van" },
                { m_bCivilians, "Civilians" },
            }, "Targets");
        }
            
        ImGui::PopItemFlag();
        ImGui::EndCombo();
    }

    ImGui::Checkbox("Aim Through Walls", &m_bThroughWalls);
    ImGui::Checkbox("Loud Only", &m_bDisableInStealth);
}

void DrawEnemyESPSection(const char* szType, ESP::EnemyESP& stSettings)
{
    if(!ImGui::BeginCombo(szType, std::format("{}###{}", stSettings.m_sPreviewText, szType).c_str()))
        return;

    ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
    if (ImGui::Selectable("Box", &stSettings.m_bBox) ||
        ImGui::Selectable("Health", &stSettings.m_bHealth) ||
        ImGui::Selectable("Armor", &stSettings.m_bArmor) ||
        ImGui::Selectable("Name", &stSettings.m_bName) ||
        ImGui::Selectable("Flags", &stSettings.m_bFlags) ||
        ImGui::Selectable("Skeleton", &stSettings.m_bSkeleton) ||
        ImGui::Selectable("Outline", &stSettings.m_bOutline))
        stSettings.UpdatePreviewText();
    ImGui::PopItemFlag();

    ImGui::EndCombo();
}

void CheatConfig::Visuals_t::Draw(){
    auto& espConfig = ESP::GetConfig();
    ImGui::Checkbox("Enable ESP", &espConfig.bESP);
    if (espConfig.bESP) {
        ImGui::Indent();

        DrawEnemyESPSection("Normal Enemies", espConfig.m_stNormalEnemies);
        DrawEnemyESPSection("Special Enemies", espConfig.m_stSpecialEnemies);

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
    Hotkey("Client Move Teleport", m_keyClientMoveTeleport);


    static std::string sRemovalsPreview = CreateMultiSelectPreviewText({
        { m_bNoSpread, "No Spread" },
        { m_bNoRecoil, "No Recoil" },
        { m_bNoCameraShake, "No Camera Shake" },
        { m_bInstantInteraction, "Instant Interaction" },
        { m_bInstantMinigame, "Instant Minigame" }
    }, "Removals");

    if(ImGui::BeginCombo("Removals", sRemovalsPreview.c_str()))
    {
        ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
        
        if (ImGui::Selectable("No Spread", &m_bNoSpread) ||
            ImGui::Selectable("No Recoil", &m_bNoRecoil) ||
            ImGui::Selectable("No Camera Shake", &m_bNoCameraShake) ||
            ImGui::Selectable("Instant Interaction", &m_bInstantInteraction) ||
            ImGui::Selectable("Instant Minigame", &m_bInstantMinigame)) {
            sRemovalsPreview = CreateMultiSelectPreviewText({
                { m_bNoSpread, "No Spread" },
                { m_bNoRecoil, "No Recoil" },
                { m_bNoCameraShake, "No Camera Shake" },
                { m_bInstantInteraction, "Instant Interaction" },
                { m_bInstantMinigame, "Instant Minigame" }
            }, "Removals");
        }
            
        ImGui::PopItemFlag();
        ImGui::EndCombo();
    }

    static std::string sBuffsPreview = CreateMultiSelectPreviewText({
        { m_bSpeedBuff, "Speed" },
        { m_bDamageBuff, "Damage" },
        { m_bArmorBuff, "Armor" }
    }, "Buffs");

    if(ImGui::BeginCombo("Buffs", sBuffsPreview.c_str()))
    {
        ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
        
        if (ImGui::Selectable("Speed", &m_bSpeedBuff) ||
            ImGui::Selectable("Damage", &m_bDamageBuff) ||
            ImGui::Selectable("Armor", &m_bArmorBuff)) {
            sBuffsPreview = CreateMultiSelectPreviewText({
                { m_bSpeedBuff, "Speed" },
                { m_bDamageBuff, "Damage" },
                { m_bArmorBuff, "Armor" }
            }, "Buffs");
        }
            
        ImGui::PopItemFlag();
        ImGui::EndCombo();
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
