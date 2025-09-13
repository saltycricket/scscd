#include "scscd.h"
#include "omod_index.h"

//#include "common/ITypes.h"                          // UInt8, UInt32 needed by PluginAPI.h
//#include "dep/f4se/f4se_common/f4se_version.h"		// RUNTIME_VERSION_1_10_984
//#include "dep/f4se/f4se/PluginAPI.h"                // F4SEPluginVersionData
#include "F4SE/API.h"
#include "F4SE/Interfaces.h"
#include <random>

using namespace std;
using namespace RE;
using namespace RE::BSScript;

static OccupationIndex OCCUPATIONS;
static ArmorIndex ARMORS(&OCCUPATIONS);
static ArmorIndex::SamplerConfig SAMPLER_CONFIG;

extern "C" __declspec(dllexport) constinit auto F4SEPlugin_Version = [] {
	F4SE::PluginVersionData v{};

	// mandatory
	v.PluginVersion(REL::Version(1, 0, 0));
	v.PluginName("SC Smart Clothing Distributor");
	v.AuthorName("saltycricket");

	// compatibility flags
	v.UsesSigScanning(false);
	v.UsesAddressLibrary(true);
	v.HasNoStructUse(false);
	v.IsLayoutDependent(true);

	// optional: minimum runtime requirements
	v.CompatibleVersions({ REL::Version(1, 10, 984) });
	v.MinimumRequiredXSEVersion(REL::Version(0, 7, 2));

	return v;
}();

//typedef void (*Fbool)(IVirtualMachine&, std::uint32_t, std::monostate, bool v);
//typedef void (*Ffloat)(IVirtualMachine&, std::uint32_t, std::monostate, float v);
//typedef void (*Fint)(IVirtualMachine&, std::uint32_t, std::monostate, int v);
//
//static void F4SEAPI setChangeOutfitChance(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setChangeOutfitChance(v); }
//static void F4SEAPI setAllowNSFW(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, bool v) { SAMPLER_CONFIG.setAllowNSFW(v); }
//static void F4SEAPI setAllowNudity(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, bool v) { SAMPLER_CONFIG.setAllowNudity(v); }
//static void F4SEAPI setProximityBias(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, float v) { SAMPLER_CONFIG.setProximityBias(v); }
//static void F4SEAPI setSkipSlotChance_0(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(0, v); }
//static void F4SEAPI setSkipSlotChance_1(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(1, v); }
//static void F4SEAPI setSkipSlotChance_2(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(2, v); }
//static void F4SEAPI setSkipSlotChance_3(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(3, v); }
//static void F4SEAPI setSkipSlotChance_4(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(4, v); }
//static void F4SEAPI setSkipSlotChance_5(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(5, v); }
//static void F4SEAPI setSkipSlotChance_6(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(6, v); }
//static void F4SEAPI setSkipSlotChance_7(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(7, v); }
//static void F4SEAPI setSkipSlotChance_8(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(8, v); }
//static void F4SEAPI setSkipSlotChance_9(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(9, v); }
//static void F4SEAPI setSkipSlotChance_10(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(10, v); }
//static void F4SEAPI setSkipSlotChance_11(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(11, v); }
//static void F4SEAPI setSkipSlotChance_12(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(12, v); }
//static void F4SEAPI setSkipSlotChance_13(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(13, v); }
//static void F4SEAPI setSkipSlotChance_14(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(14, v); }
//static void F4SEAPI setSkipSlotChance_15(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(15, v); }
//static void F4SEAPI setSkipSlotChance_16(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(16, v); }
//static void F4SEAPI setSkipSlotChance_17(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(17, v); }
//static void F4SEAPI setSkipSlotChance_18(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(18, v); }
//static void F4SEAPI setSkipSlotChance_19(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(19, v); }
//static void F4SEAPI setSkipSlotChance_20(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(20, v); }
//static void F4SEAPI setSkipSlotChance_21(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(21, v); }
//static void F4SEAPI setSkipSlotChance_22(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(22, v); }
//static void F4SEAPI setSkipSlotChance_23(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(23, v); }
//static void F4SEAPI setSkipSlotChance_24(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(24, v); }
//static void F4SEAPI setSkipSlotChance_25(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(25, v); }
//static void F4SEAPI setSkipSlotChance_26(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(26, v); }
//static void F4SEAPI setSkipSlotChance_27(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(27, v); }
//static void F4SEAPI setSkipSlotChance_28(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(28, v); }
//static void F4SEAPI setSkipSlotChance_29(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(29, v); }
//static void F4SEAPI setSkipSlotChance_30(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(30, v); }
//static void F4SEAPI setSkipSlotChance_31(IVirtualMachine& vm, std::uint32_t stackID, std::monostate, int v) { SAMPLER_CONFIG.setSkipSlotChance(31, v); }

static bool F4SEAPI registerPapyrusFns(RE::BSScript::IVirtualMachine* vm) {
	/*vm->BindNativeMethod<Fint>("SC:SCD", "name", setChangeOutfitChance, false, false);
	vm->BindNativeMethod<Fbool>("SC:SCD", "name", setAllowNSFW, false, false);
	vm->BindNativeMethod<Fbool>("SC:SCD", "name", setAllowNudity, false, false);
	vm->BindNativeMethod<Ffloat>("SC:SCD", "name", setProximityBias, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_0", setSkipSlotChance_0, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_1", setSkipSlotChance_1, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_2", setSkipSlotChance_2, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_3", setSkipSlotChance_3, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_4", setSkipSlotChance_4, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_5", setSkipSlotChance_5, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_6", setSkipSlotChance_6, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_7", setSkipSlotChance_7, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_8", setSkipSlotChance_8, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_9", setSkipSlotChance_9, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_10", setSkipSlotChance_10, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_11", setSkipSlotChance_11, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_12", setSkipSlotChance_12, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_13", setSkipSlotChance_13, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_14", setSkipSlotChance_14, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_15", setSkipSlotChance_15, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_16", setSkipSlotChance_16, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_17", setSkipSlotChance_17, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_18", setSkipSlotChance_18, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_19", setSkipSlotChance_19, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_20", setSkipSlotChance_20, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_21", setSkipSlotChance_21, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_22", setSkipSlotChance_22, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_23", setSkipSlotChance_23, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_24", setSkipSlotChance_24, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_25", setSkipSlotChance_25, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_26", setSkipSlotChance_26, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_27", setSkipSlotChance_27, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_28", setSkipSlotChance_28, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_29", setSkipSlotChance_29, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_30", setSkipSlotChance_30, false, false);
	vm->BindNativeMethod<Fint>("SC:SCD", "setSkipSlotChance_31", setSkipSlotChance_31, false, false);*/
	return true;
}

extern "C" __declspec(dllexport) bool F4SEPlugin_Load(const F4SE::LoadInterface* iface) {
	struct F4SE::InitInfo info = {
		.log = false
	};
	F4SE::Init(iface, info);
	init_logger();
	logger::info("SCSCD initializing");
	ActorLoadWatcher::configure(&ARMORS, &SAMPLER_CONFIG);

	if (!F4SE::GetTaskInterface()) {
		logger::error("SCSCD: Init failed! Task interface is not available.");
		return false;
	}

	if (auto pap = F4SE::GetPapyrusInterface()) {
		pap->Register(registerPapyrusFns);
	}

	const F4SE::MessagingInterface* msg = F4SE::GetMessagingInterface();
	if (!msg) {
		logger::error("SCSCD: Init failed! Messaging interface is not available.");
		return false;
	}
	msg->RegisterListener([](F4SE::MessagingInterface::Message* msg) {
		if (!msg) {
			logger::warn("message is nil, thats not good at all");
			return;
		}
		logger::trace(std::format("message received: type {}", msg->type));
		switch (msg->type) {
		case F4SE::MessagingInterface::kGameDataReady:
			logger::trace("message is kGameDataReady");
			if ((bool)msg->data) { // false before data, true after data
				SAMPLER_CONFIG.load(DataPath("MCM\\Settings\\SC_Smart_Clothing_Distributor.ini"), DataPath("MCM\\Config\\SC_Smart_Clothing_Distributor\\settings.ini"));
				OmodIndex::Instance().BuildFromAllForms();
				logger::info("SCSCD scanning CSV files");
				// Register occupations at CSV files :
				scan_occupations_csv(DataPath("F4SE\\Plugins\\scscd\\occupation"), OCCUPATIONS);
				// Register tuples at CSV files :
				scan_tuples_csv(DataPath("F4SE\\Plugins\\scscd\\clothing\\nsfw"), true, ARMORS);
				scan_tuples_csv(DataPath("F4SE\\Plugins\\scscd\\clothing\\sfw"), false, ARMORS);
				// Register matswaps :
				//scan_matswaps_csv(DataPath("F4SE\\Plugins\\scscd\\matswaps\\nsfw"), true, ARMORS);
				//scan_matswaps_csv(DataPath("F4SE\\Plugins\\scscd\\matswaps\\sfw"), false, ARMORS);
				// Register omods :
				scan_omods_csv(DataPath("F4SE\\Plugins\\scscd\\omods\\nsfw"), true, ARMORS);
				scan_omods_csv(DataPath("F4SE\\Plugins\\scscd\\omods\\sfw"), false, ARMORS);
				ActorLoadWatcher::watchForSettingsChanges();
			}
			// Register listener here so we can pre-empt any actors which are loaded
			// as part of savegame restore.
			ActorLoadWatcher::GetSingleton()->Register();
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			logger::info("SCSCD starting listeners");
			/*ActorLoadWatcher::GetSingleton(&ARMORS, &SAMPLER_CONFIG)->Register();*/
			break;
		default:
			break;
		}
	});

	logger::info("SCSCD is ready");
	return true;
}
