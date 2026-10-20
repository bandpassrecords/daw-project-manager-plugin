#include "Translations.h"

#include "core/Localization.h"

#include <DPMBinaryData.h>

#include <mutex>

namespace dpm
{

void installTranslations()
{
    static std::once_flag once;
    std::call_once (once, []
    {
        const auto code = translationCodeFor (juce::SystemStats::getUserLanguage());
        if (code.isEmpty())
            return;

        int size = 0;
        if (auto* data = DPMBinaryData::getNamedResource ((code + "_txt").toRawUTF8(), size))
            juce::LocalisedStrings::setCurrentMappings (
                new juce::LocalisedStrings (juce::String::fromUTF8 (data, size), false));
    });
}

} // namespace dpm
