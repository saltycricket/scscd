#include "scscd.h"

#ifdef F4OG
#include "F4SE/Logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#else // NG
#include "F4SE/API.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "REX/W32/OLE32.h"
#include "REX/W32/SHELL32.h"
#include "F4SE/Interfaces.h"
#endif // F4NG

void init_logger()
{
#ifdef F4OG
    std::optional<std::filesystem::path> maybePath = F4SE::log::log_directory();
    if (!maybePath.has_value())
        return; // nothing can be done
    std::filesystem::path path = maybePath.value();
    path /= std::format("My Games/Fallout4/F4SE/SC Smart Clothing Distributor.log");
#else // NG
    wchar_t* knownBuffer{ nullptr };
    const auto                                                     knownResult = REX::W32::SHGetKnownFolderPath(REX::W32::FOLDERID_Documents, REX::W32::KF_FLAG_DEFAULT, nullptr, std::addressof(knownBuffer));
    std::unique_ptr<wchar_t[], decltype(&REX::W32::CoTaskMemFree)> knownPath(knownBuffer, REX::W32::CoTaskMemFree);
    if (!knownPath || knownResult != 0) {
        REX::ERROR("failed to get known folder path");
        return;
    }

    std::filesystem::path path = knownPath.get();
    path /= std::format("My Games/{}/F4SE/SC Smart Clothing Distributor.log", F4SE::GetSaveFolderName()/*, F4SE::GetPluginName()*/);
#endif // F4NG

    // File sink (writes to Documents\My Games\Fallout4\F4SE\MyPlugin.log)
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);

#ifdef _DEBUG
    // Debug sink (shows in Visual Studio Output window)
    auto debugSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

    std::vector<spdlog::sink_ptr> sinks{ fileSink, debugSink };
    auto log = std::make_shared<spdlog::logger>("global", sinks.begin(), sinks.end());
#else
    auto log = std::make_shared<spdlog::logger>("global", fileSink);
#endif

    spdlog::set_default_logger(std::move(log));

    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::warn);

    // flush log every 60 seconds
    std::chrono::milliseconds interval{ 60000 };
    spdlog::flush_every(interval);

    logger::info("Logging initialized.");
}

