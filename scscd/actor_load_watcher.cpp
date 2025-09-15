#include "scscd.h"

ArmorIndex* ActorLoadWatcher::ARMORS = NULL;
ArmorIndex::SamplerConfig* ActorLoadWatcher::ARMORS_CONFIG = NULL;

void F4SEAPI ActorLoadWatcher::serialize(const F4SE::SerializationInterface* intfc)
{
    if (!intfc) return;

    if (!intfc->OpenRecord(Serialization::kTag, Serialization::kVersion)) {
        logger::error("could not open record for serialization!");
        return;
    }

    const std::uint32_t count = static_cast<std::uint32_t>(GetSingleton()->seenSet.size());
    if (!intfc->WriteRecordData(&count, sizeof(count))) return;
    for (std::pair<uint32_t, std::vector<PersistenceEntry>> pair : GetSingleton()->seenSet) {
        uint32_t actorID = pair.first;
        size_t size = pair.second.size();
        (void)intfc->WriteRecordData(&actorID, sizeof(std::uint32_t));
        (void)intfc->WriteRecordData(&size, sizeof(std::size_t));
        for (PersistenceEntry& entry : pair.second) {
            (void)intfc->WriteRecordData(&entry.armorFormID, sizeof(uint32_t));
            (void)intfc->WriteRecordData(&entry.omodFormID, sizeof(uint32_t));
        }
    }

    logger::info(std::format("Serialized {} seen-refs.", count));
}

void F4SEAPI ActorLoadWatcher::deserialize(const F4SE::SerializationInterface* intfc)
{
    logger::trace("deserializing...");
    if (!intfc) {
        logger::error("deserialize cannot proceed: no interface");
        return;
    }

    std::uint32_t type = 0;
    std::uint32_t version = 0;
    std::uint32_t length = 0;

    GetSingleton()->seenSet.clear();
    while (intfc->GetNextRecordInfo(type, version, length)) {
        if (type != Serialization::kTag) {
            logger::error(std::format("record tag mismatch (expected {}, got {})", Serialization::kTag, type));
            // skip record
            if (length > 0) {
                std::vector<std::byte> discard(length);
                (void)intfc->ReadRecordData(discard.data(), length);
            }
            continue;
        }

        // migration
        bool deserializeOmodData = true;
        if (version == 2 && Serialization::kVersion == 3) {
            version = 3;
            deserializeOmodData = false;
        }

        if (version != Serialization::kVersion) {
            logger::warn(std::format("record version mismatch (expected {}, got {})", Serialization::kVersion, version));
            // skip record
            if (length > 0) {
                std::vector<std::byte> discard(length);
                (void)intfc->ReadRecordData(discard.data(), length);
            }
            continue;
        }

        // Read our vector back
        std::uint32_t count = 0;
        if (!intfc->ReadRecordData(&count, sizeof(count))) {
            logger::error("could not deserialize seen set count");
            return;
        }

        logger::debug(std::format("Deserializing {} entries", count));
        for (uint32_t i = 0; i < count; i++) {
            uint32_t actorID;
            size_t numArmors;
            bool invalidated = false; // if true, the we will 'forget' about the whole actor.
            //uint32_t* armorIDs = NULL;
            (void)intfc->ReadRecordData(&actorID, sizeof(uint32_t));
            (void)intfc->ReadRecordData(&numArmors, sizeof(size_t));
            std::optional<uint32_t> resolvedActorID = intfc->ResolveFormID(actorID);
            logger::debug(std::format(" ... {} armors", numArmors));
            for (uint32_t armorIdx = 0; armorIdx < numArmors; armorIdx++) {
                uint32_t armorID, omodID = 0;
                // read
                (void)intfc->ReadRecordData(&armorID, sizeof(uint32_t));
                // if omod data is present, read it; else it defaults to 0 (no omod)
                // which is what it was before
                if (deserializeOmodData) (void)intfc->ReadRecordData(&omodID, sizeof(uint32_t));
                // parse
                std::optional<uint32_t> resolvedArmorID = intfc->ResolveFormID(armorID);
                if (resolvedActorID.has_value() && resolvedArmorID.has_value()) {
                    uint32_t actorFormID = resolvedActorID.value();
                    if (omodID != 0) {
                        std::optional<uint32_t> resolvedOmodID = intfc->ResolveFormID(omodID);
                        if (resolvedOmodID.has_value()) omodID = resolvedOmodID.value();
                    }
                    PersistenceEntry pe{ resolvedArmorID.value(), omodID };
                    RE::TESForm* form = RE::TESForm::GetFormByID(pe.armorFormID);
                    if (!form || form->GetFormType() != RE::ENUM_FORM_ID::kARMO) {
                        logger::warn(std::format("  could not deserialize seen-set entry: is not an armo (did plugins change?). This wardrobe is invalidated."));
                        invalidated = true;
                    }
                    else {
                        if (pe.omodFormID != 0) {
                            form = RE::TESForm::GetFormByID(pe.armorFormID);
                            if (!form || form->GetFormType() != RE::ENUM_FORM_ID::kOMOD) {
                                // wardrobe is not invalidated here because re-equip doesn't actually use the omod.
                                // but if we need omod for any reason, it will show as 'no omod' for this armor.
                                logger::warn(std::format("  could not deserialize omod entry; we can recover this wardrobe if it's not already invalidated, but omod data will be lost"));
                                pe.omodFormID = 0;
                            }
                        }
                        logger::debug(std::format("  deserialized seen-set persistence entry into actor={:#010x}, armor={:#010x}, omod={:#010x}", actorFormID, pe.armorFormID, pe.omodFormID));
                        GetSingleton()->seenSet[actorFormID].push_back(pe);
                    }
                }
                else {
                    logger::warn(std::format("  could not resolve deserialized actor={:#010x}, armor={:#010x}, omod={:#010x} (did plugin order change?)", actorID, armorID, omodID));
                }
            }
            if (invalidated) {
                if (resolvedActorID.has_value()) {
                    uint32_t actorFormID = resolvedActorID.value();
                    GetSingleton()->seenSet.erase(actorFormID);
                }
                logger::warn(std::format("invalidated actor (failed to deserialize)"));
            }
            else {
                logger::debug(std::format("deserialized actor {:#010x} set into {} armors", actorID, numArmors));
            }
        }

        logger::debug(std::format("Deserialized {} entries", count));
        logger::info(std::format("Deserialized {} entries into {} seen-refs.", count, GetSingleton()->seenSet.size()));
    }
}

void ActorLoadWatcher::Register()
{
    if (_registered) return;
    if (auto* ser = F4SE::GetSerializationInterface()) {
        ser->SetUniqueID(Serialization::kTag);                // identifies your plugin to F4SE
        ser->SetSaveCallback(serialize);
        ser->SetLoadCallback(deserialize);
        ser->SetRevertCallback(revert);
    }
    logger::trace("> ActorLoadWatcher#Register()");
    // object loaded doesn't appear to fire for newly spawned/attached actors
    auto* src =
#ifdef F4OG
        RE::ObjectLoadedEventSource::GetSingleton()
#else // NG
        RE::TESObjectLoadedEvent::GetEventSource()
#endif
        ;
    if (src) {
        src->RegisterSink(this);
        _registered = true;
    }
}

void ActorLoadWatcher::OnActorLoaded(RE::Actor* actor)
{
    if (!actor) return;
    std::unordered_map<uint32_t, std::vector<PersistenceEntry>>& seenSet = GetSingleton()->seenSet;
    uint32_t actorFormID = actor->GetFormID();
    const char* actorFullName = GetDisplayFullName(actor);
    RE::TESNPC* npc = actor->GetNPC();
    uint32_t npcFormID = npc ? npc->GetFormID() : 0;

    if (npc == NULL) {
        logger::warn(std::format("nothing to do: NPC is NULL for actor {:#010x}", actorFormID));
        return;
    }

    if (npc->GetFormID() == 0xA7D34 /* player spouse male */
        || npc->GetFormID() == 0xA7D35 /* player spouse femaile */) {
        logger::debug("not processing player spouses at game start");
        return;
    }

    if (seenSet.contains(actorFormID)) {
        logger::debug(std::format("actor is already in seen-set: {:#010x} name={} (npc: {:#010x})", actorFormID, actorFullName, npcFormID));
        // re-equip the actor's wardrobe, as they tend to disrobe between loads.
        // Note that if any item is not in their inventory, we assume the player
        // removed it on purpose, and abort the whole operation or else we'd risk
        // overriding the player's choice.
        std::vector<WardrobeEntry> armors;
        for (PersistenceEntry& pe : seenSet[actorFormID]) {
            RE::TESForm* form = RE::TESForm::GetFormByID(pe.armorFormID);
            if (form && form->GetFormType() == RE::ENUM_FORM_ID::kARMO) {
                WardrobeEntry we{
                    form->As<RE::TESObjectARMO>(),
                    pe.omodFormID == 0 ? NULL : RE::TESForm::GetFormByID(pe.omodFormID)->As<RE::BGSMod::Attachment::Mod>()
                };
                uint32_t omodFormID = we.omod ? we.omod->GetFormID() : 0;
                logger::debug(std::format("Seen-set actor={:#010x} persistence entry has been materialized into armor={:#010x}, mod={:#010x}", actor->GetFormID(), we.armor->GetFormID(), omodFormID));
                armors.push_back(we);
            }
        }
        // check that all wardrobe items appear in the npc's inventory.
        for (size_t i = 0; i < armors.size(); ++i) {
            bool itemMissing = true;
            actor->inventoryList->ForEachStack(
                [&](RE::BGSInventoryItem& item) {
                    // We purposely don't check the omods in case that information was damaged after game restore (e.g.
                    // plugins changed).
                    return item.object == armors[i].armor;
                },
                [&](RE::BGSInventoryItem& item, RE::BGSInventoryItem::Stack& stack) {
                    // if we're in this callback at all, the item was found. We can
                    // both stop iterating and flag that the item is not missing.
                    itemMissing = false;
                    return false;
                }
            );
            if (itemMissing) {
                logger::warn(std::format("armor item {:#010x} was missing, assuming the player does not want us to equip this actor", armors[i].armor->GetFormID()));
                return;
            }
        }
                    
        equipWardrobe(actor, armors);
        return;
    }
    logger::debug(std::format("actor loaded: {:#010x} name={} (npc: {:#010x})", actorFormID, actorFullName, npcFormID));

    if (actor->IsPlayerRef()) {
        logger::debug("actor: is player, doing nothing");
        return;
    }

    // ensures the seen-set has an entry, but also, prevents us from growing it unbounded
    // on the off chance it has anything in it (that shouldn't happen though)
    seenSet[actorFormID].clear();

    const uint32_t changeOutfitChance = actorIsFemale(actor)
                                      ? ARMORS_CONFIG->changeOutfitChanceF
                                      : ARMORS_CONFIG->changeOutfitChanceM;
    if ((uint32_t) (rand() % 100) > changeOutfitChance) {
        logger::debug("randomly skipping this actor");
        return;
    }

    std::vector<RE::TESObjectARMO*> sample = ARMORS->sample(actor, *ARMORS_CONFIG);
    if (sample.size() == 0) {
        // At this point if any valid wardrobe existed it should have been found (no RNG).
        // So, if no wardrobe was found, some error occurred - probably a lack of clothing
        // mods to suit this actor. Let's delete the actor from the seen-set, so that it
        // may be processed again in the future, in case the user adds the missing mods.
        logger::debug("no suitable wardrobe available for this actor - will retry on next load");
        seenSet.erase(actorFormID);
        return;
    }

    // Sample omods for each armor entry.
    std::vector<WardrobeEntry> wardrobe;
    RE::BGSMod::Attachment::Mod* lastMod = NULL;
    for (RE::TESObjectARMO* armor : sample) {
        RE::BGSMod::Attachment::Mod* mod = ActorLoadWatcher::ARMORS->sampleOmod(armor, ActorLoadWatcher::ARMORS_CONFIG->proximityBias, lastMod, ActorLoadWatcher::ARMORS_CONFIG->allowNSFWChoices);
        lastMod = mod;
        wardrobe.push_back(WardrobeEntry{ armor, mod });
        seenSet[actorFormID].push_back(PersistenceEntry{ armor->GetFormID(), mod ? mod->GetFormID() : 0 });
    }

    npc->defOutfit = nullptr;
    npc->sleepOutfit = nullptr;
    // Force reevaluation: unequip everything that came from outfit, then equip armors
    // I considered unequipping every slot, but in vanilla only the body slot is really
    // used for outfits.
#ifdef F4OG
    actor->UnequipArmorFromSlot(static_cast<RE::BIPED_OBJECT>(3), false);
#else // NG
    actor->UnequipArmorFromSlot(RE::BIPED_OBJECT::kBody, false);
#endif
    equipWardrobe(actor, wardrobe);
    logger::info(std::format("processed actor {:#010x} name={} (npc: {:#010x}); {} armors equipped", actorFormID, actorFullName, npcFormID, wardrobe.size()));
}

void equipArmorOmodPair(RE::Actor* actor, WardrobeEntry &wardrobe, bool applyNow) {
    if (auto* mgr = RE::ActorEquipManager::GetSingleton()) {
        RE::TESObjectARMO* armor = wardrobe.armor;
        RE::BGSMod::Attachment::Mod* mod = wardrobe.omod;
        if (mod != NULL) {
            logger::trace("  adding armor with omod to actor's inventory");
            RE::BSTSmartPointer<RE::TBO_InstanceData> instData;

            auto* instExtra = new RE::BGSObjectInstanceExtra();      // default ctor is available
            instExtra->AddMod(*mod, /*attachIndex*/ 1, /*rank*/ 0, /*removeInvalidMods*/true);
            armor->ApplyMods(instData, instExtra);

            auto* xInst = new RE::ExtraInstanceData(armor, instData);
            auto xlist = RE::BSTSmartPointer<RE::ExtraDataList>(new RE::ExtraDataList());
            xlist->AddExtra(xInst);
            xlist->AddExtra(instExtra);

            actor->AddObjectToContainer(armor, xlist, 1, nullptr, RE::ITEM_REMOVE_REASON::kNone);
            RE::BGSObjectInstance inst(armor, nullptr);// or instData.get()?
            mgr->EquipObject(actor, inst, /*stackID*/0, /*number*/1, /*slot*/nullptr, /*queueEquip*/true, /*force*/true, /*playSound*/false,
                /*applyNow*/applyNow, /*locked*/true);
            logger::info(std::format("equipped armor {:#010x} to actor {:#010x} with omod {:#010x}", armor->GetFormID(), actor->GetFormID(), mod->GetFormID()));
        }
        else {
            RE::BGSObjectInstance inst(armor, nullptr);
            logger::trace("  adding base armor to actor's inventory");
            actor->AddObjectToContainer(
                armor,
                nullptr,
                /*count*/ 1,
                /*fromContainer*/ nullptr,
                RE::ITEM_REMOVE_REASON::kNone
            );
            (void)mgr->EquipObject(
                actor, inst,
                /*stackID*/0,
                /*number*/1,
                /*slot*/nullptr,     // usually nullptr
                /*queueEquip*/false,      // synchronous, so our stack 'extra' has already produced instData
                /*forceEquip*/true,
                /*playSounds*/false,
                /*applyNow*/applyNow,  // trigger a single biped rebuild on the last piece
                /*locked*/true);
            logger::info(std::format("equipped armor {:#010x} to actor {:#010x}", armor->GetFormID(), actor->GetFormID()));
        }
    }
    else {
        logger::warn("No equip mgr! This shouldn't happen... but there's nothing I can do about it.");
    }
}

void ActorLoadWatcher::equipWardrobe(RE::Actor* actor, std::vector<WardrobeEntry> wardrobe, bool isRetry, int attemptsRemaining) {
    logger::trace(std::format("equipWardrobe isRetry={} attemptsRemaining={}", isRetry, attemptsRemaining));

    // If isRetry is true, we need to query the actor and see which item(s) failed to equip. Otherwise
    // we continue on.
    // Note: matswap is implemented here. Since we have to check if an item is equipped AND we need the
    // item to be equipped to generate instance data, we can do both things at once; and in fact, we can
    // only proceed with matswap if we can confirm the item was indeed equipped.
    if (isRetry) {
        std::vector<WardrobeEntry> retry;
        for (auto& entry : wardrobe) {
            if (!isEquipped(actor, entry.armor)) {
                logger::trace(std::format("isEquipped was false for {:#010x}", entry.armor->GetFormID()));
                retry.push_back(entry);
            }
        }
        wardrobe = retry;
    }

    if (wardrobe.size() == 0) {
        logger::debug(std::format("wardrobe has been fully equipped on actor {:#010x}", actor->GetFormID()));
        return;
    }

    if (attemptsRemaining == 0) {
        logger::warn(std::format("no more attempts remaining to equip actor {:#010x}; giving up with {} armors remaining", actor->GetFormID(), wardrobe.size()));
        return;
    }

    auto* mgr = RE::ActorEquipManager::GetSingleton();
    if (!mgr) {
        logger::error("no equipment manager! cannot continue");
        return;
    }

    if (!actor->inventoryList) {
        // assume if there's no inventoryList the actor isn't fully initialized yet.
        // We'll catch them in the next onLoad (seenSet will help us out).
        logger::debug("actor has no inventoryList; aborting until next actor load event");
        return;
    }

    // Header for ForEachStack implies that we need a read lock for the operations below. Not sure the best way to do that but
    // GetAllForms() returns us one.
    const auto& [map, lock] = RE::TESForm::GetAllForms();
    RE::BSAutoReadLock l{ lock };

    size_t done = 0;
    for (size_t i = 0; i < wardrobe.size(); ++i) {
        bool foundExisting = false;
        bool applyNow = i == wardrobe.size() - 1;
        actor->inventoryList->ForEachStack(
            [&](RE::BGSInventoryItem& item) {
                return item.object == wardrobe[i].armor; // restrict to this base form
            },
            [&](RE::BGSInventoryItem& item, RE::BGSInventoryItem::Stack& stack) {
                foundExisting = true; // if we're here, a match was found
                if (stack.IsEquipped())
                    return false; // already equipped, nothing to do; stop iterating
                RE::BGSObjectInstance inst(wardrobe[i].armor, nullptr);
                (void)mgr->EquipObject(
                    actor, inst,
                    /*stackID*/0, /*number*/1, /*slot*/nullptr, /*queueEquip*/false, /*forceEquip*/true,
                    /*playSounds*/false, /*applyNow*/applyNow,  // trigger a single biped rebuild on the last piece
                    /*locked*/true);
                return false; // equip done, nothing more to do; stop iterating
            }
        );
        // Only if the base armor doesn't already exist (this is usually the case),
        // generate a new one with selected omod.
        if (!foundExisting)
            equipArmorOmodPair(actor, wardrobe[i], applyNow);
        done++;
    }
    logger::debug(std::format("{} armors equipped to actor {:#010x}", done, actor->GetFormID()));
    // wait 2 frames and then verify success, re-equipping as needed
    NextFrames("equip wardrobe confirm/retry", [actor, wardrobe, attemptsRemaining]() {
        ActorLoadWatcher::equipWardrobe(actor, wardrobe, /*isRetry=*/true, attemptsRemaining - 1);
    }, 30);
}
