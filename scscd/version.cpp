#include "scscd.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 2
#define VERSION_PATCH 0
#define VERSION_TOSTR(a) #a
#define VERSION_TOSTR2(a) VERSION_TOSTR(a)
#define VERSION_STR VERSION_TOSTR2(VERSION_MAJOR) "." VERSION_TOSTR2(VERSION_MINOR) "." VERSION_TOSTR2(VERSION_PATCH)

#ifdef F4OG
#include "F4SE/Version.h"

extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = "SC Smart Clothing Distributor v" VERSION_STR;
	a_info->version = VERSION_MAJOR;

	if (a_f4se->IsEditor()) {
		//logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		//logger::critical("unsupported runtime v{}", ver.string());
		return false;
	}

	return true;
}

#else // NG

extern "C" __declspec(dllexport) constinit auto F4SEPlugin_Version = [] {
	F4SE::PluginVersionData v{};

	// mandatory
	v.PluginVersion(REL::Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH));
	v.PluginName("SC Smart Clothing Distributor v" VERSION_STR);
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

#endif // F4NG
