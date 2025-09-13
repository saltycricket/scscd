#include "scscd.h"

void scan_matswaps_csv(std::filesystem::path basedir, bool nsfw, ArmorIndex& index) {
    logger::info("Loading matswaps from " + basedir.string());
    std::vector<std::string> filenames = scandir(basedir, ".csv");
    for (std::string filename : filenames) {
        std::filesystem::path fullpath = basedir / filename;
        logger::debug("Parsing matswaps file " + fullpath.string());
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
            if (columns.size() == 0 || (columns.size() == 1 && columns[0] == ""))
            {
                // blank line
                continue;
            }
            // TODO make position-independent by reading headers
            if (columns.size() != 2) {
                // ArmorFormID[;ArmorFormID],MatSwapFormID[;MatSwapFormID]
                logger::error(std::format("skipped: expected 2 fields, got {}", columns.size()) + CSV_LINENO);
                continue;
            }

            std::vector<std::string> armorFormIDs = split_and_trim(columns[0], ';');
            if (armorFormIDs.size() == 0) {
                logger::error(std::string("skipped: no form IDs in column 1") + CSV_LINENO);
                continue;
            }

            std::vector<std::string> matswapFormIDs = split_and_trim(columns[1], ';');
            if (matswapFormIDs.size() == 0) {
                logger::error(std::string("skipped: no form IDs in column 2") + CSV_LINENO);
                continue;
            }

            std::vector<RE::TESObjectARMO*> armors = parseFormIDs<RE::TESObjectARMO>(plugin_file, armorFormIDs);
            if (armors.size() == 0) {
                logger::error(std::string("skipped: no armors specified or at least one of them failed validation") + CSV_LINENO);
                continue;
            }

            std::vector<RE::BGSMaterialSwap*> matswaps = parseFormIDs<RE::BGSMaterialSwap>(plugin_file, matswapFormIDs);
            if (matswaps.size() == 0) {
                logger::error(std::string("skipped: no matswaps specified or at least one of them failed validation") + CSV_LINENO);
                continue;
            }

            logger::debug(std::format("{}: registering a set of {} {} matswaps to a set of {} armors", filename, matswaps.size(), nsfw ? "NSFW" : "SFW", armors.size()) + CSV_LINENO);
            index.registerMatswaps(armors, matswaps, nsfw);
            count += 1;
        }
        logger::info(std::format("Registered {} matswaps from file {}", count, filename));
    }
}
