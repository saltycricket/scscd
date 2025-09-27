#include "scscd.h"

void scan_exclusions_csv(std::filesystem::path basedir, std::unordered_set<uint32_t> &exclusionList) {
    logger::info("Loading exclusions from " + basedir.string());
    std::vector<std::string> filenames = scandir(basedir, ".csv");
    for (std::string filename : filenames) {
        uint32_t count = 0;
        std::filesystem::path fullpath = basedir / filename;
        logger::debug("Parsing exclusions file " + fullpath.string());
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
            // TODO make position-independent by reading headers
            if (columns.size() != 1) {
                logger::error(std::format("skipped: expected 1 column, got {}", columns.size()) + CSV_LINENO);
                continue;
            }

            std::string idString = trim(columns[0]);
            RE::TESForm* form = FindFormByFormIDOrEditorID(plugin_file, idString, RE::TESClass::FORM_ID, /*logOnMissing*/ false);
            if (!form) form = FindFormByFormIDOrEditorID(plugin_file, idString, RE::TESFaction::FORM_ID, /*logOnMissing*/ false);
            if (!form) form = FindFormByFormIDOrEditorID(plugin_file, idString, RE::TESNPC::FORM_ID, /*logOnMissing*/ false);
            if (!form) {
                logger::warn(std::format("Form with ID {} for exclusion list was NOT FOUND", idString) + CSV_LINENO);
                continue;
            }

            exclusionList.insert(form->GetFormID());
            logger::debug(std::format("Added form {:#010x} to the exclusion list", form->GetFormID()));
            count += 1;
        }
        logger::info(std::format("Added {} exclusions from file {}", count, filename));
    }
}
