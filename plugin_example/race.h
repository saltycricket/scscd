#pragma once

#include "scscd.h"

static inline void raceVectorDedup(std::vector<RE::TESRace*>& v)
{
    std::sort(v.begin(), v.end(),
        [](const RE::TESRace* a, const RE::TESRace* b) {
            return a->GetFormID() < b->GetFormID();
        });
    v.erase(std::unique(v.begin(), v.end(),
        [](const RE::TESRace* a, const RE::TESRace* b) {
            return a->GetFormID() == b->GetFormID();
        }), v.end());
}
