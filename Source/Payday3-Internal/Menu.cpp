#include <map>
#include <imgui.h>
#include "Dumper-7/SDK.hpp"
#include "Config.hpp"
#include "Utils/Logging.hpp"
#include "Features/ESP/ESP.hpp"
#include "Menu.hpp"


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

    void DrawEnemyESPSection(const char* szType, ESP::EnemyESP& stSettings)
    {
        if(!ImGui::BeginCombo(szType, std::format("{}###{}", stSettings.m_sPreviewText, szType).c_str()))
            return;

        if (ImGui::Selectable("Box", &stSettings.m_bBox) ||
            ImGui::Selectable("Health", &stSettings.m_bHealth) ||
            ImGui::Selectable("Armor", &stSettings.m_bArmor) ||
            ImGui::Selectable("Name", &stSettings.m_bName) ||
            ImGui::Selectable("Flags", &stSettings.m_bFlags) ||
            ImGui::Selectable("Skeleton", &stSettings.m_bSkeleton) ||
            ImGui::Selectable("Outline", &stSettings.m_bOutline))
            stSettings.UpdatePreviewText();

        ImGui::EndCombo();
    }

	// Draw the main menu content
	void Draw(bool& bShowMenu)
	{
		if (!bShowMenu)
			return;

        auto& espConfig = ESP::GetConfig();

        std::string windowTitle = std::format("Payday 3 Internal - {}", CURRENT_VERSION);
        ImGui::Begin(windowTitle.c_str(), &bShowMenu, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
		// Header
		ImGui::Text("DirectX 12 Hook Active");
		ImGui::Text("Press INSERT to toggle menu");
		ImGui::Separator();

        ImGui::Checkbox("Client Move", &g_bClientMove);
		
		// Performance metrics
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 
			1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		
		ImGui::Separator();
		
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

        ImGui::End();


        ImGui::Begin("Call Traces",  &bShowMenu);
        if(ImGui::Button("Clear"))
            ImGui::OpenPopup("Confirm Clear");
            
        if (ImGui::BeginPopupModal("Confirm Clear", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Are you sure you want to clear all entries?");
            ImGui::Separator();

            if (ImGui::Button("Yes")) {
                ImGui::CloseCurrentPopup();
                g_mapCallTraces.clear();
            }

            ImGui::SameLine();

            if (ImGui::Button("No"))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
        
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
