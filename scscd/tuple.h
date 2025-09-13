#pragma once
#include "scscd.h"
#include "logger.h"

/*
 TUPLE layout (32-bit unsigned):
	- race (14 bits)
	- sex (2 bits)
	- occupation (16 bits)
 This means maximum 14 supported races, 2 supported sexes, 16 supported occupations.
 Warning: if this layout ever changes then the index will need to be rebuilt!
 Upgrade policy: Entries can be added but cannot be safely changed or removed. If they
 are, the index will need to be rebuilt.
*/
//#define RACE_WIDTH 14

// Races - 14 bits available
//#define ALIEN       0x2000 // '10000000000000'
//#define DOG         0x1000 // '01000000000000'
//#define FERAL       0x0800 // '00100000000000'
//#define GHOUL_ADULT 0x0400 // '00010000000000'
//#define GHOUL_CHILD 0x0200 // '00001000000000'
//#define HUMAN_ADULT 0x0100 // '00000100000000'
//#define HUMAN_CHILD 0x0080 // '00000010000000'
//#define SUPERMUTANT 0x0040 // '00000001000000'
//#define SYNTH_GEN1  0x0020 // '00000000100000'
//#define SYNTH_GEN2  0x0010 // '00000000010000'
//#define ALL_RACES (ALIEN | DOG | FERAL | GHOUL_ADULT | GHOUL_CHILD | HUMAN_ADULT | HUMAN_CHILD | SUPERMUTANT | SYNTH_GEN1 | SYNTH_GEN2)

//static inline uint32_t RACE_FOR_FORMID(uint32_t formID) {
//	switch (formID) {
//		case 0x00184C4D: return ALIEN;
//		case 0x000A96BF: return FERAL;
//		case 0x0006B4EC: return FERAL;
//		case 0x000EAFB6: return GHOUL_ADULT;
//		case 0x0011EB96: return GHOUL_CHILD;
//		case 0x0011D83F: return HUMAN_CHILD;
//		case 0x00013746: return HUMAN_ADULT;
//		case 0x00187AF9: return DOG;
//		case 0x0001A009: return SUPERMUTANT;
//		case 0x000E8D09: return SYNTH_GEN1;
//		case 0x0010BD65: return SYNTH_GEN2;
//		case 0x0003578A: return DOG;
//		default: return 0;
//	}
//}

#define SEX_WIDTH 2 /* Number of sex bits */
#define MALE   0x1
#define FEMALE 0x2
#define ALL_SEXES (MALE | FEMALE)
#define NO_SEXES 0
#define NO_SEX NO_SEXES
typedef uint32_t Sex;

/* there is a hard limit of 32 bits */

// 'institute spy' doesn't need an entry because it'd just be one of the others...
// does institute or bos need its own scientists, medics? Greater fragmentation means
// higher maintenance costs, where is the balance?

#define OCCUPATION_WIDTH 20 /* Number of occupation bits actually used */
#define RAILROAD_RUNAWAY    0x80000 // 10000000000000000000
#define RAILROAD_AGENT      0x40000 // 01000000000000000000
#define MINUTEMAN           0x20000 // 00100000000000000000
#define INSTITUTE_SOLDIER   0x10000 // 00010000000000000000
#define INSTITUTE_ASSASSIN  0x08000 // 00001000000000000000
#define GUNNER              0x04000 // 00000100000000000000
#define RAIDER              0x02000 // 00000010000000000000
#define BOS_SOLDIER         0x01000 // 00000001000000000000
#define BOS_SUPPORT         0x00800 // 00000000100000000000
#define MERCHANT            0x00400 // 00000000010000000000
#define CITIZEN             0x00200 // 00000000001000000000
#define CULTIST             0x00100 // 00000000000100000000
#define DRIFTER             0x00080 // 00000000000010000000
#define FARMER              0x00040 // 00000000000001000000
#define GUARD               0x00020 // 00000000000000100000
#define SCIENTIST           0x00010 // 00000000000000010000
#define MERCENARY           0x00008 // 00000000000000001000
#define VAULTDWELLER        0x00004 // 00000000000000000100
#define CAPTIVE             0x00002 // 00000000000000000010
#define DOCTOR              0x00001 // 00000000000000000001
#define ALL_OCCUPATIONS     0xFFFFF
#define NO_OCCUPATIONS 0
#define NO_OCCUPATION NO_OCCUPATIONS
typedef uint32_t Occupation;

static Occupation STR2OCCUPATION(std::string occup) {
	std::transform(occup.begin(), occup.end(), occup.begin(),
		[](unsigned char c) { return std::tolower(c); });
	if (occup == "railroad_runaway")   return RAILROAD_RUNAWAY;
	if (occup == "railroad_agent")     return RAILROAD_AGENT;
	if (occup == "minuteman")          return MINUTEMAN;
	if (occup == "institute_soldier")  return INSTITUTE_SOLDIER;
	if (occup == "institute_assassin") return INSTITUTE_ASSASSIN;
	if (occup == "gunner")             return GUNNER;
	if (occup == "raider")             return RAIDER;
	if (occup == "bos_soldier")        return BOS_SOLDIER;
	if (occup == "bos_support")        return BOS_SUPPORT;
	if (occup == "merchant")           return MERCHANT;
	if (occup == "citizen")            return CITIZEN;
	if (occup == "cultist")            return CULTIST;
	if (occup == "drifter")            return DRIFTER;
	if (occup == "farmer")             return FARMER;
	if (occup == "guard")              return GUARD;
	if (occup == "scientist")          return SCIENTIST;
	if (occup == "mercenary")          return MERCENARY;
	if (occup == "vaultdweller")       return VAULTDWELLER;
	if (occup == "captive")            return CAPTIVE;
	if (occup == "doctor")             return DOCTOR;
	return NO_OCCUPATION;
}

static Occupation BINSTR2OCCUPATION(std::string binstr) {
	Occupation r = NO_OCCUPATIONS;
	for (int i = 0; i < binstr.size(); i++) {
		if (binstr[i] == '1') {
			r = r | (1 << (OCCUPATION_WIDTH - i - 1));
		}
	}
	logger::trace(std::format("BINSTR2OCCUPATION: {} => {:#010x}", binstr, r));
	return r;
}

static Sex BINSTR2SEX(std::string binstr) {
	Sex r = NO_SEXES;
	for (int i = 0; i < binstr.size(); i++) {
		if (binstr[i] == '1') {
			r = r | (1 << (SEX_WIDTH - i - 1));
		}
	}
	logger::trace(std::format("BINSTR2SEX: {} => {:#010x}", binstr, r));
	return r;
}


#define TUPLE(race, sex, occupation) ( ((race       & ALL_RACES      ) << (SEX_WIDTH + OCCUPATION_WIDTH)) \
								     | ((sex        & ALL_SEXES)       << (            OCCUPATION_WIDTH)) \
                                     | ((occupation & ALL_OCCUPATIONS) << (                           0)) )

// Unpack the race(s) from a tuple.
#define TUPLE_RACE(t)       ((t >> (SEX_WIDTH + OCCUPATION_WIDTH)) & ALL_RACES)
// Unpack the sex(es) from a tuple.
#define TUPLE_SEX(t)        ((t >> (            OCCUPATION_WIDTH)) & ALL_SEXES)
// Unpack the occupation(s) from a tuple.
#define TUPLE_OCCUPATION(t) ((t >> (                           0)) & ALL_OCCUPATIONS)

class Tuple {
public:
	static uint32_t next_id;
	const uint32_t id; // globally unique ID for this tuple; useful for sorting/dedup ops
	std::vector<uint32_t> armors; // not safe to store pointers long term, so use form IDs
	uint32_t occupations; // Bitmap of occupations to register with this Tuple
	uint32_t slots; // cached copy of all armor biped slots combined
	bool isNSFW; // if any armor is NSFW, the whole tuple should be NSFW.
	uint32_t overrideSexes; // if nonzero, it was loaded from CSV; else it will be computed dynamically.
	uint8_t minLevel;

	Tuple(uint8_t minLevel, bool nsfw, uint32_t sexes, uint32_t occupations, std::vector<RE::TESObjectARMO*> armors);

	// Returns a list of slots where each entry in the list
	// is a slot index - that is, [0-31] inclusive. Each entry
	// corresponds with a bit in the `slots` field.
	std::vector<uint8_t> occupiedSlots() {
		std::vector<uint8_t> result;
		for (uint8_t bit = 0; bit < 32; bit++) {
			if (slots & (1 << bit)) {
				logger::trace(std::format("occupiedSlots() {:#010x} & {:#010x} != 0, push {}", slots, bit, bit));
				result.push_back(bit);
			}
		}
		return result;
	}

	/*
	 * Returns the set of races defined in ALL armors in this tuple,
	 * exclusively. For example if armor A specifies Human and Ghoul
	 * but armor B specifies only Ghoul, the result will be [Ghoul].
	 */
	std::vector<RE::TESRace*> possibleRaces();

	/*
	 * Returns the bitmap of possible sexes for this Tuple.
	 */
	uint32_t sexes();

	/*
	 * Returns the set of sexes compatible with ALL armors in this tuple.
	 * For example if armor A specifies MALE | FEMALE but armor B specifies
	 * only FEMALE then the result is [FEMALE].
	 */
	std::vector<uint32_t> possibleSexes();

	/*
	 * Returns the set of possible occupations compatible with this Tuple.
	 * The result is a set of distinct occupations, not a bitmap.
	 */
	std::vector<uint32_t> possibleOccupations();

	std::string inspect();
};
