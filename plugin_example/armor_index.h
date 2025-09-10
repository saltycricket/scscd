#pragma once

#include <vector>
#include "scscd.h"
#include "tuple.h"
#include <shared_mutex>
#include <set>
#include <functional>
#include "edid_similarity.h"
#include <filesystem>

void applyMatSwap(RE::Actor* actor, RE::TESObjectARMO* armor, RE::BGSMaterialSwap* swap);

// Utility: check if a biped slot (30-61) is set in the mask returned by GetFilledSlots()
static bool HasSlot(std::uint32_t filledMask, int slotIndex)
{
	// Guard: valid human slots are 30-61
	if (slotIndex < 30 || slotIndex > 61) {
		return false;
	}
	return (filledMask & (1u << (slotIndex - 30))) != 0;
}

static int slot2bit(int slot) {
	return slot - 30;
}

static int bit2slot(int bit) {
	return bit + 30;
}


class ArmorIndexKey {
public:
	const uint32_t race; // Form ID of race
	// these are single values, not bitmaps! e.g. [MALE, CITIZEN].
	const uint32_t sex;
	const uint32_t occupation;
	// Is the item NSFW?
	const bool isNSFW;

	ArmorIndexKey(RE::TESRace *race, uint32_t sex, uint32_t occupation, bool nsfw)
		: race(race->GetFormID()), sex(sex), occupation(occupation), isNSFW(nsfw)
	{
	}

	bool operator==(const ArmorIndexKey& o) const noexcept {
		return race == o.race
			&& sex == o.sex
			&& occupation == o.occupation
			&& isNSFW == o.isNSFW;
	}
};

namespace std {
	template<>
	struct hash<ArmorIndexKey> {
		size_t operator()(ArmorIndexKey const& k) const noexcept {
			size_t h = std::hash<uint32_t>{}(k.race);
			h ^= std::hash<uint32_t>{}(k.sex) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
			h ^= std::hash<uint32_t>{}(k.occupation) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
			h ^= std::hash<uint32_t>{}(k.isNSFW ? 1 : 0) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
			return h;
		}
	};
} // namespace std

class ArmorIndex {
	// map of tuple ID -> tuple
	std::vector<Tuple> tupleStorage;
	std::unordered_map<uint32_t, size_t> tupleIDtoIndex;

	// Proximity index is used to try to pick similarly-named armors (on the
	// assumption that they are probably meant to be used together).
	EdidIndex proximityIndex;

	// Full set of available matswaps.
	// We index them by [armor ID, matswap candidates]. We also maintain a
	// second EdidIndex - this one for matswaps, not armors.
	// When sampling occurs, we use the proximityBias from the sampler config
	// to choose a matswap that is similar to other chosen matswaps. Hopefully,
	// this will encourage 'color' coordination and won't (usually) produce a
	// rando-rainbow outfit.
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> sfwArmorMatswaps;
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> nsfwArmorMatswaps;
	EdidIndex matswapProximityIndex;

	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> sfwArmorOmods;
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> nsfwArmorOmods;
	EdidIndex omodProximityIndex;

	typedef struct {
		// vector of tuple IDs for any given slot that can be sampled by the
		// proximity index. These must be segregated by slot (32 slots total)
		// to avoid sampling an armor that uses some slot other than the one
		// we're currently processing.
		std::vector<uint32_t> sampleCandidates[32];
	} IndexEntry;

	// master storage so we can use pointers as we have a lot of denormalization
	// going on
	//std::vector<Tuple> master;

	// Base index contains the data as registered. Keys represent possible
	// race/sex/occupation combos. Values are all armors registered with
	// that combo. At runtime a lookup will be performed only if a cache hit
	// isn't found first, to deal with race inheritance.
	//std::unordered_map <ArmorIndexKey, std::vector<Tuple*>[32]> baseIndex;
	std::unordered_map <ArmorIndexKey, IndexEntry> baseIndex;

	// Cache to avoid race lookup once a valid race is found. Vectors are
	// not pointers because they will contain the combined total of all matching
	// race armors, deduplicated. For example an armor registered to a human
	// plus an armor registered to a ghoul, assuming human > ghoul, a lookup
	// for ghoul should return both armors. So the cache is not just for
	// race inheritance alone, it also caches the combining of multiple matches.
	typedef IndexEntry* CacheHit;
	//std::unordered_map <ArmorIndexKey, CacheHit> cache;
	//std::shared_mutex cacheMutex;

	//struct Gate {
	//	std::mutex m;
	//	std::condition_variable cv;
	//	bool ready { false };
	//	CacheHit cachedValue;
	//};
	//std::unordered_map<ArmorIndexKey, std::shared_ptr<Gate>> gates;
	//std::mutex gatesMutex;

	CacheHit cachedIndexLookup(bool nsfw, RE::TESRace* race, uint32_t sex, uint32_t occupation);
	void put(Tuple& t);

public:
	class SamplerConfig {
	public:
		/*
		 * Integer percentage value betwen [0, 100].
		 * What is the likelihood of replacing the current outfit?
		 * A value of 100 would never produce an unaltered outfit.
		 * A value of 0 effectively disables this mod.
		 * Note that even with a value of 100 if a viable outfit
		 * can't be produced based on other rules or not enough clothings,
		 * NPCs can still end up with an unaltered outfit.
		 */
		uint8_t changeOutfitChanceM{ 0 };
		uint8_t changeOutfitChanceF{ 0 };

		/*
		 * Integer percentage value between [0, 100].
		 * For each of the possible 32 slots, the chance it will
		 * be ignored and no item will be chosen. (Note: another
		 * item on a different slot that happens to fill more than
		 * one slot, may still be chosen and fill the ignored slot.)
		 * 
		 * This is mainly intended to influence accessories like hats,
		 * necklaces, etc.
		 */
		uint8_t skipSlotChance[32]{ 0 };

		/*
		 * Should NSFW items be considered?
		 */
		bool allowNSFWChoices{ false };

		/*
		 * If true, it is possible to retrieve a wardrobe with
		 * slots 36 (on women) and 39 (on men and women) not filled.
		 * Recommendation: leave true if you have no underwear items,
		 * otherwise nothing will ever be chosen. If you do
		 * have items to fill those slots, it's down to preference.
		 * 
		 * Note that 'nudity' here means 'slots 36 and 39 might not
		 * be filled'. It does NOT necessarily mean 'the bits are
		 * showing' - that will depend on what is chosen for all other
		 * slots combined.
		 */
		bool allowNudity{ false };

		/*
		 * Amount by which to bias searches towards matching armor pieces.
		 * If zero, items will be chosen completely at random. Other
		 * values:
		 * 0.0-0.5       : Very gentle bias. Almost uniform, small nudge toward better matches.
		 * 1.0           : Moderate bias. Similar candidates are 2-3x more likely, but random variation is still significant.
		 * 2.0 (default) : Stronger clustering. "Set" pieces will dominate, but not to the exclusion of variety.
		 * 3.0-5.0       : Very strong bias. The algorithm behaves almost like "pick the best match every time" with a little noise left.
		 * >5.0          : Practically deterministic. Unless two items tie in similarity, the same partner will be picked every time.
		 */
		float proximityBias{ 2.0 };

		std::filesystem::path inipath, defaultPath;
		std::time_t iniModTime{ 0 };

		SamplerConfig() {}

		void load(std::filesystem::path filename, std::filesystem::path defaultSettingsFilename);
		void reload() { load(this->inipath, this->defaultPath); }
		bool haveSettingsChanged();
		//void setAllowNSFW(bool);
		//void setAllowNudity(bool);
		//void setProximityBias(float);
		//void setChangeOutfitChance(uint8_t);
		//void setSkipSlotChance(uint8_t slot, uint8_t chance);
	};

	OccupationIndex* occupations;

	ArmorIndex(OccupationIndex *occupations)
	{
		this->occupations = occupations;
	}
	
	/*
	 * Registers a set of matswaps to a set of armors. Later, any one armor can be
	 * used to retrieve a random matswap.
	 * 
	 * Returns true on success.
	 */
	bool registerMatswaps(std::vector<RE::TESObjectARMO*>& armors, std::vector<RE::BGSMaterialSwap*>& matswaps, bool isNSFW);
	bool registerOmods(std::vector<RE::TESObjectARMO*>& armors, std::vector<RE::BGSMod::Attachment::Mod*>& omods, bool isNSFW);

	/*
	 * Samples available matswaps for the given armor, returning one of them.
	 * You can provide 'other' to indicate a previous matswap selection. If you
	 * do, a similar one will be found using proximityBias. If 'other' is NULL,
	 * a matswap will be returned completely at random.
	 */
	RE::BGSMaterialSwap* sampleMatswap(RE::TESObjectARMO* armor, float proximityBias, RE::BGSMaterialSwap* other, bool allowNSFW);
	RE::BGSMod::Attachment::Mod* sampleOmod(RE::TESObjectARMO* armor, float proximityBias, RE::BGSMod::Attachment::Mod* other, bool allowNSFW);

	/*
	 * Registers a set of Armors with a bitmap of compatible Occupations.
	 * The armors will be indexed as a group, and returned as a group if they are
	 * sampled later on. Thus if you want to register 2 separate armors, call
	 * registerTuple twice with two one-item vectors. Otherwise they'll be treated
	 * as a single registered entity.
	 * 
	 * Compatible races are queried from the armors directly. If any armor defines
	 * a race that is not compatible with any other armor in the list, that race
	 * won't be registered.
	 * 
	 * Compatible sexes are queried the same way and follow the same rules as races.
	 * 
	 * Returns true on success.
	 */
	bool registerTuple(uint8_t minLevel, bool nsfw, uint32_t sexes, uint32_t occupations, std::vector<RE::TESObjectARMO*> armors);

	/*
	 * Samples the index for the given actor by looking up its race, sex and occupation.
	 * Attempts to build and return a complete wardrobe for that actor, filling as
	 * many biped slots as possible from the matching index.
	 * 
	 * The second argument provides a set of configuration options to influence the
	 * sampling operation.
	 */
	std::vector<RE::TESObjectARMO*> sample(RE::Actor* a, SamplerConfig &s);
};
