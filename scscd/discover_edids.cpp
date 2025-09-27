#include "scscd.h"
#include "armor_index.h"

std::unordered_map<RE::ENUM_FORM_ID, std::unordered_map<std::string, uint32_t>> ArmorIndex::FORMS_BY_EDID_BY_TYPE;

constexpr uint32_t FOURCC(char a, char b, char c, char d) {
    return (uint32_t(uint8_t(a))) |
        (uint32_t(uint8_t(b)) << 8) |
        (uint32_t(uint8_t(c)) << 16) |
        (uint32_t(uint8_t(d)) << 24);
}

//static constexpr std::uint32_t SIG_TES4 = FOURCC('T', 'E', 'S', '4');
//static constexpr std::uint32_t SIG_GRUP = FOURCC('G', 'R', 'U', 'P');  // chunk id for groups (we'll ignore)
//static constexpr std::uint32_t SIG_EDID = FOURCC('E', 'D', 'I', 'D');
//
static inline std::uint32_t ComposeRuntimeFormID(const RE::TESFile* file, std::uint32_t localID)
{
    if (file->IsLight()) {
        const auto small = file->GetSmallFileCompileIndex() & 0x0FFFu;
        return 0xFE000000u | (small << 12) | (localID & 0x0FFFu);
    }
    else {
        const auto full = file->GetCompileIndex();
        return (std::uint32_t(full) << 24) | (localID & 0x00FFFFFFu);
    }
}

static inline RE::ENUM_FORM_ID FourCCToFormEnum(std::uint32_t sig)
{
    switch (sig) {
        // Armor & Addon
    case FOURCC('A','R','M','O'): return RE::ENUM_FORM_ID::kARMO;
    case FOURCC('A','R','M','A'): return RE::ENUM_FORM_ID::kARMA;
    case FOURCC('O','M','O','D'): return RE::ENUM_FORM_ID::kOMOD;
        // NPCs, Races, Factions
    case FOURCC('N','P','C','_'): return RE::ENUM_FORM_ID::kNPC_;
    case FOURCC('R','A','C','E'): return RE::ENUM_FORM_ID::kRACE;
    case FOURCC('F','A','C','T'): return RE::ENUM_FORM_ID::kFACT;
    case FOURCC('C','L','A','S'): return RE::ENUM_FORM_ID::kCLAS;
    default:
        return RE::ENUM_FORM_ID::kNONE;
    }
}

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <zlib.h> // link zlib


struct RecordHeader {
    char     sig[4];        // e.g. "ARMO", "NPC_", "OMOD", ...
    uint32_t dataSize;
    uint32_t flags;
    uint32_t formID;
    uint32_t vcInfo1;
    uint16_t formVersion;
    uint16_t vcInfo2;
};

struct GroupHeader {
    char sig[4];       // "GRUP"
    uint32_t groupSize;
    uint32_t label;
    uint32_t type;
    uint16_t stamp;
    uint16_t unknown;
};

static bool read_exact(std::ifstream& f, void* p, size_t n) {
    return !!f.read(reinterpret_cast<char*>(p), n);
}

static std::vector<uint8_t> inflate_zlib(const uint8_t* src, size_t sz, size_t expected) {
    std::vector<uint8_t> out(expected);
    z_stream zs{};
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
    zs.avail_in = static_cast<uInt>(sz);
    zs.next_out = reinterpret_cast<Bytef*>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());
    if (inflateInit(&zs) != Z_OK) return {};
    int ret = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (ret != Z_STREAM_END || zs.total_out != expected) return {};
    return out;
}

static inline uint16_t rd_le16(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}
static inline uint32_t rd_le32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

// data: the record payload *after* the record header (already decompressed if needed)
static std::string parse_edid_from_record_bytes(const std::vector<uint8_t>& data)
{
    const uint8_t* buf = data.data();
    const size_t n = data.size();

    size_t off = 0;
    uint32_t pendingBig = 0; // carries XXXX size to the next subrecord only

    while (off + 6 <= n) {
        const char* st = reinterpret_cast<const char*>(buf + off);
        char type[5]{ st[0], st[1], st[2], st[3], 0 };
        uint16_t sz16 = rd_le16(buf + off + 4);
        off += 6;

        // XXXX extends size of the *next* subrecord
        if (std::memcmp(type, "XXXX", 4) == 0) {
            // Per spec, size must be 4, and payload is a uint32 little-endian
            if (sz16 != 4 || off + 4 > n) break;
            pendingBig = rd_le32(buf + off);
            off += 4;
            // Loop continues; we do not consume any "real" subrecord yet
            continue;
        }

        // Effective data size for this subrecord
        uint32_t dsz = pendingBig ? pendingBig : sz16;
        pendingBig = 0;

        if (off + dsz > n) break; // bounds guard

        if (std::memcmp(type, "EDID", 4) == 0) {
            // EDID is a Z-string (ASCII/UTF-8) inside dsz bytes.
            const char* s = reinterpret_cast<const char*>(buf + off);
            size_t len = 0;
            while (len < dsz && s[len] != '\0') ++len;
            return std::string(s, len);
        }

        // Skip payload of other subrecords
        off += dsz;
    }

    return {}; // no EDID found
}

static void process_range(RE::TESFile* tesfile,
                          std::ifstream& f,
                          std::uint64_t end_offset,
                          std::function< bool(RE::ENUM_FORM_ID formtype, const std::string& edid, uint32_t formid)> fn)
{
    while (true) {
        auto here = static_cast<std::uint64_t>(f.tellg());
        if (!f || here >= end_offset) break;

        char tag[4];
        if (!read_exact(f, tag, 4)) break;

        if (std::memcmp(tag, "GRUP", 4) == 0) {
            GroupHeader gh;
            std::memcpy(gh.sig, tag, 4);
            if (!read_exact(f, reinterpret_cast<char*>(&gh) + 4, sizeof(gh) - 4)) return;

            const std::uint64_t group_start = here;
            const std::uint64_t group_end = group_start + gh.groupSize;
            const std::uint64_t payload_beg = here + 24;

            // Top-level groups (type==0): label is record 4CC. Filter here.
            if (gh.type == 0) {
                uint32_t label = gh.label; // already little-endian 4CC
                if (FourCCToFormEnum(label) == RE::ENUM_FORM_ID::kNONE) {
                    // Skip whole group
                    f.seekg(group_end, std::ios::beg);
                    continue;
                }
            }
            else {
                // Not top-level: if we don't need REFR/ACHR/etc, skip entirely.
                f.seekg(group_end, std::ios::beg);
                continue;
            }

            // Recurse into this group's payload
            f.seekg(payload_beg, std::ios::beg);
            process_range(tesfile, f, group_end, fn);

            // Ensure we're positioned at the end of this group to continue
            f.seekg(group_end, std::ios::beg);
            continue;
        }

        // Record path: we already consumed 4 bytes of signature
        RecordHeader rh{};
        std::memcpy(rh.sig, tag, 4);
        if (!read_exact(f, reinterpret_cast<char*>(&rh) + 4, sizeof(rh) - 4)) return;

        // Read record data buffer (handle compression)
        std::vector<std::uint8_t> rdata;
        if (rh.flags & 0x00040000) {
            std::uint32_t uncompressedSize = 0;
            if (!read_exact(f, &uncompressedSize, 4)) return;
            std::vector<std::uint8_t> comp(rh.dataSize - 4);
            if (!comp.empty() && !read_exact(f, comp.data(), comp.size())) return;
            rdata = inflate_zlib(comp.data(), comp.size(), uncompressedSize); // your inflater
            if (rdata.empty()) return; // or continue on error
        }
        else {
            rdata.resize(rh.dataSize);
            if (rh.dataSize && !read_exact(f, rdata.data(), rdata.size())) return;
        }

        // Parse subrecords in rdata (your existing EDID scan with XXXX handling)
        std::string edid = parse_edid_from_record_bytes(rdata); // keep your logic
        if (!edid.empty()) {
            const uint32_t fourcc = FOURCC(rh.sig[0], rh.sig[1], rh.sig[2], rh.sig[3]);
            if (!fn(FourCCToFormEnum(fourcc), edid, ComposeRuntimeFormID(tesfile, rh.formID))) {
                logger::trace("callback indicated to cease iteration");
                break;
            }
        }
    }
}

static void scanFormsInFile(RE::TESFile* tesfile, std::function< bool(RE::ENUM_FORM_ID formtype, const std::string& edid, uint32_t formid)> fn) {
    logger::trace(std::format("discovering edids in file {}", tesfile->filename));

    std::filesystem::path path = DataPath(tesfile->filename);
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        logger::warn(std::format("cannot access file {}", path.string()));
        return;
    }

    // Whole file range
    f.seekg(0, std::ios::end);
    const auto file_end = static_cast<std::uint64_t>(f.tellg());
    f.seekg(0, std::ios::beg);

    // First thing is the TES4 record (not a GRUP) - parse it like any record:
    // process_range() will handle it naturally because it treats both records and groups.
    process_range(tesfile, f, file_end, fn);
}

void ArmorIndex::indexAllFormsByTypeAndEdid() {
    auto* dh = RE::TESDataHandler::GetSingleton();
    int count = 0;

    auto fn = [&count](RE::ENUM_FORM_ID formtype, const std::string& edid, uint32_t formid) {
        if (!FORMS_BY_EDID_BY_TYPE[formtype].contains(edid)) {
            logger::trace(std::format("saw form {:#010x} with type {:#06x} and edid {}", formid, (uint32_t) formtype, edid));
            FORMS_BY_EDID_BY_TYPE[formtype][edid] = formid;
            count++;
        }
        return true;
    };
    
    // full plugins (masters + ESPs)
    for (auto* f : dh->compiledFileCollection.files) {
        if (f && f->IsActive())
            scanFormsInFile(f, fn);
    }
    // light plugins (ESLs)
    for (auto* f : dh->compiledFileCollection.smallFiles) {
        if (f && f->IsActive())
            scanFormsInFile(f, fn);
    }

    logger::info(std::format("scanned active plugins and saw {} compatible forms.", count));
}
