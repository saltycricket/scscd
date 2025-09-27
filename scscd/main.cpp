#include "scscd.h"

#include "F4SE/API.h"
#include "F4SE/Interfaces.h"
#include <random>

using namespace std;
using namespace RE;
using namespace RE::BSScript;

static OccupationIndex OCCUPATIONS;
static ArmorIndex ARMORS(&OCCUPATIONS);
static ArmorIndex::SamplerConfig SAMPLER_CONFIG;

//extern "C" void __sanitizer_set_report_path(const char* path);

void _d(int line) {
	std::string str = std::format("Line: {}", line);
	MessageBox(NULL, str.c_str(), "SCSCD Debug", 0L);
}

extern "C" __declspec(dllexport) bool F4SEPlugin_Load(const F4SE::LoadInterface* iface) {
	//__sanitizer_set_report_path("C:\\Users\\Colin2\\Documents\\scscd_asan.txt");

	// Init logger before anything else happens. Note that F4SE::Init inits
	// F4SE first and logger second. I want logger first in case F4SE fails.
	if (!init_logger()) {
		MessageBox(NULL, "Failed to initialize logger!", "SCSCD Init Failed", 0L);
		return false;
	}

	try {
#ifdef F4OG
		F4SE::Init(iface);
#else // F4NG
		struct F4SE::InitInfo info = {
			.log = false
		};
		F4SE::Init(iface, info);
#endif // F4NG
		logger::info("SCSCD initializing");
		ActorLoadWatcher::configure(&ARMORS, &SAMPLER_CONFIG);

		if (!SAMPLER_CONFIG.load(DataPath("MCM\\Settings\\SC_Smart_Clothing_Distributor.ini"), DataPath("MCM\\Config\\SC_Smart_Clothing_Distributor\\settings.ini"))) {
			MessageBox(NULL, "SCSCD: Failed to load configuration!", "SCSCD Init Failed!", 0x0L /* OK */);
			return false;
		}

		const F4SE::MessagingInterface* msg = F4SE::GetMessagingInterface();
		if (!msg) {
			logger::error("SCSCD: Init failed! Messaging interface is not available.");
			MessageBox(NULL, "SCSCD: Init failed! F4SE messaging interface is not available.", "SCSCD Init Failed!", 0x00000000L /* OK */);
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
					logger::info("SCSCD indexing forms");
					// might be tempting to scan earlier on but it's safer to wait for game data
					// because we don't know if the game is might be loading plugins in a separate
					// thread, and we must not risk contesting an open file.
					ArmorIndex::indexAllFormsByTypeAndEdid();
					logger::info("SCSCD scanning CSV files");
					scan_occupations_csv(DataPath("F4SE\\Plugins\\scscd\\occupation"), OCCUPATIONS);
					scan_tuples_csv(DataPath("F4SE\\Plugins\\scscd\\clothing"), false, ARMORS);
					scan_exclusions_csv(DataPath("F4SE\\Plugins\\scscd\\exclusions"), ActorLoadWatcher::exclusionList);
					ActorLoadWatcher::watchForSettingsChanges();
				}
				// Register listener here so we can pre-empt any actors which are loaded
				// as part of savegame restore.
				logger::trace("Beginning registration");
				ActorLoadWatcher::GetSingleton()->Register();
				break;
			case F4SE::MessagingInterface::kPostLoadGame:
			case F4SE::MessagingInterface::kNewGame:
				break;
			default:
				break;
			}
		});

		logger::info("SCSCD is ready");
		return true;
	}
	catch (std::exception const &exc) {
		MessageBox(NULL, "Fatal error!", "Fatal error!", 0L);
		logger::critical(std::format("Fatal error! {}", exc.what()));
		return false;
	}
}
