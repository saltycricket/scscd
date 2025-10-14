#include "scscd.h"
#include "matswap_validity_report.h"
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
		RE::TESObjectARMO* armo = static_cast<RE::TESObjectARMO*>(RE::TESForm::GetFormByID(formID));
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

	if (a == NULL) {
		logger::warn("asked to sample armor for a NULL actor!");
		return wardrobe;
	}

	/* Conservative equipping: the actor may have been spawned with all kinds of leveled list
	   equipment, armor overrides, etc. There's also a chance we might conflict with some other mod which
	   altered the actor's outfit package. So, check which items are equipped by default. In
	   vanilla slot 33 (bit 3) should ALWAYS be set for clothing; this is the Body outfit. Anything
	   else is an armor, weapon, or someone else's mod. (Slot 33 item may occupy more than one slot, but
	   outfit always occupies slot 33.) Initialize takenSlots to represent the current
	   outfit, MINUS the body slot that we intend to replace. The result is the set of available
	   slots that aren't fitted with some leveled item that the actor should be using. We can
	   also check the high bits of the form ID. If it's less than some threshold (allowing for DLC),
	   then we know that the item is vanilla. We should never replace a non-vanilla (modded)
	   item as this may be part of a quest mod or other custom tailored NPC outfit. */
	if (a->inventoryList) {
		// Header for ForEachStack implies that we need a read lock for the operations below. Not sure the best way to do that but
		// GetAllForms() returns us one.
		const auto& [map, lock] = RE::TESForm::GetAllForms();
		RE::BSAutoReadLock l{ lock };
		a->inventoryList->ForEachStack(
			[&](RE::BGSInventoryItem& item) { return true; }, // iterate over everything
			[&](RE::BGSInventoryItem& item, RE::BGSInventoryItem::Stack& stack) {
				if (stack.IsEquipped()) {
					// equipped item, fill its slot
					// Unfortunately I don't know a better way to safely check if the BoundObject
					// is also a BipedObject, except to use an explicit whitelist. But it seems
					// armor is the only type that has biped slots, with weapon slots being
					// hard coded(?) and not relevant here as those slots aren't referenced by
					// this mod(?).
					switch (item.object->GetFormType()) {
						case RE::ENUM_FORM_ID::kARMO: {
							// explicitly skip over any armor that occupies slot 33 (bit index 3).
							// We plan to replace this item as should be the vanilla outfit. However,
							// since it often fills more than just slot 33, we need to be careful to
							// skip it entirely rather than setting it here and reverting it later.
							// Otherwise we'll set bits on other slots that it occupies, creating
							// gaps in the final outfit as those slots wouldn't be considered for
							// outfitting.
							const uint32_t HIGHEST_VANILLA_HIGH_FORMID = 0x06; // Fallout4.esm at 0x00 plus 6 DLCs
							uint32_t armoSlots = item.object->As<RE::TESObjectARMO>()->bipedModelData.bipedObjectSlots;
							#define EQUIP_THIS_SLOT        /* no action taken */
							#define DO_NOT_EQUIP_THIS_SLOT takenSlots = takenSlots | armoSlots
							if (((item.object->GetFormID() >> 24) & 0xFF) > HIGHEST_VANILLA_HIGH_FORMID) {
								// If the item is a modded item, we never want to replace it. Do nothing.
								DO_NOT_EQUIP_THIS_SLOT;
							} else if (config.replaceArmor || (armoSlots & (1 << 3))) {
								// If the item is vanilla, and either config is set to replace it OR
								// it's the body slot which we always replace (if vanilla), then
								// DO replace it.
								EQUIP_THIS_SLOT;
							}
							else if (config.replaceArmor) {
								// This is not a body slot, and config wants to replace the item.
								EQUIP_THIS_SLOT;
							} else {
								// replaceArmor is false, and the slot is not a body slot.
								// Do nothing.
								DO_NOT_EQUIP_THIS_SLOT;
							}
							#undef DO_NOT_EQUP_THIS_SLOT
							#undef EQUIP_THIS_SLOT
							logger::debug(std::format("seen actor {:#010x} wearing armo {:#010x} has armoSlots {:#010x}, taken is now {:#010x}", a->GetFormID(), item.object->GetFormID(), armoSlots, takenSlots));
							break;
						}
					}
				}
				return true; // keep iterating
			}
		);
	}

	// Assume male if not otherwise specified. These are monsters etc that
	// are pretty androgynous.
	uint32_t sex = actorIsFemale(a) ? FEMALE : MALE;
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
	const uint8_t * const & fillSlotChance = (sex == MALE ? config.fillSlotChanceM : config.fillSlotChanceF);
	for (int slot : slots) {
		logger::trace(std::format("evaluating slot {}", slot));
		if (!(takenSlots & (uint32_t)(1 << slot))) {
			logger::trace(std::format("slot is available"));
			uint8_t fillSlot = (uint8_t)(rand() % 100);
			if (fillSlot >= fillSlotChance[slot]) {
				// we won't fill in this slot.
				logger::trace(std::format("slot {} skipped - random chance {} >= {}", slot, fillSlot, fillSlotChance[slot]));
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
		if (!config.allowNudity && ((sex == FEMALE && !HasSlot(takenSlots, /* chest underwear */ 36)) || !HasSlot(takenSlots, /* pelvis underwear */ 55))) {
			logger::debug("SCSCD: nudity detected in this wardrobe, returning empty set instead");
			wardrobe.clear();
		}
	}
	return wardrobe;
}

// Utility: is TESForm a BGSMaterialSwap?
static inline RE::BGSMaterialSwap* AsMSWP(RE::TESForm* f) {
	return f ? f->As<RE::BGSMaterialSwap>() : nullptr;
}

static bool validateMSWP(RE::BGSMaterialSwap *swap) {
	bool valid = true;
	logger::trace(std::format("  validating mswp: {:#010x}", swap->GetFormID()));
	SwapPreflightReport report = PreflightValidateBGSMTextures(swap);
	if (report.missingMaterials.size() > 0 || report.missingTextures.size() > 0) {
		for (auto& filename : report.okMaterials) {
			logger::trace(std::format("    -> ok! material {}", filename));
		}
		for (auto& filename : report.missingMaterials) {
			logger::error(std::format("    -> material {} could not be found", filename));
			valid = false;
		}
		for (auto& filename : report.okTextures) {
			logger::trace(std::format("    -> ok! texture {}", filename));
		}
		for (auto& filename : report.missingTextures) {
			logger::error(std::format("    -> texture {} could not be found", filename));
			valid = false;
		}
	}
	return valid;
}

static std::span<const RE::BGSMod::Property::Mod>
TryGetPropertySpan(const RE::BGSMod::Container* cont)
{
	// Probe both possible block IDs (0 and 1). Return the one that "looks like" Property::Mod[].
	std::array<std::span<const RE::BGSMod::Property::Mod>, 2> cands = {
		cont->GetBuffer<RE::BGSMod::Property::Mod>(0),
		cont->GetBuffer<RE::BGSMod::Property::Mod>(1),
	};

	auto looks_valid = [](std::span<const RE::BGSMod::Property::Mod> s) -> bool {
		if (s.empty()) return false;
		// Sanity-check a few entries: type must be within enum range.
		std::size_t checks = std::min<std::size_t>(s.size(), 4);
		for (std::size_t i = 0; i < checks; ++i) {
			const auto t = s[i].type;
			using T = RE::BGSMod::Property::TYPE;
			if (!(t == T::kInt || t == T::kFloat || t == T::kBool || t == T::kString ||
				t == T::kForm || t == T::kEnum || t == T::kPair)) {
				return false;
			}
		}
		return true;
	};

	if (looks_valid(cands[0])) return cands[0];
	if (looks_valid(cands[1])) return cands[1];
	return {}; // none found
}

bool ArmorIndex::registerOmods(std::vector<RE::TESObjectARMO*>& armors, std::vector<RE::BGSMod::Attachment::Mod*>& omods, bool nsfw) {
	logger::trace("> ArmorIndex::registerOmods");

	/*
	 validate omods: if we can reach into the BSMaterialSwap we should be able to use RE::BSResourceNiBinaryStream
	 to try and open the material/texture files (even if it's in an archive) to check whether the materials are actually
	 available. If we do this we won't have any purple outfits at runtime if someone's got an ESP but missing the
	 textures.
	*/
	std::vector<RE::BGSMod::Attachment::Mod*> validOmods;
	for (RE::BGSMod::Attachment::Mod* mod : omods) {
		bool valid = true;

		// legacy - matswap specified directly on omod; uncommon in FO4 but should still be supported
		if (mod->swapForm) {
			if (!validateMSWP(mod->swapForm)) {
				logger::error(std::format("skipped: omod {:#010x} failed validation (1)", mod->GetFormID()));
				valid = false;
			}
		}

		// FO4 'modern' - matswap specified as omod property. mod->GetData() isn't working in our revision,
		// so we'll try an alternative way to gain access.
		const auto props = TryGetPropertySpan(mod);
		for (const auto& p : props) {
			switch (p.type) {
				case RE::BGSMod::Property::TYPE::kForm: {
					if (auto* f = p.data.form) {
						if (auto* mswp = f->As<RE::BGSMaterialSwap>()) {
							if (!validateMSWP(mswp)) {
								logger::error(std::format("skipped: omod {:#010x}: mswp {:#010x} failed validation (2)", mod->GetFormID(), mswp->GetFormID()));
								valid = false;
							}
						}
					}
					break;
				}
				case RE::BGSMod::Property::TYPE::kPair: {
					// Some mods encode (form,value) in a pair. Resolve the formID.
					const auto formID = p.data.fv.formID;
					if (auto* f = RE::TESForm::GetFormByID(formID)) {
						if (auto* mswp = f->As<RE::BGSMaterialSwap>()) {
							if (!validateMSWP(mswp)) {
								logger::error(std::format("skipped: omod {:#010x}: mswp {:#010x} failed validation (3)", mod->GetFormID(), mswp->GetFormID()));
								valid = false;
							}
						}
					}
					break;
				}
				default:
					// kEnum/kInt/kFloat/kBool/kString don't carry forms; ignore for MSWP collection.
					break;
			}
		}

		if (valid) validOmods.push_back(mod);
	}
	logger::trace(std::format("{} of {} omods survived validation", validOmods.size(), omods.size()));

	for (RE::TESObjectARMO* armor : armors) {
		uint32_t armorID = armor->GetFormID();
		for (RE::BGSMod::Attachment::Mod* omod : validOmods) {
			uint32_t omodID = omod->GetFormID();
			std::unordered_set<uint32_t>& index = (nsfw ? nsfwArmorOmods[armorID] : sfwArmorOmods[armorID]);
			// don't register an omod more than once, else we'll end up weighting that
			// omod more than the others. (Guessing usually would not be what is intended.)
			if (!index.contains(omodID)) {
				index.insert(omodID);
				omodProximityIndex.add(omodID, omod->GetFormEditorID());
			}
		}
	}
	logger::trace("< ArmorIndex::registerMatswaps");
	return true;
}

RE::BGSMod::Attachment::Mod* ArmorIndex::sampleOmod(RE::TESObjectARMO* armor, float proximityBias, RE::BGSMod::Attachment::Mod* other, bool allowNSFW) {
	logger::trace("> ArmorIndex::sampleOmod");
	uint32_t armorFormID = armor->GetFormID();
	uint32_t otherFormID = other ? other->GetFormID() : 0xFFFFFFFF;
	std::vector<uint32_t> candidates;
	for (uint32_t candidate : sfwArmorOmods[armorFormID])
		candidates.push_back(candidate);
	if (allowNSFW) {
		for (uint32_t candidate : nsfwArmorOmods[armorFormID])
			candidates.push_back(candidate);
	}
	uint32_t sampledID = omodProximityIndex.sampleBiased(otherFormID, candidates, proximityBias);
	RE::TESForm* form = RE::TESForm::GetFormByID(sampledID);
	if (form == NULL) {
		logger::trace("< ArmorIndex::sampleOmod NULL");
		return NULL;
	}
	else {
		logger::trace("< ArmorIndex::sampleOmod found");
		return form->As<RE::BGSMod::Attachment::Mod>();
	}
}
