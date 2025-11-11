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

static float LoadFromIni(CSimpleIniA& ini, std::string fieldName, float defaultValue, bool noisy) {
    const char* v = ini.GetValue("Settings", fieldName.c_str(), nullptr);
    if (!v) {
        if (noisy) logger::warn(std::format("Field named '{}' was not found", fieldName));
        else      logger::debug(std::format("Field named '{}' was not found", fieldName));
        return defaultValue;
    }
    logger::debug(std::format("Loaded float config field {} (value: {})", fieldName, static_cast<int>(std::atof(v))));
    return static_cast<float>(std::atof(v));
}

static int LoadFromIni(CSimpleIniA& ini, std::string fieldName, int defaultValue, bool noisy) {
    const char* v = ini.GetValue("Settings", fieldName.c_str(), nullptr);
    if (!v) {
        if (noisy) logger::warn(std::format("Field named '{}' was not found", fieldName));
        else      logger::debug(std::format("Field named '{}' was not found", fieldName));
        return defaultValue;
    }
    logger::debug(std::format("Loaded int config field {} (value: {})", fieldName, static_cast<int>(std::atoi(v))));
    return static_cast<int>(std::atoi(v));
}

static bool LoadFromIni(CSimpleIniA &ini, std::string fieldName, bool defaultValue, bool noisy) {
    const char* v = ini.GetValue("Settings", fieldName.c_str(), nullptr);
    if (!v) {
        if (noisy) logger::warn(std::format("Field named '{}' was not found", fieldName));
        else      logger::debug(std::format("Field named '{}' was not found", fieldName));
        return defaultValue;
    }
    logger::debug(std::format("Loaded boolean config field {} (value: {})", fieldName, std::string("0") == v ? "false" : "true"));
    return (std::string("1") == v);
}

bool ArmorIndex::SamplerConfig::load(std::filesystem::path &path, bool noisy) {
    logger::info(std::format("Loading config file {}", path.string()));

    if (!std::filesystem::exists(path)) {
        return false;
    }

    CSimpleIniA ini;
    ini.SetUnicode(true);
    ini.LoadFile(path.c_str());
    // allow ini file to override log level
    if (LoadFromIni(ini, "bDebugLog", false, noisy)) {
        spdlog::set_level(spdlog::level::trace);
        spdlog::flush_on(spdlog::level::trace);
    }
    else {
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::warn);
    }

    // note: if noisy == true, we are loading the core/default config. This means
    // if we should fail to find a field, we use a hard coded default (e.g. 75) as
    // there is no other value to fall back on.
    // But if noisy == false, and a field is missing, then that is a field omitted
    // from an MCM config file, which is expected (MCM only saves changed fields).
    // In that case, the default should be the current value (that is, no change
    // from the core config).
    // Hence the ternary pattern: noisy ? [default] : [current]
    changeOutfitChanceM = LoadFromIni(ini, "iOutfitChangeChanceM", noisy ? 75    : changeOutfitChanceM, noisy);
    changeOutfitChanceF = LoadFromIni(ini, "iOutfitChangeChanceF", noisy ? 75    : changeOutfitChanceF, noisy);
    proximityBias       = LoadFromIni(ini, "fProximityBias",       noisy ? 2.0f  : proximityBias,       noisy);
    allowNSFWChoices    = LoadFromIni(ini, "bAllowNSFW",           noisy ? false : allowNSFWChoices,    noisy);
    allowNudity         = LoadFromIni(ini, "bAllowNudity",         noisy ? false : allowNudity,         noisy);
    replaceArmor        = LoadFromIni(ini, "bReplaceArmor",        noisy ? false : replaceArmor,        noisy);
    for (uint32_t slot = 30; slot < 62; slot++) {
        // by default, all slots have zero chance to be filled. This way, no configuration == no mod behavior.
        fillSlotChanceM[slot2bit(slot)] = LoadFromIni(ini, std::format("iMaleFillSlotChance{}",   slot), noisy ? 0 : fillSlotChanceM[slot2bit(slot)], noisy);
        fillSlotChanceF[slot2bit(slot)] = LoadFromIni(ini, std::format("iFemaleFillSlotChance{}", slot), noisy ? 0 : fillSlotChanceF[slot2bit(slot)], noisy);
    }
    iniModTime = ini_mtime(path); // track modification time so we can see if it's changed
    logger::info(std::format("Loaded config from {}", path.string()));
    return true;
}

bool ArmorIndex::SamplerConfig::load(std::filesystem::path inipath, std::filesystem::path defaultPath) {
    this->inipath = inipath;
    this->defaultPath = defaultPath;

    if (!load(defaultPath, /*noisy*/true)) {
        logger::error(std::format("File at {} does not exist; could not reset to defaults", defaultPath.string()));
        return false;
    }

    if (!load(inipath, /*noisy*/false)) {
        logger::warn(std::format("File at {} does not exist; could not load user config", inipath.string()));
    }

    logger::info("Loaded SCSCD config successfully.");
    return true;
}
