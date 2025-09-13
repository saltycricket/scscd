#include "scscd.h"

#include <unordered_set>

inline static bool modelHasFile(const RE::TESModel& m) {
	logger::trace("> modelHasFile");
	const char* s = m.GetModel();
	logger::trace(std::format("< modelHasFile => {} => {}", s, (s && *s)));
	return s && *s;
}

inline static uint32_t armorSupportedSexes(const RE::TESObjectARMO* armo) {
	uint32_t r = 0;
	logger::trace("> armorSupportedSexes");
	if (!armo) {
		logger::trace("< armorSupportedSexes (armor is null)");
		return r;
	}
	for (const auto& aa : armo->modelArray) {
		const auto* a = aa.armorAddon;
		if (!a) continue;
		if (modelHasFile(a->bipedModel[0]) || modelHasFile(a->bipedModel1stPerson[0])) r = r | MALE;
		if (modelHasFile(a->bipedModel[1]) || modelHasFile(a->bipedModel1stPerson[1])) r = r | FEMALE;
		if (r == ALL_SEXES) break; // early out
	}
	logger::trace(std::format("< armorSupportedSexes => {:#x}", r));
	return r;
}

static inline std::unordered_set<RE::TESRace*> armorParents(RE::TESRace* race) {
	std::unordered_set<RE::TESRace*> out;
	for (auto* r = race; r; r = r->armorParentRace)
		out.insert(r);
	return out;
}

static inline std::unordered_set<RE::TESRace*> armorSupportedRaces(RE::TESObjectARMO* armo)
{
	logger::trace("> armorSupportedRaces");
	std::unordered_set<RE::TESRace*> out;
	if (!armo) {
		logger::trace("< armorSupportedRaces (null armor)");
		return out;
	}

	if (auto* explicitRace = armo->GetFormRace(); explicitRace) {
		out.insert(explicitRace);
		out.merge(armorParents(explicitRace));
		logger::trace(std::format("< armorSupportedRaces (explicit race: {:#010x})", explicitRace->GetFormID()));
		return out; // treat explicit as a hard restriction
	}

	for (auto& aa : armo->modelArray) {
		auto* arma = aa.armorAddon;
		if (!arma) continue;
		if (arma->GetFormRace()) {
			logger::trace(std::format(": armorSupportedRaces : GetFormRace => {:#010x}", arma->GetFormRace()->GetFormID()));
			RE::TESRace* race = arma->GetFormRace();
			out.insert(race);
			out.merge(armorParents(race));
		}
		for (auto* r : arma->additionalRaces) {
			if (r) {
				logger::trace(std::format(": armorSupportedRaces : additionalRaces => {:#010x}", r->GetFormID()));
				out.insert(r);
				out.merge(armorParents(r));
			}
		}
	}
	logger::trace(std::format("< armorSupportedRaces => {} items", out.size()));
	return out;
}

static inline void armorVectorDedup(std::vector<RE::TESObjectARMO*>& v)
{
	logger::trace(std::format("> armorVectorDedup({})", v.size()));
	std::sort(v.begin(), v.end(),
		[](const RE::TESObjectARMO* a, const RE::TESObjectARMO* b) {
			return a->GetFormID() < b->GetFormID();
		});
	v.erase(std::unique(v.begin(), v.end(),
		[](const RE::TESObjectARMO* a, const RE::TESObjectARMO* b) {
			return a->GetFormID() == b->GetFormID();
		}), v.end());
	logger::trace(std::format("< armorVectorDedup => {}", v.size()));
}

uint32_t Tuple::next_id = 0;

Tuple::Tuple(uint8_t minLevel, bool nsfw, uint32_t sexes, uint32_t occupations, std::vector<RE::TESObjectARMO*> armors) : id(next_id++) {
	this->overrideSexes = sexes;
	this->isNSFW = nsfw;
	this->occupations = occupations;
	this->slots = 0;
	this->minLevel = minLevel;
	size_t size = armors.size();
	logger::trace(std::format("SCSCD: Creating tuple id={} containing {} armors", id, size));
	RE::TESObjectARMO* armor = NULL;
	for (size_t i = 0; i < size; i++) {
		armor = armors[i];
		uint32_t mask = static_cast<RE::BGSBipedObjectForm*>(armor)->bipedModelData.bipedObjectSlots;
		logger::trace(std::format("SCSCD:   > tuple id={} armors[{}]: {}: biped={:#010x}", id, i, armor->GetFullName(), mask));
		this->armors.push_back(armor->GetFormID());
		this->slots = this->slots | mask;
	}
	logger::debug(inspect());
}

/*
 * Returns the set of races defined in ALL armors in this tuple,
 * exclusively. For example if armor A specifies Human and Ghoul
 * but armor B specifies only Ghoul, the result will be [Ghoul].
 */
std::vector<RE::TESRace*> Tuple::possibleRaces() {
	std::vector<RE::TESRace*> races;
	logger::trace(std::format("> Tuple[{}]::possibleRaces() num armors = {}", id, armors.size()));
	for (uint32_t formID : armors) {
		RE::TESForm* form = RE::TESForm::GetFormByID(formID);
		if (!form) {
			logger::warn(std::format("! Tuple[{}]::possibleRaces() form {:#010x} not found (this shouldn't happen!)", id, formID));
			continue;
		}
		RE::TESObjectARMO* armor = form->As<RE::TESObjectARMO>();
		auto armorRaces = armorSupportedRaces(armor);
		races.insert(races.end(), armorRaces.begin(), armorRaces.end());
	}
	raceVectorDedup(races);
	logger::trace(std::format("< Tuple[{}]::possibleRaces() => {} races", id, races.size()));
	return races;
}

/*
 * Returns the bitmap of possible sexes for this Tuple.
 */
uint32_t Tuple::sexes() {
	logger::trace("> Tuple::sexes()");
	if (this->overrideSexes) return this->overrideSexes;
	int sexes = ALL_SEXES;
	for (uint32_t formID : armors) {
		RE::TESForm* form = RE::TESForm::GetFormByID(formID);
		if (form) logger::trace(": Tuple::sexes() armor form found");
		else {
			logger::warn(std::format("! Tuple[{}]::sexes() armor form {:#010x} not found (this shouldn't happen!)", id, formID));
			continue;
		}
		RE::TESObjectARMO* armor = form->As<RE::TESObjectARMO>();
		sexes = sexes & armorSupportedSexes(armor);
	}
	logger::trace("< Tuple::sexes()");
	return sexes;
}

/*
 * Returns the set of sexes compatible with ALL armors in this tuple.
 * For example if armor A specifies MALE | FEMALE but armor B specifies
 * only FEMALE then the result is [FEMALE].
 */
std::vector<uint32_t> Tuple::possibleSexes() {
	uint32_t sexes = this->sexes();
	std::vector<uint32_t> r;
	if (sexes & MALE) r.push_back(MALE);
	if (sexes & FEMALE) r.push_back(FEMALE);
	return r;
}

/*
 * Returns the set of possible occupations compatible with this Tuple.
 * The result is a set of distinct occupations, not a bitmap.
 */
std::vector<uint32_t> Tuple::possibleOccupations() {
	std::vector<uint32_t> occupations;
	uint32_t bit = 1;
	for (int i = 0; i < OCCUPATION_WIDTH; i++) {
		if (this->occupations & bit)
			occupations.push_back(bit);
		bit = bit << 1;
	}
	return occupations;
}

std::string Tuple::inspect() {
	std::string r = std::format("<Tuple id={} races=[", id);
	std::vector<RE::TESRace*> races = this->possibleRaces();
	for (int i = 0; i < races.size(); i++) {
		r += std::format("{:#010x}", races[i]->GetFormID());
		if (i > 0) r += ", ";
	}
	r += std::format("] minLevel={} sexes={:#x} occup={:#010x} nsfw={} slots={:#010x}>", minLevel, sexes(), (uint32_t) occupations, isNSFW ? 1 : 0, slots);
	return r;
}
