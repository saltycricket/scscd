#pragma once

#include <string>
#include <functional>
#include "_fallout.h"

void IndexOneTESFile(RE::TESFile* f, const std::function<bool (RE::TESFile* file, RE::ENUM_FORM_ID formType, const std::string& edid, uint32_t formID)>& cb);
