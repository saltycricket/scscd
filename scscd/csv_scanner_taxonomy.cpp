#include "scscd.h"
#include "csv_scanner.h"

static uint32_t parseSlots(std::string& str) {
    std::vector<std::string> slots = split_and_trim(str, ';');
    uint32_t mask = 0;
    for (std::string slot : slots) {
        int bit = atoi(slot.c_str()) - 30; // slot 33 -> bit 3
        mask = mask | (1 << bit);
    }
    return mask;
}

void scan_taxonomies_csv(std::filesystem::path basedir, std::unordered_map<std::string, Taxon>& index) {
    logger::info("Loading taxonomy from " + basedir.string());
    std::vector<std::string> filenames = scandir(basedir, ".csv");
    for (std::string filename : filenames) {
        std::filesystem::path fullpath = basedir / filename;
        logger::debug("Parsing taxonomy file " + fullpath.string());
        std::ifstream file(fullpath);
        if (!file) {
            logger::warn("Could not open file " + fullpath.string());
            continue;
        }
        int lineno = 0;
        std::string line;
        uint32_t count = 0;
        while (std::getline(file, line)) {
            lineno += 1;
            // strip comments
            if (size_t n = line.find('#'); n != std::string::npos)
                line = line.substr(0, n);
            std::vector<std::string> columns = csv_parse_line(line);
            // skip blank lines (after comments)
            if (columns.size() == 0 || (columns.size() == 1 && columns[0] == ""))
            {
                // blank line
                continue;
            }
            if (columns.size() != 3) {
                // Name,ARMO,ARMA
                logger::warn(std::format("skipped: expected 3 columns, got {}", columns.size()) + CSV_LINENO);
                continue;
            }
            // if we got here, we think the line is pareseable
            std::string name = columns[0];
            if (iequals(name, "Name")) // header
                continue;
            
            Taxon taxon(parseSlots(columns[1]), parseSlots(columns[2]));
            logger::debug(filename + std::format(": registering taxon {} as armo={:#010x}, arma={:#010x}", name, taxon.armo_slots, taxon.arma_slots));
            if (index.contains(name))
                logger::warn(std::format("duplicate taxon {} will replace the earlier definition", name) + CSV_LINENO);
            index[name] = taxon;
            count += 1;
        }
        logger::info(std::format("Registered {} taxons from file {}", count, filename));
    }
}
