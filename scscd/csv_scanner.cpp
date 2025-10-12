#include "scscd.h"
#include "csv_scanner.h"
#include "armor_index.h"
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

/// Parse a single CSV record (one line) into fields.
/// Supports RFC 4180 quoting: fields may be quoted with ", and "" inside quotes means a literal ".
std::vector<std::string> csv_parse_line(const std::string& input)
{
    std::string_view line(input);
    const char delimiter = ',';
    const bool strict = false;

    // Strip UTF-8 BOM if present (just in case the first line was passed in)
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.remove_prefix(3);
    }

    // Trim a single trailing '\r' (common with CRLF files when reading lines)
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }

    std::vector<std::string> out;
    std::string field;
    field.reserve(line.size()); // rough upper bound; avoids some reallocations

    enum class State { Unquoted, Quoted, QuotePending };
    State state = State::Unquoted;

    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];

        switch (state) {
        case State::Unquoted:
            if (ch == '"') {
                state = State::Quoted;
            }
            else if (ch == delimiter) {
                out.emplace_back(trim(std::move(field)));
                field.clear();
            }
            else {
                field.push_back(ch);
            }
            break;

        case State::Quoted:
            if (ch == '"') {
                state = State::QuotePending; // could be end of quoted field or an escaped quote
            }
            else {
                field.push_back(ch);
            }
            break;

        case State::QuotePending:
            if (ch == '"') {
                // Escaped quote ("")
                field.push_back('"');
                state = State::Quoted;
            }
            else if (ch == delimiter) {
                // End of quoted field
                out.emplace_back(trim(std::move(field)));
                field.clear();
                state = State::Unquoted;
            }
            else {
                // Quote followed by non-delimiter, non-quote: this is technically invalid CSV.
                // We'll treat the quote as closing and the current char as part of the next (unquoted) tail.
                // Many real-world CSVs do this; if strict, reject it.
                if (strict) {
                    throw std::runtime_error("Malformed CSV: unexpected character after closing quote");
                }
                field.push_back(ch);
                state = State::Unquoted;
            }
            break;
        }
    }

    // End of line handling
    if (state == State::Quoted) {
        // Unterminated quoted field
        if (strict) {
            throw std::runtime_error("Malformed CSV: unterminated quoted field");
        }
        // lenient mode: accept as-is
    }
    else if (state == State::QuotePending) {
        // Properly closed quoted field at end of line
        // (do nothing; just let it finalize below)
    }

    out.emplace_back(trim(std::move(field)));
    return out;
}


RE::TESForm* FindFormByFormIDOrEditorID(std::string& plugin_file, std::string& idString, RE::ENUM_FORM_ID expectedFormType, bool logOnMissing) {
    RE::TESForm* form = NULL;
    if (isFormIDString(idString)) {
        uint32_t formid = 0;
        if (auto v = hex_to_u32(idString))
            formid = *v;
        else {
            logger::warn(std::string("skipped: could not parse form ID ")
                + idString);
            return NULL;
        }
        logger::trace(std::format("parse formid: {} => {:#10x}", idString, formid));
        // At this point we've parsed a form ID and an occupation.
        // Try to find the formID within the plugin file.
        logger::trace(std::format("lookup formid: {}, {:#10x}", plugin_file, formid));
        form = LookupFormInFile<RE::TESForm>(std::string_view(plugin_file), formid);
        if (form == NULL) {
            if (logOnMissing)
                logger::error(std::format("skipped: form ID {:#x} could not be found in plugin {}", formid, plugin_file));
            return NULL;
        }
    }
    else {
        uint32_t formID = ArmorIndex::getFormByTypeAndEdid(expectedFormType, idString, logOnMissing);
        if (formID == 0) {
            if (logOnMissing)
                logger::error(std::format("skipped: form editor ID {} could not be resolved to a form ID!", idString));
            return NULL;
        }
        form = RE::TESForm::GetFormByID(formID);
        if (form == NULL) {
            if (logOnMissing)
                logger::error(std::format("skipped: form editor ID {} was resolved to form ID {:#010x} but the form itself could not be found!", idString, formID));
            return NULL;
        }
    }
    return form;
}
