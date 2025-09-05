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
    for (std::pair<uint32_t, std::vector<uint32_t>> pair : GetSingleton()->seenSet) {
        uint32_t actorID = pair.first;
        size_t size = pair.second.size();
        (void)intfc->WriteRecordData(&actorID, sizeof(std::uint32_t));
        (void)intfc->WriteRecordData(&size, sizeof(std::size_t));
        (void)intfc->WriteRecordData(pair.second.data(), (uint32_t)(sizeof(std::uint32_t) * size));
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
            uint32_t* armorIDs = NULL;
            (void)intfc->ReadRecordData(&actorID, sizeof(uint32_t));
            (void)intfc->ReadRecordData(&numArmors, sizeof(size_t));
            logger::debug(std::format(" ... {} armors", numArmors));
            armorIDs = new uint32_t[numArmors];
            (void)intfc->ReadRecordData(armorIDs, (uint32_t)(sizeof(uint32_t) * numArmors));
            std::optional<uint32_t> resolvedActorID = intfc->ResolveFormID(actorID);
            if (resolvedActorID.has_value()) {
                for (size_t idx = 0; idx < numArmors; idx++) {
                    std::optional<uint32_t> resolvedArmorID = intfc->ResolveFormID(armorIDs[idx]);
                    if (resolvedArmorID.has_value())
                        GetSingleton()->seenSet[resolvedActorID.value()].push_back(resolvedArmorID.value());
                }
            }
            delete[] armorIDs;
            logger::debug(std::format("deserialized actor {:#010x} set into {} armors", actorID, numArmors));
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
    if (auto* src = RE::TESObjectLoadedEvent::GetEventSource()) {
        src->RegisterSink(this);
        _registered = true;
    }
}

void ActorLoadWatcher::OnActorLoaded(RE::Actor* actor)
{
    if (!actor) return;
    std::unordered_map<uint32_t, std::vector<uint32_t>>& seenSet = GetSingleton()->seenSet;
    uint32_t actorFormID = actor->GetFormID();
    const char* actorFullName = actor->GetDisplayFullName();
    RE::TESNPC* npc = actor->GetNPC();
    uint32_t npcFormID = npc ? npc->GetFormID() : 0;

    if (seenSet.contains(actorFormID)) {
        logger::debug(std::format("actor is already in seen-set: {:#010x} name={} (npc: {:#010x})", actorFormID, actorFullName, npcFormID));
        // re-equip the actor's wardrobe, as they tend to disrobe between loads.
        // Note we don't check their inventory here. The theory is if the player
        // removed an item it should stay gone; attempting to equip the missing
        // item SHOULD fail silently, producing expected behavior.
        std::vector<RE::TESObjectARMO*> armors;
        for (uint32_t formID : seenSet[actorFormID])
            armors.push_back(RE::TESForm::GetFormByID(formID)->As<RE::TESObjectARMO>());
        equipWardrobe(actor, armors, true);
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

    if (rand() % 100 > ARMORS_CONFIG->changeOutfitChance) {
        logger::debug("randomly skipping this actor");
        return;
    }

    std::vector<RE::TESObjectARMO*> wardrobe = ARMORS->sample(actor, *ARMORS_CONFIG);
    if (wardrobe.size() == 0) {
        // At this point if any valid wardrobe existed it should have been found (no RNG).
        // So, if no wardrobe was found, some error occurred - probably a lack of clothing
        // mods to suit this actor. Let's delete the actor from the seen-set, so that it
        // may be processed again in the future, in case the user adds the missing mods.
        logger::debug("no suitable wardrobe available for this actor - will retry on next load");
        seenSet.erase(actorFormID);
        return;
    }

    /* See the wall of text in armor_equip_random.h for why this isn't in use. */
    //EquipOMOD::Options opts;
    //opts.maxAttachmentsPerArmor = 2;
    //opts.favorMaterialSwaps = true;
    //opts.enforceStyle = true;
    //opts.actorLevel = actor ? actor->GetLevel() : 1;
    //EquipOMOD::Service svc(opts);
    //std::size_t done = svc.EquipArmorsWithRandomMods(actor, wardrobe);

    /* Fallback from above - attach un-modded armors. */
    npc->defOutfit = nullptr;
    npc->sleepOutfit = nullptr;
    // Force reevaluation: unequip everything that came from outfit, then equip armors
    // I considered unequipping every slot, but in vanilla only the body slot is really
    // used for outfits.
    actor->UnequipArmorFromSlot(RE::BIPED_OBJECT::kBody, false);
    // Add all items to actor's inventory. It would be tempting to add & equip in the
    // same pass and this USUALLY works but sometimes NPCs are coming up naked so we
    // need to separate equip from add, so that we can retry the equip later if it fails
    // without adding a second copy.
    if (auto* mgr = RE::ActorEquipManager::GetSingleton()) {
        for (RE::TESObjectARMO* armor : wardrobe) {
            seenSet[actorFormID].push_back(armor->GetFormID());
            actor->AddObjectToContainer(
                armor,
                /*extras list*/ nullptr,
                /*count*/ 1,
                /*fromContainer*/ nullptr,
                RE::ITEM_REMOVE_REASON::kNone
            );
        }
    }
    equipWardrobe(actor, wardrobe);
    logger::info(std::format("processed actor {:#010x} name={} (npc: {:#010x}); {} armors equipped", actorFormID, actorFullName, npcFormID, wardrobe.size()));
}

void ActorLoadWatcher::equipWardrobe(RE::Actor* actor, std::vector<RE::TESObjectARMO*> wardrobe, bool isRetry, int attemptsRemaining) {
    // If isRetry is true, we need to query the actor and see which item(s) failed to equip. Otherwise
    // we continue on.
    if (isRetry) {
        std::vector<RE::TESObjectARMO*> retry;
        for (RE::TESObjectARMO* armo : wardrobe) {
            if (!isEquipped(actor, armo)) {
                retry.push_back(armo);
            }
        }
        wardrobe = retry;
    }

    if (wardrobe.size() == 0) {
        logger::debug(std::format("wardrobe has been fully equipped on actor {:#010x}", actor->GetFormID()));
        return;
    }

    if (attemptsRemaining == 0) {
        // this is actually expected due to isEquipped() always failing.
        // Uncomment warning if we can get isEquipped() to work properly.
        // Net result of isEquipped() not working is that we will always attempt 3 times (default attemptsRemaining count),
        // even if it worked; but as we're equipping an already-equipped armor 3x over 60 frames, it should be harmless,
        // as that duration will pass long before anyone interacts with the NPC to experience equip issues.
        //logger::warn(std::format("no more attempts remaining to equip actor {:#010x}; giving up with {} armors remaining", actor->GetFormID(), wardrobe.size()));
        return;
    }

    size_t done = 0;
    if (auto* mgr = RE::ActorEquipManager::GetSingleton()) {
        for (size_t i = 0; i < wardrobe.size(); ++i) {
            RE::TESObjectARMO* armor = wardrobe[i];
            RE::BGSObjectInstance inst(armor, nullptr);
            (void)mgr->EquipObject(
                actor, inst,
                /*stackID*/0,
                /*number*/1,
                /*slot*/nullptr,     // usually nullptr
                /*queueEquip*/false,      // synchronous, so our stack 'extra' has already produced instData
                /*forceEquip*/true,
                /*playSounds*/false,
                /*applyNow*/i == wardrobe.size() - 1,  // trigger a single biped rebuild on the last piece
                /*locked*/true);
            logger::debug(std::format("equipped armor {:#010x} to actor {:#010x}", armor->GetFormID(), actor->GetFormID()));
            done++;
        }
        logger::debug(std::format("{} armors equipped to actor {:#010x}", done, actor->GetFormID()));
        // wait 2 frames and then verify success, re-equipping as needed
        NextFrames([actor, wardrobe, attemptsRemaining]() {
            equipWardrobe(actor, wardrobe, /*isRetry=*/true, attemptsRemaining - 1);
            }, 30);
    }
    else {
        logger::warn("No equip mgr! This shouldn't happen... but there's nothing I can do about it.");
    }
}
