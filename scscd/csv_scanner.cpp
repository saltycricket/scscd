#include "scscd.h"
#include "armor_index.h"

RE::TESForm* FindFormByFormIDOrEditorID(std::string& plugin_file, std::string& idString, RE::ENUM_FORM_ID expectedFormType, bool logOnMissing) {
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
            if (logOnMissing)
                logger::error(std::format("skipped: form ID {:#x} could not be found in plugin {}", formid, plugin_file));
            return NULL;
        }
    }
    else {
        uint32_t formID = ArmorIndex::getFormByTypeAndEdid(expectedFormType, idString, logOnMissing);
        if (formID == 0) {
            if (logOnMissing)
                logger::error(std::format("skipped: form editor ID {} could not be resolved to a form ID!", idString));
            return NULL;
        }
        form = RE::TESForm::GetFormByID(formID);
        if (form == NULL) {
            if (logOnMissing)
                logger::error(std::format("skipped: form editor ID {} was resolved to form ID {:#010x} but the form itself could not be found!", idString, formID));
            return NULL;
        }
    }
    return form;
}
