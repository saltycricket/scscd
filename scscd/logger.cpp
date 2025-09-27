#include "scscd.h"

#define UNICODE
#define _UNICODE
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <filesystem>
#include <memory>

bool init_logger()
{
    wchar_t* knownBuffer{ nullptr };
    const auto knownResult = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, std::addressof(knownBuffer));
    std::unique_ptr<wchar_t[], decltype(&CoTaskMemFree)> knownPath(knownBuffer, CoTaskMemFree);
    if (!knownPath || knownResult != 0) {
        return false;
    }

    std::filesystem::path path = knownPath.get();
    path /= "My Games/Fallout4/F4SE/SC Smart Clothing Distributor.log";
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
    return true;
}

