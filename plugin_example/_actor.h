#ifndef ACTOR_H
#define ACTOR_H

//#include "scscd.h"
//#include <vector>
//
//// bool Actor_IsInFaction(Actor* a, TESFaction* fac);
//using _Actor_IsInFaction = bool (*)(Actor*, TESFaction*);
//extern REL::Relocation<_Actor_IsInFaction> Actor_IsInFaction;
//
//// Returns 1 = male, 2 = female, any other value = unknown (no NPC base).
//static inline int GetActorSex(Actor* a)
//{
//    if (!a) return -1;
//    TESNPC* npc = nullptr;
//    if (!npc) npc = DYNAMIC_CAST(a->baseForm, TESForm, TESNPC);
//    if (!npc) return -1;
//    const bool isFemale = (npc->actorData.flags & TESActorBaseData::kFlagFemale) != 0;
//    return isFemale ? FEMALE : MALE;
//}
//
//static inline bool IsInFaction(Actor* a, TESFaction* fac)
//{
//    //if (!a || !fac) return false;
//    //return Actor_IsInFaction(a, fac);
//}

#endif // ACTOR_H
