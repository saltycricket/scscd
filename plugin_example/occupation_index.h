#pragma once

#include "scscd.h"

class OccupationIndex
{
	/*
	 * FormIDs can represent Class, Faction or NPCs. The registry
	 * maps each of those forms to an occupation. Any given Actor
	 * reference may be assigned any of the Occupations for which
	 * the registered FormIDs are applicable. The actual assigned
	 * occupation for a given Actor will be randomly chosen from
	 * the set of occupation candidates.
	 */
	std::unordered_map<uint32_t, std::vector<Occupation>> registry;

public:
	OccupationIndex() { }

	bool put(uint32_t form, Occupation occupation);
	Occupation sample(RE::Actor* actor);
};
