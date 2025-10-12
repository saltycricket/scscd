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

    /*
    * On game load from the main menu, we get an interesting sequence of events.
    * First we get kPreLoadGame, and then actors start getting loaded and our
    * ObjectLoaded event listener begins to fire. But the savegame hasn't finished
    * loading and F4SE deserialization happens at some unknown point along the way
    * (probably after the initial references are loaded). Finally, kPostLoadGame
    * fires when everything (including deserialization) has finished and the game
    * routine starts.
    * 
    * This sequence is problematic because when the initial ObjectLoaded events fire,
    * we haven't got a seenSet yet so all actors appear as never-seen. So even if
    * we have previously assigned an outfit (or player has made changes), we would
    * process them as if they were first loaded. Only after deserialization happens
    * can we realize we made a mistake and by then it's too late.
    *
    * NOTE: on quickload, this sequence doesn't occur. We get few or no loaded events
    * and go straight to deserialization. I assume this is because the engine merely
    * rolls back changesets and doesn't actually 'load' the actors. So, quickload
    * behaves fine as no out-of-sequence events are received.
    *
    * Let's try this: we'll suppress any outfit assignment during game load. We'll
    * keep a tally of ObjectLoaded events seen during suppression, and process them
    * all after suppression ends as by then we should have finished deserializing
    * and will be able to identify actors already in the seenSet.
    */
    bool suppressProcessing{ false };
    std::vector<uint32_t> suppressedActors;

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

    void Suppress() {
        logger::debug("Actor processing suppressed.");
        suppressProcessing = true;
        suppressedActors.clear();
    }

    void Unsuppress() {
        logger::debug(std::format("Actor processing suppressed; now processing {} deferred actors.", suppressedActors.size()));
        suppressProcessing = false;
        for (uint32_t formid : suppressedActors)
            OnActorLoaded(RE::TESForm::GetFormByID<RE::Actor>(formid));
        suppressedActors.clear();
    }

    void Register();

    void OnActorLoaded(RE::Actor* actor);

private:
    std::unordered_map<uint32_t, std::vector<PersistenceEntry>> seenSet;

    static void equipWardrobe(RE::Actor* actor, std::vector<WardrobeEntry> wardrobe);

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESObjectLoadedEvent& a_event,
        RE::BSTEventSource<RE::TESObjectLoadedEvent>* /*a_source*/) override
    {
        ActorLoadWatcher* watcher = GetSingleton();
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
                    watcher->OnActorLoaded(actor);
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};
