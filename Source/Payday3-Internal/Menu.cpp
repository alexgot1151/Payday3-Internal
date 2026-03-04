#include <map>
#include <span>
#include <array>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_map>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#undef min
#undef max

#include <imgui.h>
#include <imgui_internal.h>
#include "Dumper-7/SDK.hpp"
#include "Config.hpp"
#include "Utils/Logging.hpp"
#include "Features/ESP/ESP.hpp"
#include "Menu.hpp"

namespace
{
    constexpr int g_iConfigVersion = 1;

    std::string Trim(std::string_view svValue)
    {
        size_t iBegin = 0;
        size_t iEnd = svValue.size();

        while (iBegin < iEnd && std::isspace(static_cast<unsigned char>(svValue[iBegin])))
            ++iBegin;

        while (iEnd > iBegin && std::isspace(static_cast<unsigned char>(svValue[iEnd - 1])))
            --iEnd;

        return std::string(svValue.substr(iBegin, iEnd - iBegin));
    }

    bool TryParseBool(const std::string& sValue, bool& bOut)
    {
        std::string sNormalized = sValue;
        std::transform(sNormalized.begin(), sNormalized.end(), sNormalized.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (sNormalized == "1" || sNormalized == "true")
        {
            bOut = true;
            return true;
        }

        if (sNormalized == "0" || sNormalized == "false")
        {
            bOut = false;
            return true;
        }

        return false;
    }

    bool TryParseInt(const std::string& sValue, int& iOut)
    {
        try
        {
            size_t iPos = 0;
            const int iParsed = std::stoi(sValue, &iPos);
            if (iPos != sValue.size())
                return false;

            iOut = iParsed;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool TryParseFloat(const std::string& sValue, float& flOut)
    {
        try
        {
            size_t iPos = 0;
            const float flParsed = std::stof(sValue, &iPos);
            if (iPos != sValue.size())
                return false;

            flOut = flParsed;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::filesystem::path GetConfigPath()
    {
        char szModulePath[MAX_PATH]{};
        if (GetModuleFileNameA(nullptr, szModulePath, MAX_PATH) == 0)
            return "payday3_internal.cfg";

        std::filesystem::path pathConfigDir = std::filesystem::path(szModulePath).parent_path() / "Configs";
        std::error_code ec{};
        std::filesystem::create_directories(pathConfigDir, ec);

        return pathConfigDir / "payday3_internal.cfg";
    }
}

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

bool CheatConfig::Save() const
{
    const auto pathConfig = GetConfigPath();
    std::ofstream fileConfig(pathConfig, std::ios::trunc);
    if (!fileConfig.is_open() || fileConfig.fail())
    {
        Utils::LogError(std::format("Failed to open config for writing: {}", pathConfig.string()));
        return false;
    }

    auto WriteBool = [&](const char* szKey, const bool bValue) {
        fileConfig << szKey << '=' << (bValue ? 1 : 0) << '\n';
    };

    auto WriteInt = [&](const char* szKey, const int iValue) {
        fileConfig << szKey << '=' << iValue << '\n';
    };

    auto WriteFloat = [&](const char* szKey, const float flValue) {
        fileConfig << szKey << '=' << flValue << '\n';
    };

    fileConfig << "# Payday3-Internal config\n";
    WriteInt("config.version", g_iConfigVersion);

    WriteBool("aimbot.enabled", m_aimbot.m_bEnabled);
    WriteFloat("aimbot.fov", m_aimbot.m_flAimFOV);
    WriteInt("aimbot.sorting", static_cast<int>(m_aimbot.m_eSorting));
    WriteBool("aimbot.targets.guards", m_aimbot.m_bGuards);
    WriteBool("aimbot.targets.specials", m_aimbot.m_bSpecials);
    WriteBool("aimbot.targets.fbiVan", m_aimbot.m_bFBIVan);
    WriteBool("aimbot.targets.civilians", m_aimbot.m_bCivilians);
    WriteBool("aimbot.throughWalls", m_aimbot.m_bThroughWalls);
    WriteBool("aimbot.disableInStealth", m_aimbot.m_bDisableInStealth);

    WriteInt("misc.keyClientMove.key", static_cast<int>(m_misc.m_keyClientMove.m_eKeyCode));
    WriteInt("misc.keyClientMove.type", static_cast<int>(m_misc.m_keyClientMove.m_eType));
    WriteInt("misc.keyClientMoveTeleport.key", static_cast<int>(m_misc.m_keyClientMoveTeleport.m_eKeyCode));
    WriteInt("misc.keyClientMoveTeleport.type", static_cast<int>(m_misc.m_keyClientMoveTeleport.m_eType));
    WriteInt("misc.keyClientMoveFaster.key", static_cast<int>(m_misc.m_keyClientMoveFaster.m_eKeyCode));
    WriteInt("misc.keyClientMoveFaster.type", static_cast<int>(m_misc.m_keyClientMoveFaster.m_eType));

    WriteBool("misc.clientMove.autoTeleport", m_misc.m_bClientMoveAutoTeleport);
    WriteFloat("misc.clientMove.baseSpeed", m_misc.m_flClientMoveBaseSpeed);

    WriteBool("misc.noSpread", m_misc.m_bNoSpread);
    WriteBool("misc.noRecoil", m_misc.m_bNoRecoil);
    WriteBool("misc.noFallDamage", m_misc.m_bNoFallDamage);
    WriteBool("misc.instantInteraction", m_misc.m_bInstantInteraction);
    WriteBool("misc.instantMinigame", m_misc.m_bInstantMinigame);
    WriteBool("misc.instantReload", m_misc.m_bInstantReload);
    WriteBool("misc.instantMelee", m_misc.m_bInstantMelee);
    WriteBool("misc.autoPistol", m_misc.m_bAutoPistol);

    WriteBool("misc.speedBuff", m_misc.m_bSpeedBuff);
    WriteBool("misc.damageBuff", m_misc.m_bDamageBuff);
    WriteBool("misc.armorBuff", m_misc.m_bArmorBuff);

    WriteBool("misc.noCameraShake", m_misc.m_bNoCameraShake);
    WriteBool("misc.noCameraTilt", m_misc.m_bNoCameraTilt);
    WriteFloat("misc.cameraFov", m_misc.m_flCameraFOV);

    WriteInt("misc.rapidFire", m_misc.m_iRapidFire);

    WriteBool("misc.moreBullets.enabled", m_misc.m_bMoreBullets);
    WriteInt("misc.moreBullets.count", m_misc.m_iMoreBullets);

    WriteBool("misc.superToss.enabled", m_misc.m_bSuperToss);
    WriteFloat("misc.superToss.velocity", m_misc.m_flSuperToss);

    const auto& espConfig = ESP::GetConfig();
    WriteBool("esp.enabled", espConfig.bESP);
    WriteBool("esp.normal.box", espConfig.m_stNormalEnemies.m_bBox);
    WriteBool("esp.normal.health", espConfig.m_stNormalEnemies.m_bHealth);
    WriteBool("esp.normal.armor", espConfig.m_stNormalEnemies.m_bArmor);
    WriteBool("esp.normal.name", espConfig.m_stNormalEnemies.m_bName);
    WriteBool("esp.normal.flags", espConfig.m_stNormalEnemies.m_bFlags);
    WriteBool("esp.normal.skeleton", espConfig.m_stNormalEnemies.m_bSkeleton);
    WriteBool("esp.normal.outline", espConfig.m_stNormalEnemies.m_bOutline);

    WriteBool("esp.special.box", espConfig.m_stSpecialEnemies.m_bBox);
    WriteBool("esp.special.health", espConfig.m_stSpecialEnemies.m_bHealth);
    WriteBool("esp.special.armor", espConfig.m_stSpecialEnemies.m_bArmor);
    WriteBool("esp.special.name", espConfig.m_stSpecialEnemies.m_bName);
    WriteBool("esp.special.flags", espConfig.m_stSpecialEnemies.m_bFlags);
    WriteBool("esp.special.skeleton", espConfig.m_stSpecialEnemies.m_bSkeleton);
    WriteBool("esp.special.outline", espConfig.m_stSpecialEnemies.m_bOutline);

    WriteBool("esp.civilians.box", espConfig.m_stCivilians.m_bBox);
    WriteBool("esp.civilians.flags", espConfig.m_stCivilians.m_bFlags);
    WriteBool("esp.civilians.skeleton", espConfig.m_stCivilians.m_bSkeleton);
    WriteBool("esp.civilians.outline", espConfig.m_stCivilians.m_bOutline);
    WriteBool("esp.civilians.onlyWhenSpecial", espConfig.m_stCivilians.m_bOnlyWhenSpecial);

    WriteBool("esp.debug.skeleton", espConfig.bDebugSkeleton);
    WriteBool("esp.debug.drawBoneIndices", espConfig.bDebugDrawBoneIndices);
    WriteBool("esp.debug.drawBoneNames", espConfig.bDebugDrawBoneNames);
    WriteBool("esp.debug.skeletonDrawBoneIndices", espConfig.bDebugSkeletonDrawBoneIndices);
    WriteBool("esp.debug.skeletonDrawBoneNames", espConfig.bDebugSkeletonDrawBoneNames);
    WriteBool("esp.debug.esp", espConfig.bDebugESP);

    LootESP::Config& lootespConfig = LootESP::GetConfig();
    WriteBool("lootesp.enabled", lootespConfig.bESP);

    if (fileConfig.fail())
    {
        Utils::LogError(std::format("Failed to write config: {}", pathConfig.string()));
        return false;
    }

    Utils::LogDebug(std::format("Config saved: {}", pathConfig.string()));
    return true;
}

bool CheatConfig::Load()
{
    const auto pathConfig = GetConfigPath();
    std::ifstream fileConfig(pathConfig);
    if (!fileConfig.is_open() || fileConfig.fail())
    {
        Utils::LogDebug(std::format("Config file not found, using defaults: {}", pathConfig.string()));
        return false;
    }

    std::unordered_map<std::string, std::string> mapConfigValues{};
    std::string sLine{};

    while (std::getline(fileConfig, sLine))
    {
        const std::string sTrimmed = Trim(sLine);
        if (sTrimmed.empty() || sTrimmed[0] == '#' || sTrimmed[0] == ';')
            continue;

        const size_t iEquals = sTrimmed.find('=');
        if (iEquals == std::string::npos)
            continue;

        const std::string sKey = Trim(std::string_view(sTrimmed).substr(0, iEquals));
        const std::string sValue = Trim(std::string_view(sTrimmed).substr(iEquals + 1));
        if (!sKey.empty())
            mapConfigValues[sKey] = sValue;
    }

    auto ReadBool = [&](const char* szKey, bool& bOut) -> bool {
        const auto itr = mapConfigValues.find(szKey);
        if (itr == mapConfigValues.end())
            return false;

        bool bParsed{};
        if (!TryParseBool(itr->second, bParsed))
            return false;

        bOut = bParsed;
        return true;
    };

    auto ReadInt = [&](const char* szKey, int& iOut) -> bool {
        const auto itr = mapConfigValues.find(szKey);
        if (itr == mapConfigValues.end())
            return false;

        int iParsed{};
        if (!TryParseInt(itr->second, iParsed))
            return false;

        iOut = iParsed;
        return true;
    };

    auto ReadFloat = [&](const char* szKey, float& flOut) -> bool {
        const auto itr = mapConfigValues.find(szKey);
        if (itr == mapConfigValues.end())
            return false;

        float flParsed{};
        if (!TryParseFloat(itr->second, flParsed))
            return false;

        flOut = flParsed;
        return true;
    };

    int iVersion{};
    if (ReadInt("config.version", iVersion) && iVersion != g_iConfigVersion)
        Utils::LogDebug(std::format("Loading config version {} with parser version {}", iVersion, g_iConfigVersion));

    ReadBool("aimbot.enabled", m_aimbot.m_bEnabled);
    if (ReadFloat("aimbot.fov", m_aimbot.m_flAimFOV))
        m_aimbot.m_flAimFOV = std::clamp(m_aimbot.m_flAimFOV, 0.0f, 180.0f);

    int iSorting{};
    if (ReadInt("aimbot.sorting", iSorting)
        && iSorting >= static_cast<int>(Aimbot_t::ESorting::Smart)
        && iSorting <= static_cast<int>(Aimbot_t::ESorting::Threat))
    {
        m_aimbot.m_eSorting = static_cast<Aimbot_t::ESorting>(iSorting);
    }

    ReadBool("aimbot.targets.guards", m_aimbot.m_bGuards);
    ReadBool("aimbot.targets.specials", m_aimbot.m_bSpecials);
    ReadBool("aimbot.targets.fbiVan", m_aimbot.m_bFBIVan);
    ReadBool("aimbot.targets.civilians", m_aimbot.m_bCivilians);
    ReadBool("aimbot.throughWalls", m_aimbot.m_bThroughWalls);
    ReadBool("aimbot.disableInStealth", m_aimbot.m_bDisableInStealth);

    int iHotkeyKey{};
    if (ReadInt("misc.keyClientMove.key", iHotkeyKey))
        m_misc.m_keyClientMove.m_eKeyCode = static_cast<ImGuiKey>(iHotkeyKey);

    int iHotkeyType{};
    if (ReadInt("misc.keyClientMove.type", iHotkeyType)
        && iHotkeyType >= static_cast<int>(Menu::Hotkey_t::EType::AlwaysOff)
        && iHotkeyType <= static_cast<int>(Menu::Hotkey_t::EType::Toggle))
    {
        m_misc.m_keyClientMove.m_eType = static_cast<Menu::Hotkey_t::EType>(iHotkeyType);
    }

    if (ReadInt("misc.keyClientMoveTeleport.key", iHotkeyKey))
        m_misc.m_keyClientMoveTeleport.m_eKeyCode = static_cast<ImGuiKey>(iHotkeyKey);

    if (ReadInt("misc.keyClientMoveTeleport.type", iHotkeyType)
        && iHotkeyType >= static_cast<int>(Menu::Hotkey_t::EType::AlwaysOff)
        && iHotkeyType <= static_cast<int>(Menu::Hotkey_t::EType::Toggle))
    {
        m_misc.m_keyClientMoveTeleport.m_eType = static_cast<Menu::Hotkey_t::EType>(iHotkeyType);
    }

    if (ReadInt("misc.keyClientMoveFaster.key", iHotkeyKey))
        m_misc.m_keyClientMoveFaster.m_eKeyCode = static_cast<ImGuiKey>(iHotkeyKey);

    if (ReadInt("misc.keyClientMoveFaster.type", iHotkeyType)
        && iHotkeyType >= static_cast<int>(Menu::Hotkey_t::EType::AlwaysOff)
        && iHotkeyType <= static_cast<int>(Menu::Hotkey_t::EType::Toggle))
    {
        m_misc.m_keyClientMoveFaster.m_eType = static_cast<Menu::Hotkey_t::EType>(iHotkeyType);
    }

    ReadBool("misc.clientMove.autoTeleport", m_misc.m_bClientMoveAutoTeleport);
    if (ReadFloat("misc.clientMove.baseSpeed", m_misc.m_flClientMoveBaseSpeed))
        m_misc.m_flClientMoveBaseSpeed = std::max(0.0f, m_misc.m_flClientMoveBaseSpeed);

    ReadBool("misc.noSpread", m_misc.m_bNoSpread);
    ReadBool("misc.noRecoil", m_misc.m_bNoRecoil);
    ReadBool("misc.noFallDamage", m_misc.m_bNoFallDamage);
    ReadBool("misc.instantInteraction", m_misc.m_bInstantInteraction);
    ReadBool("misc.instantMinigame", m_misc.m_bInstantMinigame);
    ReadBool("misc.instantReload", m_misc.m_bInstantReload);
    ReadBool("misc.instantMelee", m_misc.m_bInstantMelee);
    ReadBool("misc.autoPistol", m_misc.m_bAutoPistol);

    ReadBool("misc.speedBuff", m_misc.m_bSpeedBuff);
    ReadBool("misc.damageBuff", m_misc.m_bDamageBuff);
    ReadBool("misc.armorBuff", m_misc.m_bArmorBuff);

    ReadBool("misc.noCameraShake", m_misc.m_bNoCameraShake);
    ReadBool("misc.noCameraTilt", m_misc.m_bNoCameraTilt);
    ReadFloat("misc.cameraFov", m_misc.m_flCameraFOV);

    int iRapidFire{};
    if (ReadInt("misc.rapidFire", iRapidFire))
        m_misc.m_iRapidFire = std::clamp(iRapidFire, 0, 2);

    ReadBool("misc.moreBullets.enabled", m_misc.m_bMoreBullets);
    int iMoreBullets{};
    if (ReadInt("misc.moreBullets.count", iMoreBullets))
        m_misc.m_iMoreBullets = std::max(1, iMoreBullets);

    ReadBool("misc.superToss.enabled", m_misc.m_bSuperToss);
    if (ReadFloat("misc.superToss.velocity", m_misc.m_flSuperToss))
        m_misc.m_flSuperToss = std::max(0.0f, m_misc.m_flSuperToss);

    auto& espConfig = ESP::GetConfig();
    ReadBool("esp.enabled", espConfig.bESP);

    ReadBool("esp.normal.box", espConfig.m_stNormalEnemies.m_bBox);
    ReadBool("esp.normal.health", espConfig.m_stNormalEnemies.m_bHealth);
    ReadBool("esp.normal.armor", espConfig.m_stNormalEnemies.m_bArmor);
    ReadBool("esp.normal.name", espConfig.m_stNormalEnemies.m_bName);
    ReadBool("esp.normal.flags", espConfig.m_stNormalEnemies.m_bFlags);
    ReadBool("esp.normal.skeleton", espConfig.m_stNormalEnemies.m_bSkeleton);
    ReadBool("esp.normal.outline", espConfig.m_stNormalEnemies.m_bOutline);

    ReadBool("esp.special.box", espConfig.m_stSpecialEnemies.m_bBox);
    ReadBool("esp.special.health", espConfig.m_stSpecialEnemies.m_bHealth);
    ReadBool("esp.special.armor", espConfig.m_stSpecialEnemies.m_bArmor);
    ReadBool("esp.special.name", espConfig.m_stSpecialEnemies.m_bName);
    ReadBool("esp.special.flags", espConfig.m_stSpecialEnemies.m_bFlags);
    ReadBool("esp.special.skeleton", espConfig.m_stSpecialEnemies.m_bSkeleton);
    ReadBool("esp.special.outline", espConfig.m_stSpecialEnemies.m_bOutline);

    ReadBool("esp.civilians.box", espConfig.m_stCivilians.m_bBox);
    ReadBool("esp.civilians.flags", espConfig.m_stCivilians.m_bFlags);
    ReadBool("esp.civilians.skeleton", espConfig.m_stCivilians.m_bSkeleton);
    ReadBool("esp.civilians.outline", espConfig.m_stCivilians.m_bOutline);
    ReadBool("esp.civilians.onlyWhenSpecial", espConfig.m_stCivilians.m_bOnlyWhenSpecial);

    ReadBool("esp.debug.skeleton", espConfig.bDebugSkeleton);
    ReadBool("esp.debug.drawBoneIndices", espConfig.bDebugDrawBoneIndices);
    ReadBool("esp.debug.drawBoneNames", espConfig.bDebugDrawBoneNames);
    ReadBool("esp.debug.skeletonDrawBoneIndices", espConfig.bDebugSkeletonDrawBoneIndices);
    ReadBool("esp.debug.skeletonDrawBoneNames", espConfig.bDebugSkeletonDrawBoneNames);
    ReadBool("esp.debug.esp", espConfig.bDebugESP);

    m_misc.m_keyClientMove.m_bActive = false;
    m_misc.m_keyClientMove.m_bPressedThisFrame = false;
    m_misc.m_keyClientMoveTeleport.m_bActive = false;
    m_misc.m_keyClientMoveTeleport.m_bPressedThisFrame = false;
    m_misc.m_keyClientMoveFaster.m_bActive = false;
    m_misc.m_keyClientMoveFaster.m_bPressedThisFrame = false;

    auto& lootespConfig = LootESP::GetConfig();
    ReadBool("lootesp.enabled", lootespConfig.bESP);

    Utils::LogDebug(std::format("Config loaded: {}", pathConfig.string()));
    return true;
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

    auto& lootespConfig = ESP::GetConfig();
    ImGui::Checkbox("Enable Loot ESP", &lootespConfig.bESP);
    if (lootespConfig.bESP) {
        // Loot ESP options
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
        {"Instant Melee", "Melee", m_bInstantMelee},
        {"Auto Pistol", "Auto", m_bAutoPistol}
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

    static const char* aRapidFireOptions[]{ "Disabled", "Steady", "Rapid" };
    ImGui::Combo("Rapid Fire", &m_iRapidFire, aRapidFireOptions, IM_ARRAYSIZE(aRapidFireOptions));

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

        ImGui::Separator();
        if (ImGui::Button("Save Config"))
            CheatConfig::Get().Save();

        ImGui::SameLine();
        if (ImGui::Button("Load Config"))
            CheatConfig::Get().Load();
		
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
