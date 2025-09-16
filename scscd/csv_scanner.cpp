#include "scscd.h"

RE::TESForm* FindFormByFormIDOrEditorID(std::string& plugin_file, std::string& idString, RE::ENUM_FORM_ID expectedFormType) {
    RE::TESForm* form = NULL;
    if (isFormIDString(idString)) {
        uint32_t formid = 0;
        if (auto v = hex_to_u32(idString))
            formid = *v;
        else {
            logger::warn(std::string("skipped: could not parse form ID ")
                + idString);
            return NULL;
        }
        logger::trace(std::format("parse formid: {} => {:#10x}", idString, formid));
        // At this point we've parsed a form ID and an occupation.
        // Try to find the formID within the plugin file.
        logger::trace(std::format("lookup formid: {}, {:#10x}", plugin_file, formid));
        form = LookupFormInFile<RE::TESForm>(std::string_view(plugin_file), formid);
        if (form == NULL) {
            logger::error(std::format("skipped: form ID {:#x} could not be found in plugin {}", formid, plugin_file));
            return NULL;
        }
    }
    else {
        // We need to try to deal with potential for editor ID conflicts.
        // map of [expected form type ID, [editor id, form id]]
        // it's scoped this way to reduce vectors for conflicting editor IDs (but they still can - inherent limitation)
        static std::unordered_map<RE::ENUM_FORM_ID, std::unordered_map<std::string, uint32_t>> editorToFormIDs;
        if (editorToFormIDs.empty()) {
            const auto& [map, lock] = RE::TESForm::GetAllForms();
            RE::BSAutoReadLock l{ lock };
            for (auto& entry : *map) {
                std::string edID(entry.second->GetFormEditorID());
                RE::ENUM_FORM_ID formType = entry.second->GetFormType();
                // EditorID may not be known for this form. Seems it is not universally supported.
                if (edID.size() > 0) {
                    if (editorToFormIDs[formType].contains(edID)) {
                        logger::warn(std::format("EDITOR ID CONFLICT: Form mapping for form type {:#06x} already contains editor ID {}; it points to form {:#010x}. The new form, {:#010x}, will be ignored!",
                            (uint32_t)formType, edID, editorToFormIDs[formType][edID], (uint32_t)entry.second->GetFormID()));
                    }
                    else {
                        editorToFormIDs[formType][edID] = entry.first;
                        if (formType == RE::ENUM_FORM_ID::kFACT) {
                            logger::trace(std::format("registered faction form {:#010x} with edid {}", entry.first, edID));
                        }
                    }
                }
            }
        }
        form = RE::TESForm::GetFormByID(editorToFormIDs[expectedFormType][idString]);
        if (form == NULL) {
            return NULL;
        }
    }
    return form;
}
