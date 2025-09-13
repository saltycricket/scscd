#include "scscd.h"
#include <SimpleIni.h>
#include <windows.h>
#include <iostream>
#include <filesystem>

static std::time_t ini_mtime(std::filesystem::path &inipath) {
    auto ftime = fs::last_write_time(inipath);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now()
        + std::chrono::system_clock::now()
    );
    return std::chrono::system_clock::to_time_t(sctp);
}

static bool ini_newer_than(std::filesystem::path& inipath, std::time_t t) {
    return (std::filesystem::exists(inipath) && ini_mtime(inipath) > t);
}

bool ArmorIndex::SamplerConfig::haveSettingsChanged() {
    return ini_newer_than(inipath, iniModTime) ||
           ini_newer_than(defaultPath, iniModTime);
}

static float LoadFromIni(CSimpleIniA& ini, std::string fieldName, float defaultValue) {
    const char* v = ini.GetValue("Settings", fieldName.c_str(), nullptr);
    if (!v) {
        logger::warn(std::format("Field named '{}' was not found", fieldName));
        return defaultValue;
    }
    logger::info(std::format("Loaded float config field {} (value: {})", fieldName, static_cast<int>(std::atof(v))));
    return static_cast<float>(std::atof(v));
}

static int LoadFromIni(CSimpleIniA& ini, std::string fieldName, int defaultValue) {
    const char* v = ini.GetValue("Settings", fieldName.c_str(), nullptr);
    if (!v) {
        logger::warn(std::format("Field named '{}' was not found", fieldName));
        return defaultValue;
    }
    logger::info(std::format("Loaded int config field {} (value: {})", fieldName, static_cast<int>(std::atoi(v))));
    return static_cast<int>(std::atoi(v));
}

static bool LoadFromIni(CSimpleIniA &ini, std::string fieldName, bool defaultValue) {
    const char* v = ini.GetValue("Settings", fieldName.c_str(), nullptr);
    if (!v) {
        logger::warn(std::format("Field named '{}' was not found", fieldName));
        return defaultValue;
    }
    logger::info(std::format("Loaded boolean config field {} (value: {})", fieldName, std::string("0") == v ? "false" : "true"));
    return (std::string("1") == v);
}

static void WriteToIni(std::filesystem::path inipath, std::string fieldName, float v)
{
    CSimpleIniA ini;
    ini.SetUnicode(true);
    ini.LoadFile(inipath.c_str());
    ini.SetValue("Settings", fieldName.c_str(), std::to_string(v).c_str());
    ini.SaveFile(inipath.c_str());
    logger::info(std::format("Saved modified config for {} (changed to float {})", fieldName, std::to_string(v)));
}

static void WriteToIni(std::filesystem::path inipath, std::string fieldName, int v)
{
    CSimpleIniA ini;
    ini.SetUnicode(true);
    ini.LoadFile(inipath.c_str());
    ini.SetValue("Settings", fieldName.c_str(), std::to_string(v).c_str());
    ini.SaveFile(inipath.c_str());
    logger::info(std::format("Saved modified config for {} (changed to integer {})", fieldName, std::to_string(v)));
}

static void WriteToIni(std::filesystem::path inipath, std::string fieldName, bool v)
{
    CSimpleIniA ini;
    ini.SetUnicode(true);
    ini.LoadFile(inipath.c_str());
    ini.SetValue("Settings", fieldName.c_str(), v ? "true" : "false");
    ini.SaveFile(inipath.c_str());
    logger::info(std::format("Saved modified config for {} (changed to boolean {})", fieldName, std::to_string(v)));
}

void ArmorIndex::SamplerConfig::load(std::filesystem::path inipath, std::filesystem::path defaultPath) {
    this->inipath = inipath;
    this->defaultPath = defaultPath;
    if (!std::filesystem::exists(inipath)) {
        // if inipath doesn't exist, temporarily use defaultPath.
        // Later if we detect that inipath is created, it will switch
        // over automatically.
        inipath = defaultPath;
    }
    CSimpleIniA ini;
    ini.SetUnicode(true);
    ini.LoadFile(inipath.c_str());
    // allow ini file to override log level
    if (LoadFromIni(ini, "bDebugLog", false)) {
        spdlog::set_level(spdlog::level::trace);
        spdlog::flush_on(spdlog::level::trace);
    }
    else {
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::warn);
    }
    changeOutfitChanceM = LoadFromIni(ini, "iOutfitChangeChanceM", 75); // touch 3 out of every 4 NPCs
    changeOutfitChanceF = LoadFromIni(ini, "iOutfitChangeChanceF", 75); // touch 3 out of every 4 NPCs
    proximityBias = LoadFromIni(ini, "fProximityBias", 2.0f);
    allowNSFWChoices = LoadFromIni(ini, "bAllowNSFW", false);
    allowNudity = LoadFromIni(ini, "bAllowNudity", false);
    replaceArmor = LoadFromIni(ini, "bReplaceArmor", false);
    skipSlotChance[slot2bit(30)] = LoadFromIni(ini, "iSlotSkipChance30", 75); // Slot 30 - Hair Top[vanilla hair, rarely used by clothing mods]
    skipSlotChance[slot2bit(31)] = LoadFromIni(ini, "iSlotSkipChance31", 75); // Slot 31 - Hair Long[vanilla hair, rarely used by clothing mods]
    skipSlotChance[slot2bit(32)] = LoadFromIni(ini, "iSlotSkipChance32", 100); // Slot 32 - FaceGen Head[avoid:engine reserved]
    skipSlotChance[slot2bit(33)] = LoadFromIni(ini, "iSlotSkipChance33", 60); // Slot 33 - BODY[full outfits, vanilla default; avoid for layering]
    skipSlotChance[slot2bit(34)] = LoadFromIni(ini, "iSlotSkipChance34", 60); // Slot 34 - L Hand[gloves(left)]
    skipSlotChance[slot2bit(35)] = LoadFromIni(ini, "iSlotSkipChance35", 60); // Slot 35 - R Hand[gloves(right)]
    skipSlotChance[slot2bit(36)] = LoadFromIni(ini, "iSlotSkipChance36", 0); // Slot 36 -[U] Torso[underwear top / bra / undershirt]
    skipSlotChance[slot2bit(37)] = LoadFromIni(ini, "iSlotSkipChance37", 75); // Slot 37 -[U] L Arm[underwear sleeve(left)]
    skipSlotChance[slot2bit(38)] = LoadFromIni(ini, "iSlotSkipChance38", 75); // Slot 38 -[U] R Arm[underwear sleeve(right)]
    skipSlotChance[slot2bit(39)] = LoadFromIni(ini, "iSlotSkipChance39", 0); // Slot 39 -[U] L Leg[underwear bottom / panties]
    skipSlotChance[slot2bit(40)] = LoadFromIni(ini, "iSlotSkipChance40", 50); // Slot 40 -[U] R Leg[underwear leggings]
    skipSlotChance[slot2bit(41)] = LoadFromIni(ini, "iSlotSkipChance41", 50); // Slot 41 -[A] Torso[armor chestplate / top layer over torso]
    skipSlotChance[slot2bit(42)] = LoadFromIni(ini, "iSlotSkipChance42", 50); // Slot 42 -[A] L Arm[armor overlay, left arm]
    skipSlotChance[slot2bit(43)] = LoadFromIni(ini, "iSlotSkipChance43", 50); // Slot 43 -[A] R Arm[armor overlay, right arm]
    skipSlotChance[slot2bit(44)] = LoadFromIni(ini, "iSlotSkipChance44", 50); // Slot 44 -[A] L Leg[armor overlay, left leg]
    skipSlotChance[slot2bit(45)] = LoadFromIni(ini, "iSlotSkipChance45", 50); // Slot 45 -[A] R Leg[armor overlay, right leg]
    skipSlotChance[slot2bit(46)] = LoadFromIni(ini, "iSlotSkipChance46", 80); // Slot 46 - Headband[headbands, helmets, hats, circlets]
    skipSlotChance[slot2bit(47)] = LoadFromIni(ini, "iSlotSkipChance47", 55); // Slot 47 - Eyes[eyewear, goggles, glasses]
    skipSlotChance[slot2bit(48)] = LoadFromIni(ini, "iSlotSkipChance48", 80); // Slot 48 - Beard[masks, lower - face coverings]
    skipSlotChance[slot2bit(49)] = LoadFromIni(ini, "iSlotSkipChance49", 85); // Slot 49 - Mouth[scarves, bandanas, rebreathers]
    skipSlotChance[slot2bit(50)] = LoadFromIni(ini, "iSlotSkipChance50", 85); // Slot 50 - Neck[neckwear, scarves, chokers]
    skipSlotChance[slot2bit(51)] = LoadFromIni(ini, "iSlotSkipChance51", 75); // Slot 51 - Ring[rings(primary)]
    skipSlotChance[slot2bit(52)] = LoadFromIni(ini, "iSlotSkipChance52", 70); // Slot 52 - Scalp[secondary hair / helmet accessories, seldom used]
    skipSlotChance[slot2bit(53)] = LoadFromIni(ini, "iSlotSkipChance53", 100); // Slot 53 - Decapitation[avoid:engine reserved, kill FX]
    skipSlotChance[slot2bit(54)] = LoadFromIni(ini, "iSlotSkipChance54", 75); // Slot 54 - Unnamed[backpack, capes, cloaks, satchels]
    skipSlotChance[slot2bit(55)] = LoadFromIni(ini, "iSlotSkipChance55", 40); // Slot 55 - Unnamed[bandoliers, belts, ammo pouches]
    skipSlotChance[slot2bit(56)] = LoadFromIni(ini, "iSlotSkipChance56", 30); // Slot 56 - Unnamed[harnesses, tactical gear, skirts alt]
    skipSlotChance[slot2bit(57)] = LoadFromIni(ini, "iSlotSkipChance57", 30); // Slot 57 - Unnamed[skirts, kilts, robes, cloaks alt]
    skipSlotChance[slot2bit(58)] = LoadFromIni(ini, "iSlotSkipChance58", 60); // Slot 58 - Unnamed[body jewelry, 'on - back' gear, holsters]
    skipSlotChance[slot2bit(59)] = LoadFromIni(ini, "iSlotSkipChance59", 100); // Slot 59 - Shield[avoid:buggy / conflicts with grenades]
    skipSlotChance[slot2bit(60)] = LoadFromIni(ini, "iSlotSkipChance60", 100); // Slot 60 - Pip - Boy[reserved by engine, Pip - Boy only]
    skipSlotChance[slot2bit(61)] = LoadFromIni(ini, "iSlotSkipChance61",  90); // Slot 61 - FX[misc FX; sometimes safe for special equipables]
    iniModTime = ini_mtime(inipath); // track modification time so we can see if it's changed
    logger::info("Loaded config from INI");
}
/*
void ArmorIndex::SamplerConfig::setAllowNSFW(bool enabled) {
    allowNSFWChoices = enabled;
    WriteToIni(inipath, "bAllowNSFW", enabled);
}

void ArmorIndex::SamplerConfig::setAllowNudity(bool enabled) {
    allowNudity = enabled;
    WriteToIni(inipath, "bAllowNudity", enabled);
}

void ArmorIndex::SamplerConfig::setProximityBias(float bias) {
    proximityBias = bias;
    WriteToIni(inipath, "fProximityBias", bias);
}

void ArmorIndex::SamplerConfig::setChangeOutfitChance(uint8_t chance) {
    changeOutfitChance = chance;
    WriteToIni(inipath, "iOutfitChangeChance", chance);
}

void ArmorIndex::SamplerConfig::setSkipSlotChance(uint8_t slot, uint8_t chance) {
    skipSlotChance[slot2bit(slot)] = chance;
    WriteToIni(inipath, std::format("iSlotSkipChance{}", slot), chance);
}
*/
