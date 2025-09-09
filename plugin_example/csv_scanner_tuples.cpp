#include "scscd.h"

void scan_tuples_csv(std::filesystem::path basedir, bool nsfw, ArmorIndex& index) {
    logger::info("Loading tuples from " + basedir.string());
    std::vector<std::string> filenames = scandir(basedir, ".csv");
    for (std::string filename : filenames) {
        std::filesystem::path fullpath = basedir / filename;
        logger::debug("Parsing tuples file " + fullpath.string());
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
            return;
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
            if (columns.size() >= 4) level = (uint8_t)atoi(columns[3].c_str());

            std::vector<RE::TESObjectARMO*> armors = parseFormIDs<RE::TESObjectARMO>(plugin_file, formIDs);
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
