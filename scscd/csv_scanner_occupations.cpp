#include "scscd.h"

void scan_occupations_csv(std::filesystem::path basedir, OccupationIndex& index) {
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
        std::string plugin_file = file_basename(filename);
        remove_suffix_icase(plugin_file, ".csv");
        // check if relevant plugin is loaded or not
        if (!FindTESFileByName(plugin_file)) {
            logger::debug(std::format("plugin {} not loaded; skipping", plugin_file));
            continue;
        }
        uint32_t count = 0;
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
            logger::trace(std::format(" ...with type: {:#06x})", (unsigned int)form->GetFormType()));
            bool rc = true;
            switch (form->GetFormType()) {
            case RE::ENUM_FORM_ID::kCLAS:
            case RE::ENUM_FORM_ID::kFACT:
            case RE::ENUM_FORM_ID::kNPC_:
                break;
            default:
                logger::error(std::format("looked-up form has type {:#06x} "
                    "(it must be one of CLAS, FACT or NPC_)",
                    (unsigned int)form->GetFormType()) + CSV_LINENO);
                continue;
            }

            logger::debug(filename + std::format(": registering {:#06x} form {:#10x} as a {} ({:#10x})", (uint32_t)form->GetFormType(), formid, occupationString, occupation));
            rc = index.put(form->GetFormID(), occupation);
            if (!rc) {
                logger::error("! occupation registration of most recent form failed!");
                continue;
            }
            count += 1;
        }
        logger::info(std::format("Registered {} occupations from file {}", count, filename));
    }
}
