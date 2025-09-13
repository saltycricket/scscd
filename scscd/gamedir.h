#pragma once
#include <filesystem>
#include <system_error>
#include <windows.h>

namespace fs = std::filesystem;

static fs::path GetGameRootDir()
{
	std::wstring buf(MAX_PATH, L'\0');

	for (;;) {
		DWORD n = ::GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
		if (n == 0) {
			throw std::system_error(static_cast<int>(::GetLastError()),
				std::system_category(),
				"GetModuleFileNameW failed");
		}
		if (n < buf.size() - 1) {
			buf.resize(n);
			break;
		}
		// buffer was too small; grow and retry
		buf.resize(buf.size() * 2);
	}

	return fs::path(buf).parent_path(); // e.g. ...\Fallout 4
}

static fs::path GetDataDir()
{
	return GetGameRootDir() / L"Data";
}

static fs::path DataPath(fs::path relative) // convenience
{
	return GetDataDir() / relative;
}
