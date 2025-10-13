#include "texture_index.h"
#include "csv_scanner.h"
#include "benchmark.h"

struct BA2Header {
    char     magic[4];        // "BTDX" (some docs flip to "BDTX")
    uint32_t version;         // 1
    char     type[4];         // "GNRL" or "DX10"
    uint32_t fileCount;
    uint64_t nameTableOffset; // absolute file offset of the name table
};

static inline std::string NormalizeLowerSlash(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool ReadBA2NameTable(const std::filesystem::path& ba2, std::unordered_set<std::string>& outNames)
{
    std::ifstream f(ba2, std::ios::binary);
    if (!f) return false;

    BA2Header h{};
    if (!f.read(reinterpret_cast<char*>(&h), sizeof(h))) return false;
    if (std::string_view(h.magic, 4) != "BTDX") return false; // be strict; change if you need to accept both

    // Seek to name table and read fileCount entries: [u16 length][bytes...]
    f.seekg(static_cast<std::streamoff>(h.nameTableOffset), std::ios::beg);
    outNames.reserve(outNames.size() + h.fileCount);

    for (uint32_t i = 0; i < h.fileCount; ++i) {
        uint16_t len = 0;
        if (!f.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
        if (len == 0) { continue; }

        std::string s(len, '\0');
        if (!f.read(s.data(), len)) return false;

        // Some archives may include a trailing NUL; if present, strip it.
        if (!s.empty() && s.back() == '\0') s.pop_back();

        std::string path = NormalizeLowerSlash(std::move(s));
        //logger::trace(std::format("  : SEEN TEXTURE {}", path));
        outNames.insert(path);
    }
    return true;
}

bool TextureIndex::contains(const std::string& path)
{
    if (index.size() == 0) {
        // Index isn't built yet.
        // We have no choice but to index every BA2 file, because
        // many mods depend on materials from the various vanilla and DLCs.
        // We can't easily step through RE::TESFiles because the vanilla files
        // have various naming conventions (too much work to replicate).
        // We also can't be sure some other mod dependency isn't involved.
        // There's also the possibility of tweaked INIs with BA2s specified there.
        // Granted it's possible not every BA2 present will be actually used.
        // But indexing them all is probably the safest heuristic at this point.
        benchmark("building texture index", [&] {
            for (auto ba2 : scandir(GetDataDir(), ".ba2", false)) {
                logger::debug(std::format("scanning BA2 {}", ba2));
                ReadBA2NameTable(ba2, index);
            }
        });
    }

    return index.contains(NormalizeLowerSlash(path));
}
