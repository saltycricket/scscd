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

class ActorLoadWatcher final : public /*RE::BSTEventSink<RE::BGSActorCellEvent>, */ RE::BSTEventSink<RE::TESObjectLoadedEvent>
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

    static void watchForSettingsChanges() {
#ifdef F4OG
        F4SE::GetTaskInterface()->AddTask(new SettingsChangeWatcherTask());
#else // NG
        F4SE::GetTaskInterface()->AddTaskPermanent(new SettingsChangeWatcherTask());
#endif
    }

private:
    std::unordered_map<uint32_t, std::vector<PersistenceEntry>> seenSet;
    static const int SETTINGS_POLL_FREQUENCY_MS = 5000; // poll every 30 seconds

    struct SettingsChangeWatcherTask : F4SE::ITaskDelegate
    {
        std::chrono::steady_clock::time_point last_poll_at;

        SettingsChangeWatcherTask() {
            this->last_poll_at = std::chrono::steady_clock::now();
        }

        SettingsChangeWatcherTask(std::chrono::steady_clock::time_point t) : last_poll_at(t) {
        }

        void Run() override {
            auto t = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t - this->last_poll_at).count();
            if (elapsed > SETTINGS_POLL_FREQUENCY_MS) {
                if (ARMORS_CONFIG && ARMORS_CONFIG->haveSettingsChanged()) {
                    logger::debug("reloading settings");
                    ARMORS_CONFIG->reload();
                }
                this->last_poll_at = t;
            }
#ifdef F4OG
            F4SE::GetTaskInterface()->AddTask(new SettingsChangeWatcherTask(last_poll_at));
#endif
        }
    };

    struct DeferFramesTask : F4SE::ITaskDelegate
    {
        int frames{ 0 };
        std::function<void()> fn;
        std::string desc;

        DeferFramesTask(int f, std::string desc, std::function<void()> ff) { frames = f; this->desc = desc;  fn = ff; }

        void Run() override
        {
            if (frames-- > 0) {
                #ifdef F4OG
                logger::trace(std::format("deferring task '{}' with {}", desc, frames));
                #endif
                F4SE::GetTaskInterface()->AddTask(new DeferFramesTask(frames, desc, std::move(fn)));   // run again next frame
            }
            else {
                if (fn) {
                    logger::trace(std::format("calling deferred fn: {}", desc));
                    fn();
                } else {
                    logger::warn(std::format("BUG: No deferred fn to call! Task description: {}", desc));
                }
            }
        }
    };

    static void NextFrames(std::string desc, std::function<void()> fn, int frames = 1)
    {
        auto* t = new DeferFramesTask(frames, desc, std::move(fn));
        F4SE::GetTaskInterface()->AddTask(t);
    }

    static void OnActorLoaded(RE::Actor* actor);
    //static void equipWardrobe(RE::Actor* actor,
    //    std::vector<RE::TESObjectARMO*> wardrobe);
    static void equipWardrobe(RE::Actor* actor,
                              std::vector<WardrobeEntry> wardrobe,
                              bool isRetry = false,
                              int attemptsRemaining = 3);

    void OnActorLoadedSoon(std::uint32_t actorRefID, int frames = 0)
    {
        if (frames == 0) {
            // default = stagger randomly over the next several frames.
            // We may see quite a lot of loads especially when a game is
            // restored, so ammortize performance costs over time.
            const int ammortize_frame_count = 5;
            frames = (rand() % ammortize_frame_count) + 1;
        }
        NextFrames("process actor loaded", [actorRefID] {
            if (auto* form = RE::TESForm::GetFormByID(actorRefID)) {
                if (auto* ref = form->As<RE::TESObjectREFR>()) {
                    if (auto* actor = ref->As<RE::Actor>()) {
                        OnActorLoaded(actor);
                    }
                }
            }
        }, frames);
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESObjectLoadedEvent& a_event,
        RE::BSTEventSource<RE::TESObjectLoadedEvent>* /*a_source*/) override
    {
#ifdef F4OG
        logger::trace(std::format("Processing object loaded event..."));
#endif // F4OG

        const uint32_t id =
#ifdef F4OG
            a_event.formId
#else
            a_event.formID
#endif
            ;

#ifdef F4OG
        logger::trace(std::format("... for form ID {:#010x}", id));
#endif // F4OG

        OnActorLoadedSoon(id);
        return RE::BSEventNotifyControl::kContinue;
    }

    //RE::BSEventNotifyControl ProcessEvent(
    //    const RE::BGSActorCellEvent& a_event,
    //    RE::BSTEventSource<RE::BGSActorCellEvent>* /*a_source*/) override
    //{
    //    const uint32_t id = a_event.actor->GetFormID();
    //    OnActorLoadedSoon(id);

    //    return RE::BSEventNotifyControl::kContinue;
    //}
};
