#include "plugin_reader.h"

static RE::ENUM_FORM_ID FourCCToFormEnum(std::uint32_t sig)
{
    switch (sig) {
        // Armor & Addon
    case 'OMRA': return RE::ENUM_FORM_ID::kARMO; // "ARMO"
    case 'AMRA': return RE::ENUM_FORM_ID::kARMA; // "ARMA"
    case 'DOMO': return RE::ENUM_FORM_ID::kOMOD; // "OMOD"

        // Weapons, Ammo, Misc
    case 'NOPW': return RE::ENUM_FORM_ID::kWEAP; // "WEAP"
    case 'OMMA': return RE::ENUM_FORM_ID::kAMMO; // "AMMO"
    case 'CSIM': return RE::ENUM_FORM_ID::kMISC; // "MISC"

        // NPCs, Races, Factions
    case 'CPON': return RE::ENUM_FORM_ID::kNPC_; // "NPC_"
    case 'ECAR': return RE::ENUM_FORM_ID::kRACE; // "RACE"
    case 'TCAF': return RE::ENUM_FORM_ID::kFACT; // "FACT"

        // World / Cells
    case 'DLRW': return RE::ENUM_FORM_ID::kWRLD; // "WRLD"
    case 'LLEC': return RE::ENUM_FORM_ID::kCELL; // "CELL"

        // Quests, Dialog
    case 'TSUQ': return RE::ENUM_FORM_ID::kQUST; // "QUST"
    case 'GOLD': return RE::ENUM_FORM_ID::kDIAL; // "DIAL"
    case 'ETON': return RE::ENUM_FORM_ID::kNOTE; // "NOTE"

        // Keywords, Globals
    case 'YEK_': return RE::ENUM_FORM_ID::kKYWD; // "KYWD"
    case 'LBOL': return RE::ENUM_FORM_ID::kGLOB; // "GLOB"

        // Misc. commonly used
    case 'EVIL': return RE::ENUM_FORM_ID::kLVLI; // "LVLI"
    //case 'ECAF': return RE::ENUM_FORM_ID::kFACE; // "FACE"
    //case 'GPMS': return RE::ENUM_FORM_ID::kMSGP; // "MSGP"

    default:
        return RE::ENUM_FORM_ID::kNONE;
    }
}

static constexpr std::uint32_t FOURCC(char a, char b, char c, char d) {
    return (std::uint32_t(std::uint8_t(a))) |
        (std::uint32_t(std::uint8_t(b)) << 8) |
        (std::uint32_t(std::uint8_t(c)) << 16) |
        (std::uint32_t(std::uint8_t(d)) << 24);
}

static constexpr std::uint32_t SIG_TES4 = FOURCC('T', 'E', 'S', '4');
static constexpr std::uint32_t SIG_GRUP = FOURCC('G', 'R', 'U', 'P');  // chunk id for groups (we'll ignore)
static constexpr std::uint32_t SIG_EDID = FOURCC('E', 'D', 'I', 'D');  // subrecord we want

static std::uint32_t ComposeRuntimeFormID(const RE::TESFile* file, std::uint32_t localID)
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

static inline void fourcc_to_text(uint32_t v, char out[5]) {
    std::memcpy(out, &v, 4); out[4] = '\0';
}

void ProbeFile(RE::TESFile* f) {
    if (!f || !f->IsActive()) return;
    logger::trace(std::format("Probing {}", f->filename));
    if (!f->OpenTES(RE::NiFile::OpenMode::kReadOnly, true)) return;

    uint32_t forms = 0;

    // Consume TES4 (top-level record)
    if (f->NextForm(false)) {
        ++forms;
        char rec[5]; fourcc_to_text(f->currentform.form, rec);
        logger::trace(std::format("FORM {} (local {:#010x}) [top-level]", rec, f->currentform.formID));
        // Peek first few chunks of TES4:
        for (int i = 0; i < 3; ++i) {
            uint32_t cid = f->GetTESChunk();
            if (cid == 0) break;
            char chk[5]; fourcc_to_text(cid, chk);
            logger::trace(std::format("  CHUNK {} ({} bytes)", chk, f->actualChunkSize));
            if (!f->NextChunk()) break;
        }
    }

    // Now walk groups -> records
    while (f->NextGroup()) {
        while (f->NextForm(false)) {
            ++forms;
            char rec[5]; fourcc_to_text(f->currentform.form, rec);
            logger::trace(std::format("FORM {} (local {:#010x})", rec, f->currentform.formID));

            // Only dive into chunks if you care (e.g., to fetch EDID)
            // for (;;) { uint32_t cid = f->GetTESChunk(); ... if (!f->NextChunk()) break; }
        }
    }

    f->CloseTES(true);
    logger::debug(std::format("Scanned {} forms from plugin {}", forms, f->GetFilename().data()));
}

void IndexOneTESFile(RE::TESFile* file, const std::function<bool(RE::TESFile* file, RE::ENUM_FORM_ID formType, const std::string& edid, uint32_t formID)>& cb)
{
    ProbeFile(file);

    if (!file) {
        logger::warn("TESFile was NULL");
        return;
    }

    if (!file->IsActive()) {
        logger::warn(std::format("TES file {} was not active", file->filename));
        return;
    }

    // Open for read (engine will handle resource path/locking)
    if (!file->OpenTES(RE::NiFile::OpenMode::kReadOnly, /*lock*/true)) {
        logger::warn(std::format("Could not open TES file {}", file->filename));
        return;
    }

    // Walk records
    uint32_t numForms = 0;
    //while (file->NextGroup()) {
        while (file->NextForm(/*skipIgnored*/false)) {
            numForms++;
            logger::trace(std::format("  .. Seen Form 4CC == {:#010x}", file->currentform.form));

            // Skip the file header record itself
            if (file->currentform.form == SIG_TES4) {
                continue;
            }

            const std::uint32_t localID = file->currentform.formID;      // local id as stored in this file
            const std::uint32_t runtimeID = ComposeRuntimeFormID(file, localID);

            // Iterate subrecords (chunks) of this record
            while (true) {
                const std::uint32_t chunk = file->GetTESChunk();          // 4CC of current subrecord
                if (chunk == 0) {
                    // Some builds return 0 when no more chunks are available for this record
                    break;
                }

                if (chunk == SIG_EDID) {
                    // We know how big it is:
                    const std::uint32_t sz = file->actualChunkSize;
                    std::string edid;
                    edid.resize(sz); // EDID is a raw bytestring; usually ASCII, not null-terminated

                    // Copy the payload into our buffer
                    if (file->GetChunkData(edid.data(), sz)) {
                        logger::trace(std::format("  ..   .. with formid {:#010x}, edid {}", runtimeID, edid));
                        RE::ENUM_FORM_ID formType = FourCCToFormEnum(file->currentform.form);
                        if (formType != RE::ENUM_FORM_ID::kNONE) {
                            if (!cb(file, formType, edid, runtimeID)) {
                                logger::warn("callback indicated we should abort");
                                goto abort;
                            }
                        }
                        break;
                        // We could break here, but keep walking to safely advance the chunk cursor
                    }
                }

                // advance to next subrecord; false => this record has no more chunks
                if (!file->NextChunk()) {
                    break;
                }
            }
        }
    //}

    logger::debug(std::format("Scanned {} forms from plugin {}", numForms, file->filename));

abort:
    // Always close when done
    (void) file->CloseTES(/*forceClose*/true);
}
