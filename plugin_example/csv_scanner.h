#pragma once

#include "scscd.h"
//#include <RE/Bethesda/BSTList.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <charconv>
#include <optional>
#include <string_view>
#include <cstdint>
#include <filesystem>
#include <utility>

static std::vector<std::string> scandir(std::filesystem::path dir, std::string ext) {
    WIN32_FIND_DATA findFileData;
    std::string glob_pattern = std::string("*") + ext;
    const char * glob = (dir / glob_pattern).string().c_str();
    HANDLE hFind = FindFirstFile(glob, &findFileData);
    std::vector<std::string> matches;

    if (hFind == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open directory\n";
        return matches;
    }

    do {
        if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            matches.push_back(findFileData.cFileName);
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);
    return matches;
}

static std::string trim(std::string_view sv) {
    auto is_not_space = [](unsigned char c) { return !std::isspace(c); };

    // left trim
    auto begin = std::find_if(sv.begin(), sv.end(), is_not_space);
    // right trim
    auto end = std::find_if(sv.rbegin(), sv.rend(), is_not_space).base();

    return (begin < end ? std::string(begin, end) : std::string{});
}

static std::vector<std::string> split_and_trim(const std::string& input, char delimiter = ',') {
    std::vector<std::string> result;
    std::string_view sv{ input };

    size_t start = 0;
    while (start < sv.size()) {
        auto end = sv.find(delimiter, start);
        if (end == std::string_view::npos) {
            end = sv.size();
        }
        std::string_view token = sv.substr(start, end - start);
        result.push_back(trim(token));
        start = end + 1;
    }

    return result;
}

static std::optional<std::uint32_t> hex_to_u32(std::string_view s) {
    // Allow optional "0x" / "0X"
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s.remove_prefix(2);

    if (s.empty()) return std::nullopt;

    std::uint32_t value{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value, 16);

    if (ec == std::errc{} && ptr == s.data() + s.size())
        return value;  // success

    return std::nullopt; // invalid char or overflow
}

static bool ends_with_icase(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) {
        return false;
    }
    auto offset = s.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(s[offset + i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }
    return true;
}

static void remove_suffix_icase(std::string& s, const std::string& suffix) {
    if (ends_with_icase(s, suffix)) {
        s.resize(s.size() - suffix.size());
    }
}

static bool IsLight(const RE::TESFile *file) {
    return file->compileIndex == 0xFE;
}

static uint32_t MakeRuntimeFormID(const RE::TESFile* file, uint32_t csvIdLocalOrRuntimeGuess)
{
    // Clear any pasted load-order prefix
    uint32_t localId = csvIdLocalOrRuntimeGuess & 0x00FFFFFFu;

    if (file && IsLight(file)) {
        // ESL / ESL-flagged: FE | smallIndex | 12-bit local
        const std::uint16_t smallID = file->GetSmallFileCompileIndex(); // 0..0x0FFF
        localId &= 0x00000FFFu;
        return 0xFE000000u | (static_cast<uint32_t>(smallID) << 12) | localId;
    }
    else {
        // Full ESM/ESP: (compileIndex << 24) | low 24 bits
        const std::uint8_t idx = file ? file->GetCompileIndex() : 0xFF; // 0xFF = invalid
        return (static_cast<uint32_t>(idx) << 24) | (localId & 0x00FFFFFFu);
    }
}

static bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        unsigned char ca = static_cast<unsigned char>(a[i]);
        unsigned char cb = static_cast<unsigned char>(b[i]);
        if ((ca | 32) != (cb | 32)) return false;
    }
    return true;
}

static RE::TESFile* FindTESFileByName(std::string_view plugin) {
    logger::trace(std::format("> FindTESFileByName {}", plugin));
    auto* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) return nullptr;

    const auto iequals = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            unsigned char ca = static_cast<unsigned char>(a[i]);
            unsigned char cb = static_cast<unsigned char>(b[i]);
            if ((ca | 32) != (cb | 32)) return false;
        }
        return true;
        };

    const auto try_match = [&](RE::TESFile* f) -> RE::TESFile* {
        if (!f) return nullptr;
        std::string_view name = f->GetFilename();
        name = std::string_view(name.data(), std::char_traits<char>::length(name.data()));
        return iequals(name, plugin) ? f : nullptr;
        };

    // Full (non-light) files
    for (RE::TESFile* f : dh->compiledFileCollection.files) {
        if (auto* hit = try_match(f)) {
            logger::trace("< FindTESFileByName found");
            return hit;
        }
    }
    // Light (ESL / ESL-flagged)
    if constexpr (requires { dh->compiledFileCollection.smallFiles; }) {
        for (RE::TESFile* f : dh->compiledFileCollection.smallFiles) {
            if (auto* hit = try_match(f)) {
                logger::trace("< FindTESFileByName found (light)");
                return hit;
            }
        }
    }

    logger::trace("< FindTESFileByName not found");
    return nullptr;
}

template <class T = RE::TESForm>
static T* LookupFormInFile(std::string_view plugin, uint32_t csvIdLocalOrRuntimeGuess) {
    // special case
    logger::trace(std::format("> LookupFormInFile {}, {:#010x}", plugin, csvIdLocalOrRuntimeGuess));
    if (plugin == "Fallout4.esm") {
        const auto runtimeId = csvIdLocalOrRuntimeGuess & 0x00FFFFFF;
        logger::trace(std::format("< LookupFormInFile => base game, {:#010x}", runtimeId));
        return RE::TESForm::GetFormByID(runtimeId);
    }
    if (auto* file = FindTESFileByName(plugin)) {
        const auto runtimeId = MakeRuntimeFormID(file, csvIdLocalOrRuntimeGuess);
        logger::trace(std::format("< LookupFormInFile => {:#010x}", runtimeId));
        if (auto* base = RE::TESForm::GetFormByID(runtimeId)) {
            // kNONE from TESForm base class means any form is valid
            // and the following cast should work too
            if (T::FORM_ID != RE::ENUM_FORM_ID::kNONE && base->GetFormType() != T::FORM_ID) {
                logger::error(std::format("Expected type {:#06x}, got type {:#06x}", (uint32_t) T::FORM_ID, (uint32_t) base->GetFormType()));
                return nullptr;
            }
            return base->As<T>();  // safe RTTI cast
        }
        else {
            logger::error(std::format("form not found in expected plugin {}", plugin));
            return nullptr;
        }
    }
    logger::warn(std::format("Plugin {} does not appear to have been loaded (this might just mean mod is not installed/enabled)", plugin));
    return nullptr;
}

#define CSV_LINENO std::string(" at ") + filename + std::string(":") + std::to_string(lineno)

static void scan_occupations_csv(std::filesystem::path basedir, OccupationIndex& index) {
    logger::info("Loading occupations from " + basedir.string());
    std::vector<std::string> filenames = scandir(basedir, ".csv");
    for (std::string filename : filenames) {
        std::filesystem::path fullpath = basedir / filename;
        logger::debug("Parsing occupations file " + fullpath.string());
        std::ifstream file(fullpath);
        if (!file) {
            logger::warn("Could not open file " + fullpath.string());
            continue;
        }
        int lineno = 0;
        std::string line;
        // remove .csv from filename, we will need this later during form lookup
        std::string plugin_file = filename;
        remove_suffix_icase(plugin_file, ".csv");
        while (std::getline(file, line)) {
            lineno += 1;
            // strip comments
            if (size_t n = line.find('#'); n != std::string::npos)
                line = line.substr(0, n);
            std::vector<std::string> columns = split_and_trim(line, ',');
            // skip blank lines (after comments)
            if (columns.size() == 0 || (columns.size() == 1 && columns[0] == ""))
            {
                // blank line
                continue;
            }
            if (columns.size() < 2) {
                // Occupation,FormID[,EditorID]
                logger::warn(std::format("skipped: expected 2+ fields, got {}", columns.size()) + CSV_LINENO);
                continue;
            }
            // if we got here, we think the line is pareseable
            std::string occupationString = columns[0];
            std::string formIDString = columns[1];
            if (iequals(occupationString, "occupation")) // header
                continue;
            Occupation occupation = STR2OCCUPATION(occupationString);
            if (occupation == NO_OCCUPATION) {
                logger::warn(std::string("skipped: could not parse occupation ")
                    + occupationString
                    + CSV_LINENO);
                continue;
            }
            logger::trace(std::format("parse occupation: {} => {:#10x}", occupationString, occupation));
            uint32_t formid = 0;
            if (auto v = hex_to_u32(formIDString))
                formid = *v;
            else {
                logger::warn(std::string("skipped: could not parse form ID ")
                    + formIDString
                    + CSV_LINENO);
                continue;
            }
            logger::trace(std::format("parse formid: {} => {:#10x}", formIDString, formid));
            // At this point we've parsed a form ID and an occupation.
            // Try to find the formID within the plugin file.
            logger::trace(std::format("lookup formid: {}, {:#10x}", plugin_file, formid));
            RE::TESForm* form = LookupFormInFile<RE::TESForm>(std::string_view(plugin_file), formid);
            if (form == NULL) {
                logger::error(std::format("skipped: form ID {:#x} could not be found in plugin {}", formid, plugin_file) + CSV_LINENO);
                continue;
            }

            logger::trace("(found...");
            logger::trace(std::format(" ...{:#10x}", form->GetFormID()));
            logger::trace(std::format(" ...with type: {:#06x})", (unsigned int) form->GetFormType()));
            bool rc = true;
            switch (form->GetFormType()) {
                case RE::ENUM_FORM_ID::kCLAS:
                case RE::ENUM_FORM_ID::kFACT:
                case RE::ENUM_FORM_ID::kNPC_:
                    break;
                default:
                    logger::error(std::format("looked-up form has type {:#06x} "
                                              "(it must be one of CLAS, FACT or NPC_)",
                                              (unsigned int) form->GetFormType()) + CSV_LINENO);
                    continue;
            }

            logger::debug(filename + std::format(": registering {:#06x} form {:#10x} as a {} ({:#10x})", (uint32_t) form->GetFormType(), formid, occupationString, occupation));
            rc = index.put(form->GetFormID(), occupation);
            if (!rc) {
                logger::error("! occupation registration of most recent form failed!");
                continue;
            }
        }
    }
}

static void scan_tuples_csv(std::filesystem::path basedir, bool nsfw, ArmorIndex& index) {
    logger::info("Loading occupations from " + basedir.string());
    std::vector<std::string> filenames = scandir(basedir, ".csv");
    for (std::string filename : filenames) {
        std::filesystem::path fullpath = basedir / filename;
        logger::debug("Parsing occupations file " + fullpath.string());
        std::ifstream file(fullpath);
        if (!file) {
            logger::warn("Could not open file " + fullpath.string());
            continue;
        }
        int lineno = 0;
        std::string line;
        // remove .csv from filename, we will need this later during form lookup
        std::string plugin_file = filename;
        remove_suffix_icase(plugin_file, ".csv");
        while (std::getline(file, line)) {
            lineno += 1;
            // strip comments
            if (size_t n = line.find('#'); n != std::string::npos)
                line = line.substr(0, n);
            std::vector<std::string> columns = split_and_trim(line, ',');
            if (columns.size() == 0 || (columns.size() == 1 && columns[0] == ""))
            {
                // blank line
                continue;
            }
            if (columns.size() != 3 && columns.size() != 4) {
                // Sexes,Occupation,FormID[;FormID][,Level]
                logger::error(std::format("skipped: expected 3 or 4 fields, got {}", columns.size()) + CSV_LINENO);
                continue;
            }

            uint32_t sexes = 0;
            std::string sexBinstr = trim(columns[0]);
            if (iequals(sexBinstr, "sex") || iequals(sexBinstr, "occupation"))
                continue; // csv header
            if (sexBinstr.size() != SEX_WIDTH) {
                logger::error(std::format("skipped: sex binstr {} must contain exactly {} characters",
                    sexBinstr, SEX_WIDTH) + CSV_LINENO);
                continue;
            }
            sexes = BINSTR2SEX(sexBinstr);

            // if we got here, we think the line is pareseable
            std::string occupationBinstr = trim(columns[1]);
            if (occupationBinstr.size() != OCCUPATION_WIDTH) {
                logger::error(std::format("skipped: occupation binstr {} must contain exactly {} characters",
                                          occupationBinstr, OCCUPATION_WIDTH) + CSV_LINENO);
                continue;
            }
            uint32_t occupations = BINSTR2OCCUPATION(occupationBinstr);
            if (occupations == NO_OCCUPATIONS) {
                logger::error(std::format("skipped: occupation binstr {} could "
                                          "not be decoded (no occupation bits set)",
                                          occupationBinstr) + CSV_LINENO);
                continue;
            }

            std::string formIDsString = columns[2];
            std::vector<std::string> formIDs = split_and_trim(formIDsString, ';');
            if (formIDs.size() == 0) {
                logger::error(std::string("skipped: no form IDs in column 2") + CSV_LINENO);
                continue;
            }

            // optional level
            std::uint8_t level = 0;
            if (columns.size() >= 4) level = (uint8_t) atoi(columns[3].c_str());

            std::vector<RE::TESObjectARMO *> armors;
            for (std::string formIDString : formIDs) {
                uint32_t formid = 0;
                if (auto v = hex_to_u32(formIDString))
                    formid = *v;
                else {
                    logger::warn(std::format("skipped: could not parse form ID {}", formIDString) + CSV_LINENO);
                    // if one armor is bad the whole tuple is questionable - skip it
                    armors.clear();
                    break;
                }
                // At this point we've parsed a form ID and an occupation.
                // Try to find the formID within the plugin file.
                RE::TESForm* form = LookupFormInFile<RE::TESForm>(std::string_view(plugin_file), formid);
                if (form == NULL) {
                    logger::error(std::format("skipped: form ID {:#10x} could not be"
                                              "found in plugin {}",
                                              formid,
                                              plugin_file) + CSV_LINENO);
                    // if one armor is bad the whole tuple is questionable - skip it
                    armors.clear();
                    break;
                }

                if (form->GetFormType() != RE::ENUM_FORM_ID::kARMO) {
                    logger::error(std::format("skipped: looked-up form has type {:#10x} (it must be ARMO)",
                                              (unsigned int)form->GetFormType()) + CSV_LINENO);
                    // if one armor is bad the whole tuple is questionable - skip it
                    armors.clear();
                    break;
                }

                RE::TESObjectARMO* armor = form->As<RE::TESObjectARMO>();
                armors.push_back(armor);
            }

            if (armors.size() == 0) {
                logger::error(std::string("skipped: no armors specified or at least one of them failed validation") + CSV_LINENO);
                continue;
            }

            logger::debug(filename + std::string(": registering a set of ")
                + std::to_string(armors.size())
                + std::format(" armors for level {}+ characters as ", level)
                + (nsfw ? std::string("NSFW") : std::string("SFW"))
                + std::format(" for occupation map {:#10x}", occupations)
                + CSV_LINENO);
            index.registerTuple(level, nsfw, sexes, occupations, armors);
        }
    }
}
