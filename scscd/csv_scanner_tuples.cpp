#include "scscd.h"
#include "csv_scanner.h"

void scan_tuples_csv(std::filesystem::path basedir, bool nsfw, ArmorIndex& index, std::unordered_map<std::string, Taxon> &taxonomy) {
    logger::info("Loading tuples from " + basedir.string());
    std::vector<std::string> filenames = scandir(basedir, ".csv");
    for (std::string filename : filenames) {
        uint32_t count = 0;
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
            continue;
        }
        while (std::getline(file, line)) {
            lineno += 1;
            // strip comments
            if (size_t n = line.find('#'); n != std::string::npos)
                line = line.substr(0, n);
            std::vector<std::string> columns = csv_parse_line(line);
            if (columns.size() == 0 || (columns.size() == 1 && columns[0] == ""))
            {
                // blank line
                continue;
            }
            // TODO make position-independent by reading headers
            if (columns.size() < 3 || columns.size() > 7) {
                // Sexes,Occupation,FormOrEditorIDs[,Level][,OModIDs][,ClothingTypeID]
                logger::error(std::format("skipped: expected 3 to 7 fields, got {}", columns.size()) + CSV_LINENO);
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

            // optional omods list (Form or editor IDs)
            std::vector<RE::BGSMod::Attachment::Mod*> omods;
            if (columns.size() >= 5) {
                std::vector<std::string> omodIDs = split_and_trim(columns[4], ';');
                if (omodIDs.size() > 0) {
                    omods = parseFormIDs<RE::BGSMod::Attachment::Mod>(plugin_file, omodIDs);
                }
            }

            // optional per-item nsfw flag
            bool localNSFW = nsfw;
            if (columns.size() >= 6 && columns[5].size() > 0) {
                if (columns[5] == "1" || columns[5] == "true")
                    localNSFW = true;
                else
                    localNSFW = false;
            }

            // optional item category name for biped slot override
            if (columns.size() >= 7 && columns[6].size() > 0) {
                if (taxonomy.contains(columns[6])) {
                    if (armors.size() == 1) {
                        const Taxon& taxon = taxonomy[columns[6]];
                        RE::TESObjectARMO* armor = armors[0];
                        uint32_t armo_former = static_cast<RE::BGSBipedObjectForm*>(armor)->bipedModelData.bipedObjectSlots;
                        static_cast<RE::BGSBipedObjectForm*>(armor)->bipedModelData.bipedObjectSlots = taxon.armo_slots;
                        logger::debug(std::format("changed ARMO bipe flags for armor {:#010x} from {:#010x} to {:#010x}", armor->GetFormID(), armo_former, taxon.armo_slots) + CSV_LINENO);
                        // modelArray contains armor attachments. We expect attachment 0 to always be the base object which
                        // is what we want to modify here. If there are any other attachments, they may be matswaps/etc, and
                        // we shouldn't have to manipulate those.
                        if (armor->modelArray.size() > 0) {
                            uint32_t arma_former = armor->modelArray[0].armorAddon->bipedModelData.bipedObjectSlots;
                            armor->modelArray[0].armorAddon->bipedModelData.bipedObjectSlots = taxon.arma_slots;
                            logger::debug(std::format("changed ARMA bipe flags for armor {:#010x} from {:#010x} to {:#010x}", armor->GetFormID(), arma_former, taxon.arma_slots) + CSV_LINENO);
                        }
                        else {
                            logger::warn(std::format("tried to modify ARMA flags for armor {:#010x} but there were no armor attachments", armor->GetFormID()) + CSV_LINENO);
                        }
                    }
                    else {
                        logger::warn(std::format("{} items specify clothing type {} in one entry, but only 1 item can appear if a clothing type is given", armors.size(), columns[6]) + CSV_LINENO);
                    }
                }
                else {
                    logger::warn(std::format("item specifies clothing type {} but that type does not exist so no slots will be changed", columns[6]) + CSV_LINENO);
                }
            }

            logger::debug(filename + std::string(": registering a set of ")
                + std::to_string(armors.size())
                + std::format(" armors with {} potential omods", omods.size())
                + std::format(" for level{} + characters as ", level)
                + (localNSFW ? std::string("NSFW") : std::string("SFW"))
                + std::format(" for occupation map {:#10x}", occupations)
                + CSV_LINENO);
            index.registerTuple(level, localNSFW, sexes, occupations, armors);
            if (omods.size() > 0) {
                if (!index.registerOmods(armors, omods, localNSFW)) {
                    logger::warn(std::string("omod registration failed") + CSV_LINENO);
                }
            }
            count += 1;
        }
        logger::info(std::format("Registered {} sets from file {}", count, filename));
    }
}
