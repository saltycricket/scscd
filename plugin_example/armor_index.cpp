#include "scscd.h"
#include <random>

ArmorIndex::CacheHit ArmorIndex::cachedIndexLookup(bool nsfw, RE::TESRace* race, uint32_t sex, uint32_t occupation)
{
	logger::trace(std::format("> ArmorIndex::cachedIndexLookup nsfw={} race={:#010x} sex={:#010x} occup={:#010x}", nsfw, race->GetFormID(), sex, occupation));
	if (!race) return NULL;
	ArmorIndexKey id(race, sex, occupation, nsfw);
	// As the index is now fully built at startup, there doesn't appear to really be anything to cache.
	if (baseIndex.contains(id))
		return &baseIndex[id];
	//auto iter = baseIndex.find(id);
	//if (iter != baseIndex.end())
	//	return &iter->second;
	logger::trace("ArmorIndex::cachedIndexLookup : index not found");
	return NULL;
	/*
	// 1) Fast path: shared read of memo
	{
		std::shared_lock sl(cacheMutex);
		if (auto it = cache.find(id); it != cache.end()) {
			logger::trace("< ArmorIndex::cachedIndexLookup fast hit");
			return it->second;
		}
	}

	// 2) Acquire/create gate for this key
	logger::trace(": ArmorIndex::cachedIndexLookup miss");
	std::shared_ptr<Gate> gate;
	{
		std::lock_guard lg(gatesMutex);
		auto [it, inserted] = gates.emplace(id, std::make_shared<Gate>());
		gate = it->second;
		if (!inserted) {
			logger::trace(": ArmorIndex::cachedIndexLookup waiting");
			// Someone else is building this key. Wait for it to finish.
			std::unique_lock gl(gate->m);
			gate->cv.wait(gl, [&] { return gate->ready; });
			logger::trace("< ArmorIndex::cachedIndexLookup ready");
			return gate->cachedValue ? gate->cachedValue : kEmpty;
		}
	}

	// 3) We are the builder for this key: compute outside global locks
	logger::trace(": ArmorIndex::cachedIndexLookup building new entry");
	CacheHit hit = &baseIndex[id];
	cache.emplace(id, hit);
	*/
	// for every armor parent, build a combined index of all armors that target its key.
	// Disabled - actually, this happens during index, so is redundant. Also now
	// 
	//for (auto* r = race; r; r = r->armorParentRace) {
	//	logger::trace(": ArmorIndex::cachedIndexLookup");
	//	logger::trace(std::format(": :  r={:#010x}", r->GetFormID()));
	//	ArmorIndexKey id2(r, sex, occupation, nsfw);
	//	if (auto it = baseIndex.find(id2); it != baseIndex.end()) {
	//		for (int slot = 0; slot < 32; slot++) {
	//			ArmorIndex::IndexEntry& entry = it->second[slot];
	//			for (Tuple& tuple : entry.tuples) {
	//				//if (entry != NULL) {
	//					logger::trace(std::format(": ArmorIndex::cachedIndexLookup T[{}]={}", slot, tuple.inspect()));
	//					hit->tuples[slot].push_back(&tuple);
	//				//}
	//				//else {
	//				//	logger::trace("6");
	//				//	logger::warn(std::format(": ArmorIndex::cachedIndexLookup a Tuple entry at slot {} for this index has is NULL", slot));
	//				//}
	//			}
	//		}
	//	}
	//}
	// we don't want any tuples present in the cache hit more than once
	// for any given slot
	/*logger::trace(": ArmorIndex::cachedIndexLookup dedup");
	if (!hit) {
		logger::warn("! ArmorIndex::cachedIndexLookup => hit is NULL, this shouldn't be possible");
	}
	else {
		for (int slot = 0; slot < 32; slot++) {
			std::vector<Tuple>& vec = hit->tuples[slot];
			std::sort(vec.begin(), vec.end(),
				[](const Tuple* a, const Tuple* b) { return a && b ? a->id < b->id : (a ? false : true); });
			vec.erase(std::unique(vec.begin(), vec.end(),
				[](const Tuple* a, const Tuple* b) { return a && b ? a->id == b->id : (a ? false : true); }),
				vec.end());
			logger::trace(std::format(": ArmorIndex::cachedIndexLookup : size={} at slot {}", vec.size(), slot));
		}
	}*/
	// 4) Publish to memo + wake waiters
	/*
	logger::trace(": ArmorIndex::cachedIndexLookup publish");
	{
		std::unique_lock ul(cacheMutex);
	}
	{
		std::lock_guard gl(gate->m);
		gate->cachedValue = hit;
		gate->ready = true;
	}
	gate->cv.notify_all();

	// 5) Remove the gate (cleanup)
	logger::trace(": ArmorIndex::cachedIndexLookup release");
	{
		std::lock_guard lg(gatesMutex);
		gates.erase(id);
	}

	logger::trace("< ArmorIndex::cachedIndexLookup entry built");
	return hit;
	*/
}

bool ArmorIndex::registerTuple(uint8_t minLevel, bool nsfw, uint32_t sexes, uint32_t occupations, std::vector<RE::TESObjectARMO*> armors) {
	if (occupations == 0 || occupations > ALL_OCCUPATIONS) {
		logger::error("! Error: Occupations has wrong bit count");
		return false;
	}
	if (sexes > ALL_SEXES) {
		logger::error("! Error: Sexes has wrong bit count");
		return false;
	}
	Tuple tuple(minLevel, nsfw, sexes, occupations, armors);
	//master.push_back(tuple);
	//put(&master[master.size() - 1]);
	put(tuple);
	logger::debug("SCSCD: registered tuple " + tuple.inspect());
	return true;
}

void ArmorIndex::put(Tuple &t) {
	// or we could step through the existing cache and add the armor, but
	// this is unlikely to matter because in practice put() should only be
	// called on startup. Just clear the cache just-in-case and can optimize
	// later if calling patterns change.
	//cache.clear();

	logger::trace(std::format("ArmorIndex::put t={}", t.inspect()));
	for (uint32_t formID : t.armors) {
		RE::TESObjectARMO* armo = static_cast<RE::TESObjectARMO*>(RE::TESForm::GetFormByNumericID(formID));
		if (!armo) continue;
		for (RE::TESRace* race : t.possibleRaces()) {
			for (uint32_t sex : t.possibleSexes()) {
				for (uint32_t occupation : t.possibleOccupations()) {
					ArmorIndexKey key(race, sex, occupation, t.isNSFW);
					for (uint8_t slot : t.occupiedSlots()) {
						logger::trace(std::format("ArmorIndex::put r={:#010x} s={:x} o={:#010x} nsfw={} slot={} id={}",
							race->GetFormID(), sex, occupation, t.isNSFW, slot, t.id));
						size_t idx = this->tupleStorage.size();
						this->tupleStorage.push_back(t);
						logger::trace(std::format("ArmorIndex::put ... stored as tuple={}", this->tupleStorage[idx].inspect()));
						uint32_t id = this->tupleStorage[idx].id;
						// upper byte of id will contain minLevel. If it's nonzero we have a problem.
						if (id & 0xFF000000)
							logger::error(std::format(" !!! Natural tuple ID's upper byte is occupied. "
								"This is a mod limitation currently and means you have registered too many armors. "
								"Tuple {} will be omitted from the index. This limitation may be addressed in the "
								"future if the problem becomes widespread. Therefore, you are encouraged to report "
								"this issue to help decide whether it should be fixed.",
								t.inspect()));
						else {
							// encode minLevel into id so that we can efficiently
							// reject candidates before we have to access them.
							// If this constrains max # of tuples too much, we may
							// have to remove this (per message above) and access the
							// index to prune candidates at runtime.
							id = ((t.minLevel << 24) | id) & 0xFFFFFFFF;
							this->tupleIDtoIndex[id] = idx;
							this->proximityIndex.add(id, armo->GetFullName());
							baseIndex[key].sampleCandidates[slot].push_back(id);
						}
					}
				}
			}
		}
	}
}

std::vector<RE::TESObjectARMO*> ArmorIndex::sample(RE::Actor* a, SamplerConfig& config) {
	std::vector<RE::TESObjectARMO*> wardrobe;
	logger::debug("SCSCD: constructing wardrobe sample");
	uint32_t takenSlots = 0; // all slots available
	// Assume male if not otherwise specified. These are monsters etc that
	// are pretty androgynous.
	uint32_t sex = a->GetSex() == RE::SEX::kFemale ? FEMALE : MALE;
	logger::debug(std::format("SCSCD: > actor sex is {}", sex == FEMALE ? "female" : "male"));
	
	logger::debug("SCSCD: > getting actor occupation");
	uint32_t occupation = occupations->sample(a);
	if (occupation == 0) {
		logger::debug("SCSCD: > actor has no occupation, returning empty set");
		return wardrobe;
	}
	else {
		logger::debug("SCSCD: > actor's chosen occupation is {:#010x}", occupation);
	}
	ArmorIndex::CacheHit nsfwIndexHit = config.allowNSFWChoices ? cachedIndexLookup(true, a->race, sex, occupation) : NULL;
	ArmorIndex::CacheHit  sfwIndexHit = cachedIndexLookup(false, a->race, sex, occupation);
	logger::debug(std::format("SCSCD: > indexes nsfw={} sfw={}", !!nsfwIndexHit, !!sfwIndexHit));
	if (!nsfwIndexHit && !sfwIndexHit) {
		logger::debug("SCSCD: > No valid indexes, returning empty set");
		return wardrobe;
	}

	// step through each of the 32 possible biped slots in random order.
	// Sample each, disabling slots as we go so that we can't pick tuples
	// that occupy similar slots.
	// randomly order the slots
	int slots[32];
	uint32_t mostRecentTupleID = 0xFFFFFFFF; // no choice to start
	std::iota(std::begin(slots), std::end(slots), 0);
	std::shuffle(std::begin(slots), std::end(slots), std::default_random_engine{});
	for (int slot : slots) {
		logger::trace(std::format("evaluating slot {}", slot));
		if (!(takenSlots & (uint32_t)(1 << slot))) {
			logger::trace(std::format("slot is available"));
			uint8_t skipSlot = (uint8_t)(rand() % 100);
			if (skipSlot < config.skipSlotChance[slot]) {
				// we won't fill in this slot.
				logger::trace(std::format("slot {} skipped - random chance {} < {}", slot, skipSlot, config.skipSlotChance[slot]));
				continue;
			}

			// slot is still available; try to find an armor to fill it

			// build a set of sampleCandidates from the sfw index, and add the nsfw index
			// if it's enabled.
			std::vector<uint32_t> sampleCandidates;
			if (sfwIndexHit) {
				logger::trace("getting sfw samples");
				std::vector<uint32_t>& ref = sfwIndexHit->sampleCandidates[slot];
				logger::trace(std::format("adding {} samples", ref.size()));
				sampleCandidates.insert(sampleCandidates.end(), ref.begin(), ref.end());
			}
			if (nsfwIndexHit) {
				logger::trace("getting nsfw samples");
				std::vector<uint32_t>& ref = nsfwIndexHit->sampleCandidates[slot];
				logger::trace(std::format("adding {} samples", ref.size()));
				sampleCandidates.insert(sampleCandidates.end(), ref.begin(), ref.end());
			}
			// Reject any tuple which occupies a bit that is already set.
			// Yes we are indexing by slot, but the armors may occupy more than one slot,
			// and if they do then the other occupied slot may already be taken.
			logger::trace("rejecting ineligible samples");
			for (size_t i = 0; i < sampleCandidates.size(); i++) {
				uint32_t id = sampleCandidates[i];
				uint8_t minLevel = (id >> 24) & 0xFF;
				bool rejected = false;
				if (minLevel > a->GetLevel()) {
					logger::trace(std::format("slot {} rejecting candidate id {} because its minLevel {} is higher than actor level {}",
						slot, id, minLevel, a->GetLevel()));
					rejected = true;
				}
				if ((takenSlots & tupleStorage[tupleIDtoIndex[id]].slots) != 0) {
					// found one
					logger::trace(std::format("slot {} rejecting candidate id {} because its slots are in use: cand={:#010x} & taken={:#010x} != 0",
						slot, id, tupleStorage[tupleIDtoIndex[id]].slots, takenSlots));
					rejected = true;
				}
				if (rejected) {
					sampleCandidates.erase(sampleCandidates.begin() + i);
					i--;
				}
			}
			if (sampleCandidates.size() == 0) {
				logger::trace(std::format("slot {} skipped - there are no eligible candidates for it", slot));
				continue;
			}

			// choose a candidate index at random. Lower indexes reference
			// nsfw, higher reference sfw.
			// choose between sfw and nsfw using a weight according to number of entries in an index
			// so that bigger index weighs heavier for greater variety.
			logger::trace(std::format("slot {} - sampling from {} candidates", slot, sampleCandidates.size()));
			uint32_t tupleID = this->proximityIndex.sampleBiased(mostRecentTupleID, sampleCandidates, config.proximityBias);
			logger::trace(std::format("slot {} - sampled tuple ID {}", slot, tupleID));
			mostRecentTupleID = tupleID;
			Tuple &tuple = tupleStorage[tupleIDtoIndex[tupleID]];
			// strip high bits (min level) for check
			if (tuple.id != (tupleID & 0x00FFFFFF)) {
				logger::error(std::format("BUG: tuple ID {} does not match its indexed ID {}: probable bad pointer or memory corruption", tuple.id, tupleID));
			}

			// We finally have the chosen tuple.
			// Mark all slots in use by this tuple as no longer available, and add
			// the armor items it contains to the wardrobe.
			takenSlots = takenSlots | tuple.slots;
			logger::trace(std::format(": ArmorIndex::sample() looking up {} armors, takenSlots is now {:#010x}", tuple.armors.size(), takenSlots));
			for (int i = 0; i < tuple.armors.size(); i++) {
				logger::trace(std::format(": ArmorIndex::sample() getting armor form {:#010x}", tuple. armors[i]));
				RE::TESForm* form = RE::TESForm::GetFormByID(tuple.armors[i]);
				if (form) logger::trace(": ArmorIndex::sample() form found");
				else {
					logger::warn(std::format(": ArmorIndex::sample() form {:#010x} not found (this shouldn't happen!)", tuple.armors[i]));
					continue;
				}
				RE::TESObjectARMO* armor = form->As<RE::TESObjectARMO>();
				logger::trace(": ArmorIndex::sample() - adding as armor");
				wardrobe.push_back(armor);
			}
			logger::trace(std::format(": slot {} eval is now complete", slot));
		}
	}
	logger::trace(": ArmorIndex::sample() - all slots considered");

	// in theory, wardrobe now complete consists of armors that do not
	// overlap; unused biped slots represent slots that are not used by
	// any registered armor that matches the race, sex, occupation, faction
	// constraints.
	logger::debug(std::format("SCSCD: sample contains {} armors and final bitmap is {:#010x}", wardrobe.size(), takenSlots));
	for (auto* armor : wardrobe) {
		logger::trace(std::format("       - {:#010x} bip={:#010x} {}", armor->GetFormID(), armor->bipedModelData.bipedObjectSlots, armor->GetFullName()));
	}
	// no need to check or log anything if it's already an empty set - no changes would be made
	if (wardrobe.size() > 0) {
		if (!config.allowNudity && (!HasSlot(takenSlots, 36) || !HasSlot(takenSlots, 39))) {
			logger::debug("SCSCD: nudity detected in this wardrobe, returning empty set instead");
			wardrobe.clear();
		}
	}
	return wardrobe;
}