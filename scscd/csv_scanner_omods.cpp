#include "scscd.h"
#include "matswap_validity_report.h"

void scan_omods_csv(std::filesystem::path basedir, bool nsfw, ArmorIndex& index) {
    logger::info("Loading omods from " + basedir.string());
    std::vector<std::string> filenames = scandir(basedir, ".csv");
    for (std::string filename : filenames) {
        std::filesystem::path fullpath = basedir / filename;
        logger::debug("Parsing omods file " + fullpath.string());
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
                // ArmorFormID[;ArmorFormID],OmodFormID[;OmodFormID]
                logger::error(std::format("skipped: expected 2 fields, got {}", columns.size()) + CSV_LINENO);
                continue;
            }

            if (istartswith(columns[0], "ArmorFormID")) // header
                continue;

            std::vector<std::string> armorFormIDs = split_and_trim(columns[0], ';');
            if (armorFormIDs.size() == 0) {
                logger::error(std::string("skipped: no form IDs in column 1") + CSV_LINENO);
                continue;
            }

            std::vector<std::string> omodFormIDs = split_and_trim(columns[1], ';');
            if (omodFormIDs.size() == 0) {
                logger::error(std::string("skipped: no form IDs in column 2") + CSV_LINENO);
                continue;
            }

            std::vector<RE::TESObjectARMO*> armors = parseFormIDs<RE::TESObjectARMO>(plugin_file, armorFormIDs);
            if (armors.size() == 0) {
                logger::error(std::string("skipped: no armors specified or at least one of them failed validation") + CSV_LINENO);
                continue;
            }

            std::vector<RE::BGSMod::Attachment::Mod*> omods = parseFormIDs<RE::BGSMod::Attachment::Mod>(plugin_file, omodFormIDs);
            if (omods.size() == 0) {
                logger::error(std::string("skipped: no omods specified or at least one of them failed validation") + CSV_LINENO);
                continue;
            }

            /*
             validate omods: if we can reach into the BSMaterialSwap we should be able to use RE::BSResourceNiBinaryStream
             to try and open the material/texture files (even if it's in an archive) to check whether the materials are actually
             available. If we do this we won't have any purple outfits at runtime if someone's got an ESP but missing the
             textures.
            */
            bool valid = true;
            for (RE::BGSMod::Attachment::Mod* mod : omods) {
                if (mod->swapForm) {
                    SwapPreflightReport report = PreflightValidateBGSMTextures(mod->swapForm);
                    if (report.missingMaterials.size() > 0 || report.missingTextures.size() > 0) {
                        logger::error(std::format("skipped: omod {:#010x} failed validation:", mod->GetFormID()) + CSV_LINENO);
                        for (auto& filename : report.missingMaterials) {
                            logger::error(std::format("    -> material {} could not be found", filename) + CSV_LINENO);
                            valid = false;
                        }
                        for (auto& filename : report.missingTextures) {
                            logger::error(std::format("    -> texture {} could not be found", filename) + CSV_LINENO);
                            valid = false;
                        }
                    }
                }
            }
            if (!valid) continue;

            logger::debug(std::format("{}: registering a set of {} {} omods to a set of {} armors", filename, omods.size(), nsfw ? "NSFW" : "SFW", armors.size()) + CSV_LINENO);
            index.registerOmods(armors, omods, nsfw);
            count += 1;
        }
        logger::info(std::format("Registered {} omods from file {}", count, filename));
    }
}
