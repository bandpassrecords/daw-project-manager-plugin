#include "Localization.h"

namespace dpm
{

const juce::StringArray& shippedTranslationCodes()
{
    static const juce::StringArray codes { "de", "es", "fr", "it", "ja", "pt", "ru", "zh" };
    return codes;
}

juce::String translationCodeFor (const juce::String& systemLanguage)
{
    const auto code = systemLanguage.trim().substring (0, 2).toLowerCase();
    return shippedTranslationCodes().contains (code) ? code : juce::String();
}

} // namespace dpm
