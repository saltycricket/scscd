#include "scscd.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>

struct SwapPreflightReport
{
    std::vector<std::string> missingMaterials;  // .bgsm/.bgem that couldn't be opened
    std::vector<std::string> missingTextures;   // .dds that couldn't be opened
    std::vector<std::string> okMaterials;
    std::vector<std::string> okTextures;
};

namespace detail
{
    // Open and test via operator bool()
    inline bool ResourceExists(const char* path)
    {
        if (!path || !*path) return false;

        RE::BSResourceNiBinaryStream s(path);
        // Optionally: fall back to a rescan-aware open if you need it:
        // if (!s) { std::unique_ptr<RE::BSResourceNiBinaryStream> rs(RE::BSResourceNiBinaryStream::BinaryStreamWithRescan(path)); return rs && static_cast<bool>(*rs); }
        return static_cast<bool>(s);
    }

    // Read whole file through NiBinaryStream; no std::filesystem, works for BA2.
    inline bool ReadWholeFile(const char* path, std::vector<std::uint8_t>& out)
    {
        out.clear();
        if (!path || !*path) return false;

        RE::BSResourceNiBinaryStream s(path);
        if (!s) return false;

        // Try to get a size hint (not strictly required)
        RE::NiBinaryStream::BufferInfo info{};
        s.GetBufferInfo(info);
        if (info.fileSize > 0 && info.fileSize < (1ull << 31)) {  // guard against bogus sizes
            out.reserve(static_cast<std::size_t>(info.fileSize));
        }
        else {
            out.reserve(4096);
        }

        // Read loop (binary_read/DoRead both work; prefer binary_read wrapper)
        std::uint8_t buf[64 * 1024];
        for (;;) {
            std::size_t got = s.binary_read(buf, sizeof(buf));
            if (got == 0) break;
            out.insert(out.end(), buf, buf + got);
        }

        // Treat zero-length as success (some resources are legitimately empty)
        // If you want to be strict, require out.size()>0 instead.
        return true;
    }

    // Extract ASCII substrings that contain ".dds" (case-insensitive), e.g. "textures\\foo\\bar_d.dds"
    inline std::vector<std::string> ExtractDDSPaths(const std::vector<std::uint8_t>& bytes)
    {
        std::vector<std::string> paths;
        std::string cur;

        auto flush = [&]() {
            if (cur.size() >= 5) { // "a.dds" minimal-ish
                // Quick check for ".dds" (case-insensitive)
                std::string low = cur;
                std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return std::tolower(c); });
                if (low.find(".dds") != std::string::npos) {
                    // normalize slashes a bit
                    for (auto& ch : cur) if (ch == '\\') ch = '/';
                    // strip surrounding spaces
                    auto l = cur.find_first_not_of(' ');
                    auto r = cur.find_last_not_of(' ');
                    if (l != std::string::npos && r != std::string::npos) {
                        paths.emplace_back(cur.substr(l, r - l + 1));
                    }
                }
            }
            cur.clear();
            };

        // Acceptable filename characters in BGSM/BGEM strings are usually printable ASCII
        auto is_ok = [](unsigned char c) -> bool {
            return c >= 32 && c <= 126; // printable
            };

        for (auto b : bytes) {
            if (is_ok(b)) {
                cur.push_back(static_cast<char>(b));
                // Heuristic guard: avoid runaway extremely long 'strings'
                if (cur.size() > 512) flush();
            }
            else {
                flush();
            }
        }
        flush();

        // De-dup (case-insensitive)
        std::unordered_set<std::string> seen;
        std::vector<std::string> unique;
        for (auto& p : paths) {
            std::string key = p;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
            if (!seen.count(key)) {
                seen.insert(std::move(key));
                unique.emplace_back(std::move(p));
            }
        }
        return unique;
    }
}

inline SwapPreflightReport PreflightValidateBGSMTextures(const RE::BGSMaterialSwap* swap)
{
    using namespace detail;
    SwapPreflightReport report;

    if (!swap) {
        report.missingMaterials.emplace_back("<null swap>");
        return report;
    }

    // swap->swapMap: BSFixedString -> Entry{ BSFixedString swapMaterial, float colorRemap }
    for (const auto& kv : swap->swapMap) {
        const auto& entry = kv.second;
        const char* matPath = entry.swapMaterial.c_str();
        if (!matPath || !*matPath) {
            report.missingMaterials.emplace_back("<empty material path>");
            continue;
        }

        // 1) Can we open the BGSM/BGEM at all?
        std::vector<std::uint8_t> bytes;
        if (!ReadWholeFile(matPath, bytes)) {
            report.missingMaterials.emplace_back(matPath);
            continue;
        }
        report.okMaterials.emplace_back(matPath);

        // 2) Extract all .dds-like substrings from the material bytes
        auto ddsPaths = ExtractDDSPaths(bytes);
        if (ddsPaths.empty()) {
            // Some minimal materials might not reference textures (rare), treat as OK
            continue;
        }

        // 3) Probe each .dds via resource loader
        for (auto& tex : ddsPaths) {
            if (ResourceExists(tex.c_str())) {
                report.okTextures.emplace_back(tex);
            }
            else {
                report.missingTextures.emplace_back(tex);
            }
        }
    }

    return report;
}
