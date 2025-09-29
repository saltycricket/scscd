#pragma once

#include <cstdint>
#include <functional>

#include "scscd.h"
#include "armor_index.h"
#include <F4SE/API.h>
#include <F4SE/Interfaces.h>

namespace Serialization {
    static constexpr std::uint32_t kTag = 'SEEN';
    static constexpr std::uint32_t kVersion = 3;
}

struct WardrobeEntry {
    RE::TESObjectARMO* armor{ NULL };
    RE::BGSMod::Attachment::Mod* omod{ NULL };
};

struct PersistenceEntry {
    uint32_t armorFormID, omodFormID;
};

inline bool IsLoadedActor(RE::TESObjectREFR* ref)
{
    if (!ref) return false;
    // Type check: actors are ACHR / FormType::ActorCharacter in FO4 headers
    if (!ref->Is(RE::ENUM_FORM_ID::kACHR)) return false;

    // Loaded in memory and 3D present are both nice to filter:
    if (!ref->Get3D()) return false;            // ensures currently loaded (has 3D)
    // You can add more checks here (e.g., in same worldspace, distance to player, etc.)

    return true;
}

static bool isEquipped(RE::Actor* actor, RE::TESObjectARMO* armor) {
    logger::trace("> isEquipped");
    if (!actor || !armor) {
        logger::trace("< isEquipped armor or actor is null");
        return false;
    }

    auto inv = actor->inventoryList;
    if (!inv) {
        logger::trace("< isEquipped inventoryList is null");
        return false;
    }

    bool isEquipped = false;
    inv->ForEachStack(
        [&](RE::BGSInventoryItem& item) {
            return item.object == armor; // restrict to this base form
        },
        [&](RE::BGSInventoryItem& /*item*/, RE::BGSInventoryItem::Stack& stack) {
            isEquipped = stack.IsEquipped();
            return false;
        }
    );

    logger::trace(std::format("< isEquipped result={}", isEquipped));
    return isEquipped;

    //auto  mask = armo->bipedModelData.bipedObjectSlots;
    //RE::BGSEquipIndex idx;
    //for (uint32_t i = 0; i < 64; i++) {
    //    if (armo->FillsBipedSlot(i)) {
    //        idx.index = i;
    //        break;
    //    }
    //}
    //RE::BGSObjectInstance inst(nullptr, nullptr);
    //const auto* filled = actor->GetEquippedItem(&inst, idx);

    ///* Apparently this doesn't work as of 20250903 and object is always NULL. It leads to a redundant equip but that should be harmless (if a little less efficient). */
    ////logger::info(std::format("isEquipped {:#010x} ~= {:#010x}", armo->GetFormID(), inst.object ? inst.object->GetFormID() : 0));
    //return (filled && inst.object == armo);
}

class ActorLoadWatcher final : public RE::BSTEventSink<RE::TESObjectLoadedEvent>
{
public:
    static std::unordered_set<uint32_t> exclusionList;
    static ArmorIndex* ARMORS;
    static ArmorIndex::SamplerConfig* ARMORS_CONFIG;
    bool _registered{ false };

    static void configure(ArmorIndex* index, ArmorIndex::SamplerConfig* config) {
        ARMORS = index;
        ARMORS_CONFIG = config;
    }

    static ActorLoadWatcher* GetSingleton()
    {
        static ActorLoadWatcher instance;
        return std::addressof(instance);
    }

    static void F4SEAPI serialize(const F4SE::SerializationInterface* intfc);

    static void F4SEAPI deserialize(const F4SE::SerializationInterface* intfc);

    static void F4SEAPI revert(const F4SE::SerializationInterface* /*unused*/) {
        GetSingleton()->seenSet.clear();
    }

    void Register();

    static void OnActorLoaded(RE::Actor* actor);

private:
    std::unordered_map<uint32_t, std::vector<PersistenceEntry>> seenSet;

    static void equipWardrobe(RE::Actor* actor, std::vector<WardrobeEntry> wardrobe);

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESObjectLoadedEvent& a_event,
        RE::BSTEventSource<RE::TESObjectLoadedEvent>* /*a_source*/) override
    {
        const uint32_t id =
#ifdef F4OG
            a_event.formId
#else
            a_event.formID
#endif
            ;
        // possibly reload settings if necessary
        if (ARMORS_CONFIG && ARMORS_CONFIG->haveSettingsChanged()) {
            logger::debug("reloading settings");
            ARMORS_CONFIG->reload();
        }

        if (auto* form = RE::TESForm::GetFormByID(id)) {
            if (auto* ref = form->As<RE::TESObjectREFR>()) {
                if (auto* actor = ref->As<RE::Actor>()) {
                    OnActorLoaded(actor);
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};
