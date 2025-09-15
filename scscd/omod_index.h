#pragma once
#include <vector>
#include <unordered_map>
#include <span>
#include "_fallout.h"
#include "logger.h"

class OmodIndex
{
public:
    static OmodIndex& Instance()
    {
        static OmodIndex s;
        return s;
    }

    // Call this ONCE at startup after you've built a list of all forms (via your GetAllForms()).
    // Pass them in here to build a stable OMOD list / map for later queries.
    void BuildFromAllForms()
    {
        logger::trace("> OmodIndex::BuildFromAllForms()");
        mods_all_.clear();
        mods_for_armo_.clear();
        //omod_to_loose_.clear();
        const auto& [map, lock] = RE::TESForm::GetAllForms();
        if (!map) {
            logger::warn("BUG: could not get all forms");
            return;
        }
        RE::BSAutoReadLock l{ lock };
        for (auto &entry : *map) {
            uint32_t formID = entry.first;
            RE::TESForm* form = entry.second;
            if (!form) continue;
            if (form->GetFormType() != RE::ENUM_FORM_ID::kOMOD) continue; // restrict to OMODs

            auto* mod = static_cast<RE::BGSMod::Attachment::Mod*>(form);
            mods_all_.push_back(mod->GetFormID());

            // Prefilter: only keep mods whose targetFormType includes ARMO
            if (mod->targetFormType.get() == RE::ENUM_FORM_ID::kARMO)
                mods_for_armo_.push_back(formID);
        }
        logger::trace(std::format("< OmodIndex::BuildFromAllForms() mods_all={} mods_for_armo={}", mods_all_.size(), mods_for_armo_.size()));
    }

    // Return OMODs suitable for ARMO (prefiltered roughly; final legality will be
    // enforced by BGSObjectInstanceExtra::RemoveInvalidMods later)
    const std::vector<RE::BGSMod::Attachment::Mod*> ForArmor() { return buildForms(mods_for_armo_); }

    // If you want to expose 'all' (unfiltered), you can also provide:
    const std::vector<RE::BGSMod::Attachment::Mod*> All() { return buildForms(mods_all_); }

private:
    OmodIndex() = default;

    std::vector<RE::BGSMod::Attachment::Mod*> buildForms(std::vector<uint32_t>& forms) {
        std::vector<RE::BGSMod::Attachment::Mod*> out;
        for (uint32_t form : forms)
            out.push_back(RE::TESForm::GetFormByID(form)->As<RE::BGSMod::Attachment::Mod>());
        return out;
    }

    std::vector<uint32_t> mods_all_;
    std::vector<uint32_t> mods_for_armo_;
    //std::unordered_map<const RE::BGSMod::Attachment::Mod*, RE::TESObjectMISC*> omod_to_loose_;
};
