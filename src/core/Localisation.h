#pragma once

#include <juce_core/juce_core.h>

namespace dpm
{

/** The languages the plugin ships translations for: the app's nine, minus
    English, which is the text in the code. Each has resources/translations/<code>.txt.
*/
const juce::StringArray& shippedTranslationCodes();

/** The translation to use for a system language such as "pt-BR" or
    "zh_Hans". Empty for English and for languages without a translation.
*/
juce::String translationCodeFor (const juce::String& systemLanguage);

} // namespace dpm
