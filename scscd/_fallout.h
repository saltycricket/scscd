#pragma once

#ifdef F4OG

#include "RE/Fallout.h"

#else // F4NG

// Pulls in *ALL* addresses which will fail at runtime if the loaded address
// lib doesn't define any *ONE* of those addresses. Instead we need to opt in
// to addresses one by one by including only the needed headers.
#include "RE/Fallout.h"

// This might not be necessary to do after all, since ng isn't binary compatible with og anyway.
//#include "F4SE/Impl/PCH.h"
//#include "RE/A/Actor.h"
//#include "RE/A/ActorEquipManager.h"
//#include "RE/B/BGSActorCellEvent.h"
//#include "RE/B/BGSMaterialSwap.h"
//#include "RE/B/BGSObjectInstanceExtra.h"
//#include "RE/B/BIPED_OBJECT.h"
//#include "RE/B/BSCoreTypes.h"
//#include "RE/B/BSResourceNiBinaryStream.h"
//#include "RE/B/BSTSmartPointer.h"
//#include "RE/E/ENUM_FORM_ID.h"
//#include "RE/E/ExtraDataList.h"
//#include "RE/E/ExtraInstanceData.h"
//#include "RE/E/ExtraMaterialSwap.h"
//#include "RE/T/TESClass.h"
//#include "RE/T/TESDataHandler.h"
//#include "RE/T/TESFaction.h"
//#include "RE/T/TESNPC.h"
//#include "RE/T/TESObjectARMA.h"
//#include "RE/T/TESObjectARMO.h"
//#include "RE/T/TESObjectLoadedEvent.h"
//#include "RE/T/TESObjectMISC.h"
//#include "RE/T/TESObjectREFR.h"
//#include "RE/T/TESRace.h"
//
//#include "_TESFormUtil.h"

#endif // F4NG

#include "_shim.h"
