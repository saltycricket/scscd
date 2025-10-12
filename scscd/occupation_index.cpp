#include "scscd.h"
#include <random>

bool OccupationIndex::put(uint32_t form, Occupation o) {
	registry[form].push_back(o);
	return true;
}

Occupation OccupationIndex::sample(RE::Actor* actor) {
	// No matter how many occupations match an actor,
	// only one can be chosen. Try to pick one at random in
	// priority order (character first, then faction, then class as a last resort).
	// This allows character assignments to act as overrides, and since class is often wrong, makes
	// class a last resort.

	RE::TESClass* klass = NULL;
	RE::TESNPC* npc = actor->GetNPC();
	if (npc == NULL)
		logger::trace("no NPC");
	else {
		logger::trace(std::format("npc form={:#010x} name={}", npc->GetFormID(), npc->GetFullName()));
		klass = npc->cl;
	}
	if (klass == NULL)
		logger::trace("no klass");
	else
		logger::trace(std::format("klass form={:#010x} name={}", klass->GetFormID(), klass->GetFullName()));
	// FIXME Does doing this imply exclusionList should be a member of OccupationIndex?
	std::unordered_set<uint32_t>& exclusionList = ActorLoadWatcher::exclusionList;
	if (exclusionList.contains(npc->GetFormID())
		|| (klass && exclusionList.contains(klass->GetFormID()))) {
		logger::info(std::format("not processing excluded NPC {:#010x} (refr {:#010x})", npc->GetFormID(), actor->GetFormID()));
		return NO_OCCUPATION;
	}

	std::vector<Occupation>* npcOccups = npc && registry.contains(npc->GetFormID()) ? &registry[npc->GetFormID()] : NULL;
	size_t nNPCOccups = (npcOccups == NULL ? 0 : npcOccups->size());
	if (nNPCOccups != 0) {
		size_t choice = (size_t)(rand() % nNPCOccups);
		logger::debug(std::format("OccupationIndex::sample NPC actorID={:#010x} npcOccupsSize={} choiceIndex={}",
						actor->GetFormID(), nNPCOccups, choice));
		return (*npcOccups)[choice];
	}

	// faction is the only hairy one, we have to consider all factions
	// the NPC is a part of, that we also have registrants for.
	std::vector<Occupation> factionOccups;
	if (npc) {
		for (auto& fr : npc->factions) {
			if (fr.faction) {
				uint32_t factionID = fr.faction->GetFormID();
				// Exclusion list might include this faction. If it does, the NPC isn't a valid
				// target, no matter how many eligible factions it may belong to.
				if (exclusionList.contains(factionID)) return NO_OCCUPATION;
				bool included = registry.contains(factionID);
				logger::trace(std::format(": : npc inFaction={} formid={:#010x}", included, factionID/*, fr.faction->GetFullName() */));
				if (included) {
					std::vector<Occupation>* toAdd = &registry[factionID];
					factionOccups.insert(std::end(factionOccups), std::begin(*toAdd), std::end(*toAdd));
				}
			}
		}

		// dynamic factions
		//auto* ptr = actor->extraList.get();
		//if (ptr) {
		//	// we don't actually have a ExtraFactionChanges or equivalent class,
		//	// so we can't do this part of the lookup. We can explain it away as
		//	// "they were once in faction, they still have their clothes" OR
		//	// "they're new to the faction, and don't have a wardrobe yet".
		//	// Good enough for our needs.
		//	// Keeping this in case it becomes available in a future version.
		//	/*
		//	if (auto* changes = ptr->GetByType(RE::EXTRA_DATA_TYPE::kFactionChanges)) {
		//		for (auto& fr : changes->factions) {
		//			emplace_if_match(fr.faction);
		//		}
		//	}
		//	*/
		//}
	}
	if (factionOccups.size() != 0) {
		size_t choice = (size_t)(rand() % factionOccups.size());
		logger::debug(std::format("OccupationIndex::sample Faction actorID={:#010x} factionOccupsSize={} choiceIndex={}",
			actor->GetFormID(), factionOccups.size(), choice));
		return factionOccups[choice];
	}

	std::vector<Occupation>* classOccups = klass && registry.contains(klass->GetFormID()) ? &registry[klass->GetFormID()] : NULL;
	size_t nClassOccups = (classOccups == NULL ? 0 : classOccups->size());
	if (nClassOccups != 0) {
		size_t choice = (size_t)(rand() % nClassOccups);
		logger::debug(std::format("OccupationIndex::sample Class actorID={:#010x} classOccupsSize={} choiceIndex={}",
			actor->GetFormID(), nClassOccups, choice));
		return (*classOccups)[choice];
	}

	// no matches in index
	return NO_OCCUPATION;
}
