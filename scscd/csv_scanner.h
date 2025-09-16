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
    std::vector<std::string> matches;
    try {
        for (auto const& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".csv") {
                matches.push_back(entry.path().string());
            }
        }
    }
    catch (std::filesystem::filesystem_error const& e) {
        logger::warn(std::format("filesystem error: {}", e.what()));
    }
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

static std::string file_basename(std::string& s) {
    return std::filesystem::path(s).filename().string();
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

static bool istartswith(std::string_view a, std::string_view b) {
    if (a.size() < b.size()) return false;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
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
    // Problem: many/most clothing mods are distributed as full ESPs. But people who run a lot of them
    // (me included) flag them as ESLs. When that happens the form IDs are compacted and effectively
    // renamed. So, the form IDs in presented to us here could be either the original form IDs
    // (fine) or the newly compacted form IDs. Luckily, the compaction process seems deterministic,
    // so two users with separate compaction processes SHOULD produce the same compacted form IDs.
    // In practice I think this means we can get away with placing both original and compacted
    // form IDs into the same CSV file. But I don't want to log a bunch of warnings for missing
    // forms when this is now intended behavior. So, we can interrogate the plugin file itself
    // (if found). If the plugin is light, then we'll skip non-light forms and only warn on
    // missing light forms. If the plugin isn't light, then we'll skip light forms and only warn
    // on missing full forms. In both cases, if we omit an opposite, we'll log it as debug,
    // that way verbose logging can still catch them in a pinch.
    //
    // Now there is one caveat. How do we know whether we were given a light or full form ID?
    // Well, we don't know the plugin order so we skip and re-compute the first two nibbles
    // in all cases. But the author of the CSV does know whether they are filling in a light
    // or full form. So they can enter the first two nibbles as FE, same as the engine does.
    // Thus, any form in the CSV which begins 0xFExxxxxx is light.

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
            // at this point the form isn't found; we only need to decide at what level to
            // log it.
            if (IsLight(file) && (csvIdLocalOrRuntimeGuess & 0xFE) == 0xFE) {
                // light plugin & expected light form is missing
                logger::error(std::format("form {:#010x} not found in expected plugin {}", runtimeId, plugin));
            }
            else if (!IsLight(file) && (csvIdLocalOrRuntimeGuess & 0xFE) != 0xFE) {
                // standard plugin & expected full form is missing
                logger::error(std::format("form {:#010x} not found in expected plugin {}", runtimeId, plugin));
            }
            else {
                // form type doesn't match plugin type.
                logger::debug(std::format("form {:#010x} not found in expected plugin {}", runtimeId, plugin));
            }
            return nullptr;
        }
    }
    logger::warn(std::format("Plugin {} does not appear to have been loaded (this might just mean mod is not installed/enabled)", plugin));
    return nullptr;
}

#define CSV_LINENO std::string(" at ") + filename + std::string(":") + std::to_string(lineno)

static bool isFormIDString(const std::string& s)
{
    if (s.size() != 8) {
        return false;
    }
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isxdigit(c);
        });
}

RE::TESForm* FindFormByFormIDOrEditorID(std::string& plugin_file, std::string& idString, RE::ENUM_FORM_ID expectedFormType);

template<class T>
static std::vector<T*> parseFormIDs(std::string &plugin_file, std::vector<std::string>& formIDs) {
    std::vector<T*> forms;
    for (std::string idString : formIDs) {
        // As of 1.1.0, idString could be a form ID or an editor ID.
        RE::TESForm* form = FindFormByFormIDOrEditorID(plugin_file, idString, T::FORM_ID);
        if (form == NULL) {
            // if one form is bad the whole tuple is questionable - skip it
            forms.clear();
            break;
        }

        if (form->GetFormType() != T::FORM_ID) {
            logger::error(std::format("looked-up form has type {:#06x} (it must be {:#06x})",
                (uint16_t)form->GetFormType(), (uint16_t) T::FORM_ID));
            // if one form is bad the whole set is questionable - skip it
            forms.clear();
            break;
        }

        forms.push_back(form->As<T>());
    }
    return forms;
}

void scan_occupations_csv(std::filesystem::path basedir, OccupationIndex& index);
void scan_tuples_csv(std::filesystem::path basedir, bool nsfw, ArmorIndex& index);
void scan_exclusions_csv(std::filesystem::path basedir, std::unordered_set<uint32_t>& exclusionList);
