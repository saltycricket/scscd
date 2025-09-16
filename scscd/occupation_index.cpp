#include "scscd.h"
#include <random>

bool OccupationIndex::put(uint32_t form, Occupation o) {
	registry[form].push_back(o);
	return true;
}

Occupation OccupationIndex::sample(RE::Actor* actor) {
	// No matter how many occupations match an actor,
	// only one can be chosen. So instead of trying to
	// accumulate all possible occupations, just iterate
	// in a random order and return the first match.

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

	std::vector<Occupation>* classOccups = klass && registry.contains(klass->GetFormID()) ? &registry[klass->GetFormID()] : NULL;
	std::vector<Occupation>* npcOccups = npc && registry.contains(npc->GetFormID()) ? &registry[npc->GetFormID()] : NULL;
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

	// now we have all possible occups, with minimal copying.
	// just pick a number and bucket them by their sizes.
	size_t nClassOccups = (classOccups == NULL ? 0 : classOccups->size());
	size_t nNPCOccups = (npcOccups == NULL ? 0 : npcOccups->size());
	size_t total_count = nClassOccups + nNPCOccups + factionOccups.size();
	size_t choice = total_count == 0 ? 0 : (size_t)(rand() % total_count);
	logger::debug(std::format("OccupationIndex::sample formID={:#010x} classOccupsSize={}, npcOccupsSize={}, factionOccupsSize={}, totalOccupsSize={}, choiceIndex={}",
		actor->GetFormID(), nClassOccups, nNPCOccups, factionOccups.size(), total_count, choice));
	if (choice < nClassOccups)
		return (*classOccups)[choice];
	choice -= nClassOccups;
	if (choice < nNPCOccups)
		return (*npcOccups)[choice];
	choice -= nNPCOccups;
	if (choice < factionOccups.size())
		return factionOccups[choice];
	return NO_OCCUPATION;
}
