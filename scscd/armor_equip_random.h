#pragma once

#include <vector>
#include <random>
#include <algorithm>
#include <cstdint>
#include <unordered_set>

#include "RE/Fallout.h"
#include "omod_index.h"

/* *******************************************************************************
 * 
 * The goal of this class is to equip a random OMOD onto a given piece of armor.
 * Essentially, it was trying to achieve variety in material swaps. If other mods
 * like weave came along, that'd be a bonus. It was intended to be able to replicate
 * a naive leveled-lists implementation by assigning an optional level to each
 * mod; perhaps we'd get those from CSVs or something. It would also attempt to match
 * color styles (with some variation so it's not too uniform), and favor material
 * swaps over other mods in general, again with some room for randomness.
 * 
 * Ok, so here's how things stand as of 20250831. We need some things to work that
 * currently don't seem to.
 * 
 * 1. GetData doesn't seem to populate the data structure no matter what I do.
 *    Without that, we can't enumerate the omod's properties, so we can't search
 *    for a MaterialSwap property, meaning we can't honor the 'prefer mat swaps'
 *    option.
 * 2. mod->swapForm seems virtually always (but not actually always?? Depends on
 *    specific ESP?) to be NULL, so we can't identify swaps going that route either
 *    - which would be easier if it worked.
 * 3. The dealbreaker: in FO4Edit we have an MNAM (Target OMOD Keywords) field. This
 *    is what 'links' an OMOD to its compatible armors. Without that we cannot
 *    identify which armors any given OMOD will work with, and experiments in choosing
 *    at random yielded way too many invalid results. Even within a particular ESP,
 *    there are often too many options for a random choice to work. This field does
 *    not seem to be available at all, so we're dead.
 *    I thought to simply add ALL mods to the instance and then let RemoveInvalidMods()
 *    do the work, but I don't see any way to iterate over the mods once attached.
 *    Without that, we would always have every attachment on every armor - seems bad.
 *    The only other option I can think of would be to iterate over every mod in the
 *    set, query the instance after RemoveInvalidMods() to see if it's still there,
 *    and if so build a second instance containing the randomness. That sounds really
 *    expensive and any caching we do would have to be done at the per-armor level:
 *    that's a ton of cache misses.
 * 
 * So the TLDR on omods is we can't do them without at least MNAM, and we can't
 * implement the cool options without either GetData or swapForm. For now, this
 * code will remain for reference only - we'll bypass it and equip un-modded
 * forms.
 * 
 * *******************************************************************************
 */
namespace EquipOMOD
{
    struct Options
    {
        std::uint32_t maxAttachmentsPerArmor = 2;
        bool          favorMaterialSwaps = true;
        bool          enforceStyle = true;
        std::uint32_t actorLevel = 1;
        std::uint32_t randomSeed = 0; // 0 => nondeterministic
    };

    class Service
    {
    public:
        explicit Service(Options opt = {})
            : opts_(opt)
            , rng_(opt.randomSeed ? opt.randomSeed : static_cast<std::uint32_t>(std::random_device{}()))
        {
        }

        // Returns how many armor pieces were successfully added & equipped
        std::size_t EquipArmorsWithRandomMods(RE::Actor* actor,
            const std::vector<RE::TESObjectARMO*>& armors)
        {
            logger::trace("> EquipArmorsWithRandomMods");
            if (!actor) {
                logger::trace("  actor is null");
                return 0;
            }

            std::size_t equipped = 0;
            reset_style();

            logger::trace("  evaluating armors");
            for (auto* armo : armors) {
                if (!armo) continue;

                // 1) Own an ExtraDataList via BSTSmartPointer to satisfy AddObjectToContainer
                logger::trace("  creating xlist");
                RE::BSTSmartPointer<RE::ExtraDataList> xlist(new RE::ExtraDataList());

                // 2) Ensure we have a BGSObjectInstanceExtra on this stack (manually create it)
                logger::trace("  creating instExtra");
                auto* instExtra = new RE::BGSObjectInstanceExtra();      // default ctor is available
                logger::trace("  adding extra to extra list");
                xlist->AddExtra(instExtra);

                // 3) Collect candidate OMODs (global map -> keys are Mod*)
                logger::trace("  collecting omod candidates");
                auto candidates = collect_candidates_for_armor(armo);

                // 4) Choose up to N OMODs (bias toward swaps + style)
                logger::trace("  choosing omods from candidates");
                auto chosen = pick_mods(candidates);

                // 5) Attach chosen OMODs to the instance
                //    Pick first non-disabled attach index if present, else 0
                //std::uint8_t attachIndex = 0;
                logger::trace("  attaching chosen omods to instance");
                //if (auto idxSpan = instExtra->GetIndexData(); !idxSpan.empty()) {  // GetIndexData()
                //    for (const auto& idx : idxSpan) {
                //        if (!idx.disabled) { 
                //            logger::trace(std::format("  using attachIndex={}", idx.index));
                //            attachIndex = idx.index;
                //            break;
                //        }
                //    }
                //}

                logger::trace("  adding mods");
                for (auto* mod : chosen) {
                    // mod attachIndex will depend on armo index at which attachPoint appears
                    uint32_t attachIndex = 0;
                    for (uint32_t i = 0; i < armo->attachParents.size; i++) {
                        uint16_t kwidx = armo->attachParents.array[i].keywordIndex;
                        RE::BGSKeyword* attachkw = RE::detail::BGSKeywordGetTypedKeywordByIndex(RE::KeywordType::kAttachPoint, kwidx);
                        if (attachkw) {
                            RE::BGSKeyword* modkw = RE::detail::BGSKeywordGetTypedKeywordByIndex(RE::KeywordType::kAttachPoint, mod->attachPoint.keywordIndex);
                            if (modkw && modkw->GetFormID() == attachkw->GetFormID()) {
                                attachIndex = i;
                                break;
                            }
                        }
                    }
                    logger::trace(std::format("    adding mod {:#010x} ({}) to attach index {} of armor {:#010x} ({})",
                        mod->GetFormID(), mod->GetFullName(), attachIndex, armo->GetFormID(), armo->GetFullName()));
                    instExtra->AddMod(*mod, attachIndex, /*rank*/ 0, /*removeInvalidMods*/ false);  // AddMod(...)
                }

                // Clean up any now-invalid combos against this armor's attach parents
                logger::trace("  pruning invalid omods");
                instExtra->RemoveInvalidMods(&armo->attachParents);  // RemoveInvalidMods(...)

                // 6) Ask the extra to synthesize base instance data for this armor
                logger::trace("  creating base instance data");
                RE::BSTSmartPointer<RE::TBO_InstanceData> instData;
                instExtra->CreateBaseInstanceData(*armo, instData);  // CreateBaseInstanceData(...)
                if (!instData) {
                    // If NG ever returns null here, skip this armor gracefully
                    // FIXME instead of skipping armor, just equip it with no mods.
                    logger::warn("  no instance data, skipping");
                    continue;
                }

                // 7) Add to inventory (uses BSTSmartPointer<ExtraDataList> per your signature)
                logger::trace("  adding object and omod list to actor's inventory");
                actor->AddObjectToContainer(
                    armo,
                    xlist,
                    /*count*/ 1,
                    /*fromContainer*/ nullptr,
                    RE::ITEM_REMOVE_REASON::kNone
                );

                // 8) Build object wrapper and equip via ActorEquipManager
                logger::trace("  building object instance");
                RE::BGSObjectInstanceT<RE::TESObjectARMO> obj(armo, instData.get()); // ctor(T*, TBO_InstanceData*)
                auto* aem = RE::ActorEquipManager::GetSingleton();
                logger::trace("  equipping instance");
                aem->EquipObject(actor, obj,
                    /*stackID*/ 0, /*number*/ 1,
                    /*slot*/ nullptr,
                    /*queue*/ false, /*force*/ false,
                    /*playSounds*/ true, /*applyNow*/ true, /*locked*/ false);

                ++equipped;
                logger::trace(std::format("  completed equipping armor number ", equipped));
            }

            logger::trace(std::format("< EquipArmorsWithRandomMods => {}", equipped));
            return equipped;
        }


    private:
        using OMOD = RE::BGSMod::Attachment::Mod;

        struct Cand {
            OMOD* mod{};
            bool  isSwap{};
            float color = -1.0f; // BGSModelMaterialSwap::colorRemappingIndex (inherited by OMOD)
        };

        void reset_style()
        {
            styleSeeded_ = false;
            targetColor_ = -1.0f;
        }

        static inline const char* cstr(std::string_view sv) { return sv.data() ? sv.data() : ""; }

        void DumpOMOD(RE::BGSMod::Attachment::Mod* omod)
        {
            if (!omod) {
                logger::trace("[OMOD] (null)");
                return;
            }

            // -------- Basic identity --------
            const auto formID = omod->GetFormID();
            std::string_view edid = {};  // editorID might be empty if not available
            if (auto ed = omod->GetFormEditorID(); ed && *ed) edid = ed;

            std::string_view fullName = {};
            if (auto fn = omod->GetFullName(); fn && *fn) fullName = fn;

            logger::trace("[OMOD] {:08X}  EDID='{}'  Name='{}'",
                formID, cstr(edid), cstr(fullName));

            // Linked loose mod (if present)
            if (auto loose = omod->GetLooseMod(); loose) {  // GetLooseMod() is exposed
                logger::trace("  LooseMod: {:08X}", loose->GetFormID());
            }
            else {
                logger::trace("  LooseMod: (none)");
            }

            // -------- Engine-populated container & data --------
            RE::BGSMod::Attachment::Mod::Data data{};
            omod->GetData(data);  // populates Container::Data + extended fields

            // Extended flags / limits
            {
                // targetFormType is an EnumSet of ENUM_FORM_ID
                // We can't introspect which bits easily without the enum->name table here,
                // but we can still print the raw byte(s) the EnumSet stores if needed.
                logger::trace("  Data.targetFormType: (EnumSet)  [cannot easily pretty-print here]");
                logger::trace("  Data.maxRank: {}", static_cast<int>(data.maxRank));
                logger::trace("  Data.lvlsPerTierScaledOffset: {}", static_cast<int>(data.lvlsPerTierScaledOffset));
                logger::trace("  Data.optional: {}", data.optional ? "true" : "false");
                logger::trace("  Data.childrenExclusive: {}", data.childrenExclusive ? "true" : "false");
            }

            // Container: attachments & property mods
            logger::trace("  Container: attachmentCount={}, propertyModCount={}",
                data.attachmentCount, data.propertyModCount);

            // ---- Attachments
            logger::trace("  [data] attachments count: {}", data.attachmentCount);
            for (std::uint32_t i = 0; i < data.attachmentCount; ++i) {
                const auto& inst = data.attachments[i];  // Attachment::Instance
                const auto* child = inst.mod;
                logger::trace("    [Attachment #{}] index={}, optional={}, childrenExclusive={}, mod={:08X}",
                    i,
                    static_cast<int>(inst.index),
                    inst.optional ? "true" : "false",
                    inst.childrenExclusive ? "true" : "false",
                    child ? child->GetFormID() : 0u);

                if (child) {
                    // A tiny peek at child name if available, to make reading easier
                    std::string_view cfn{};
                    if (auto nm = child->GetFullName(); nm && *nm) cfn = nm;
                    logger::trace("      -> Child OMOD: Name='{}'", cstr(cfn));
                }
            }

            // ---- Property Mods
            // Each describes a (target, op, type, step) with data union
            logger::trace("  [data] property mod count: {}", data.propertyModCount);
            for (std::uint32_t i = 0; i < data.propertyModCount; ++i) {
                const auto& pm = data.propertyMods[i];

                // Decode enums as integral for logging.
                const std::uint32_t target = pm.target;
                const std::uint32_t op = static_cast<std::uint32_t>(pm.op);
                const std::uint32_t type = static_cast<std::uint32_t>(pm.type);

                logger::trace("    [PropMod #{}] target={}, op={}, type={}, step={}",
                    i, target, op, type, static_cast<int>(pm.step));

                // The data union interpretation depends on 'type'.
                // We'll log the common possibilities without assuming internal helpers:
                switch (pm.type) {
                case RE::BGSMod::Property::TYPE::kInt:
                    logger::trace("      data.mm: min.i={}, max.i={}", pm.data.mm.min.i, pm.data.mm.max.i);
                    break;
                case RE::BGSMod::Property::TYPE::kFloat:
                    logger::trace("      data.mm: min.f={}, max.f={}", pm.data.mm.min.f, pm.data.mm.max.f);
                    break;
                case RE::BGSMod::Property::TYPE::kBool:
                    logger::trace("      data.mm: min.i={}, max.i={} (bool as int range)", pm.data.mm.min.i, pm.data.mm.max.i);
                    break;
                case RE::BGSMod::Property::TYPE::kString:
                    logger::trace("      data.str: '{}'", pm.data.str.c_str());
                    break;
                case RE::BGSMod::Property::TYPE::kForm:
                    logger::trace("      data.form: {:08X}", pm.data.form ? pm.data.form->GetFormID() : 0u);
                    break;
                case RE::BGSMod::Property::TYPE::kEnum:
                    logger::trace("      data.mm: min.i={}, max.i={} (enum as int range)", pm.data.mm.min.i, pm.data.mm.max.i);
                    break;
                case RE::BGSMod::Property::TYPE::kPair:
                    logger::trace("      data.fv: formID={:08X}, value={}", pm.data.fv.formID, pm.data.fv.value);
                    break;
                default:
                    logger::trace("      data: (unknown type)");
                    break;
                }
            }

            logger::trace("  filter keywords count: {}", omod->filterKeywords.size);
            for (uint32_t i = 0; i < omod->filterKeywords.size; i++) {
                auto* kw = RE::detail::BGSKeywordGetTypedKeywordByIndex(RE::KeywordType::kInstantiationFilter, omod->filterKeywords.array[i].keywordIndex);
                if (kw)
                    logger::trace(std::format("    filterKeyword[{}]: {:08X}: {}", i, kw->GetFormID(), kw->GetFormEditorID()));
                else
                    logger::trace(std::format("    filterKeyword[{}]: null", i));
            }

            logger::trace("  attach parents count: {}", omod->attachParents.size);
            for (uint32_t i = 0; i < omod->attachParents.size; i++) {
                auto* kw = RE::detail::BGSKeywordGetTypedKeywordByIndex(RE::KeywordType::kAttachPoint, omod->attachParents.array[i].keywordIndex);
                if (kw)
                    logger::trace(std::format("    attachKeyword[{}]: {:08X}: {}", i, kw->GetFormID(), kw->GetFormEditorID()));
                else
                    logger::trace(std::format("    attachKeyword[{}]: null", i));
            }
        }

        std::vector<Cand> collect_candidates_for_armor(RE::TESObjectARMO* armo)
        {
            logger::trace("> collect_candidates_for_armor");
            std::vector<Cand> out;

            // Engine map: Mod* -> Loose mod misc; key iteration enumerates OMODs
            logger::trace("  getting all loose omods");
            //auto& all = RE::BGSMod::Attachment::GetAllLooseMods();  // :contentReference[oaicite:12]{index=12}
            std::vector<RE::BGSMod::Attachment::Mod*> all = OmodIndex::Instance().ForArmor();
            logger::trace(std::format("  reserving {} candidates", all.size()));
            out.reserve(all.size());

            logger::trace("  evaluating omods");
            for (auto& kv : all) {
                auto* mod = const_cast<RE::BGSMod::Attachment::Mod*>(kv);
                if (!mod) {
                    logger::trace("  an omod was null");
                    continue;
                }

                // Trying to filter down to mods compatible with a give armor. What I really need is the
                // MNAM block (Target OMOD Keywords) but that doesn't seem available. So as a heuristic
                // I will only allow mods that appear in the same TESFile.
                // [Actually this isn't viable either as one file can still define way too many incompatible
                // omods, but the file heuristic still helps a lot when dumping OMODs.]
                if (mod->GetFile() != armo->GetFile())
                    continue;
                DumpOMOD(mod);


                RE::BGSKeyword* apkw = RE::detail::BGSKeywordGetTypedKeywordByIndex(RE::KeywordType::kAttachPoint, mod->attachPoint.keywordIndex);
                if (!apkw || !armo->attachParents.HasKeyword(apkw))
                    continue;
                logger::trace("  building candidate");
                Cand c{};
                c.mod = mod;
                c.isSwap = false;
                RE::BGSMod::Attachment::Mod::Data data{};
                mod->GetData(data);
                for (uint32_t i = 0; i < data.propertyModCount; i++) {
                    if (data.propertyMods[i].type == RE::BGSMod::Property::TYPE::kPair) {
                        RE::TESForm* form = RE::TESForm::GetFormByID(data.propertyMods[i].data.fv.formID);
                        if (form != NULL && form->GetFormType() == RE::ENUM_FORM_ID::kMSWP) {
                            // yes: this mod applies a material swap
                            c.isSwap = true;
                            // there is an int value here: is it the color remap index?
                            break;
                        }
                    }
                }
                c.color = mod->colorRemappingIndex;
                logger::trace(std::format("  candidate: {:#010x} (swap={} color={})", mod->GetFormID(), c.isSwap, c.color));
                out.push_back(c);
            }

            logger::trace(std::format("< collect_candidates_for_armor: {} candidates", out.size()));
            return out;
        }

        std::vector<OMOD*> pick_mods(const std::vector<Cand>& pool)
        {
            std::vector<OMOD*> picked;
            if (pool.empty() || opts_.maxAttachmentsPerArmor == 0) return picked;

            std::uniform_real_distribution<float> u01(0.0f, 1.0f);

            auto score = [&](const Cand& c) -> float {
                float s = 0.1f;
                if (opts_.favorMaterialSwaps && c.isSwap) s += 0.9f;
                if (opts_.actorLevel > 1) {
                    float n = std::min<std::uint32_t>(opts_.actorLevel, 50) * 0.002f; // up to +0.1
                    s += u01(rng_) * n;
                }
                if (opts_.enforceStyle) {
                    if (!styleSeeded_) {
                        s += (c.isSwap && c.color >= 0.0f) ? 0.5f : 0.0f;
                    }
                    else if (targetColor_ >= 0.0f && c.color >= 0.0f) {
                        float d = std::abs(targetColor_ - c.color);
                        s += std::max(0.0f, 1.0f - d);
                    }
                }
                s += u01(rng_) * 0.05f;
                return s;
                };

            std::vector<const Cand*> ranked;
            ranked.reserve(pool.size());
            for (auto& c : pool) ranked.push_back(&c);
            std::sort(ranked.begin(), ranked.end(), [&](const Cand* a, const Cand* b) {
                return score(*a) > score(*b);
                });

            std::unordered_set<OMOD*> seen;
            std::unordered_set<uint16_t> seen_attachment_points;
            for (auto* c : ranked) {
                if (picked.size() >= opts_.maxAttachmentsPerArmor) break;
                if (!c->mod || seen.count(c->mod)) continue;
                // don't pick another mod using the same attachment point
                if (seen_attachment_points.count(c->mod->attachPoint.keywordIndex)) continue;

                if (opts_.enforceStyle && !styleSeeded_ && c->color >= 0.0f) {
                    targetColor_ = c->color;
                    styleSeeded_ = true;
                }

                picked.push_back(c->mod);
                seen.insert(c->mod);
                seen_attachment_points.insert(c->mod->attachPoint.keywordIndex);
            }

            return picked;
        }

    private:
        Options        opts_;
        std::mt19937   rng_;
        bool           styleSeeded_ = false;
        float          targetColor_ = -1.0f;
    };
} // namespace EquipOMOD
