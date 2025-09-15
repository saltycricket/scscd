#pragma once

#include "scscd.h"

static bool actorIsFemale(RE::Actor* a) {
    if (!a) return false;
#ifdef F4OG
    RE::TESNPC* npc = a->GetNPC();
    if (!npc) return false;
    return npc->GetSex() == 0x01;
#else
    return a->GetSex() == RE::SEX::kFemale;
#endif
}

static const char* GetDisplayFullName(RE::Actor* actor) {
#ifdef F4OG
    return "N/A in OG"; // or I haven't found it yet
#else // NG
    return actor->GetDisplayFullName();
#endif
}

