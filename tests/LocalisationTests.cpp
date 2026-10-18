#include "core/Localisation.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace dpm;

namespace
{
    const juce::File sourceDir (DPM_SOURCE_DIR);

    /** Every string the code passes to TRANS(). */
    std::set<juce::String> translatedStringsInCode()
    {
        std::set<juce::String> keys;

        for (auto& f : sourceDir.getChildFile ("src").findChildFiles (juce::File::findFiles, true, "*.cpp;*.h;*.mm"))
        {
            const auto text = f.loadFileAsString();
            for (int at = text.indexOf ("TRANS ("); at >= 0; at = text.indexOf (at + 1, "TRANS ("))
            {
                const auto open = text.indexOfChar (at, '"');
                juce::String key;
                for (int i = open + 1; i < text.length() && text[i] != '"'; ++i)
                {
                    if (text[i] == '\\' && i + 1 < text.length())
                        ++i;
                    key << juce::String::charToString (text[i]);
                }
                keys.insert (key);
            }
        }
        return keys;
    }
}

TEST_CASE ("System languages map to the shipped translations", "[l10n]")
{
    CHECK (translationCodeFor ("pt-BR") == "pt");
    CHECK (translationCodeFor ("pt_PT") == "pt");     // the one Portuguese translation is Brazilian
    CHECK (translationCodeFor ("zh_Hans") == "zh");
    CHECK (translationCodeFor ("DE") == "de");
    CHECK (translationCodeFor ("ja") == "ja");
    CHECK (translationCodeFor ("en-GB") == "");
    CHECK (translationCodeFor ("ko") == "");
    CHECK (translationCodeFor ("") == "");
}

TEST_CASE ("The code's UI strings were found", "[l10n]")
{
    const auto keys = translatedStringsInCode();
    CHECK (keys.size() > 30);
    CHECK (keys.count ("To-dos") == 1);
}

TEST_CASE ("Every translation has exactly the code's UI strings", "[l10n]")
{
    // The plugin ships in the app's nine languages: a string missing here
    // shows up in English for every user of that language.
    const auto keys = translatedStringsInCode();

    for (auto& code : shippedTranslationCodes())
    {
        const auto file = sourceDir.getChildFile ("resources/translations/" + code + ".txt");
        INFO (file.getFullPathName().toStdString());
        REQUIRE (file.existsAsFile());

        const juce::LocalisedStrings strings (file.loadFileAsString(), false);
        const auto& mappings = strings.getMappings();

        for (auto& key : keys)
        {
            INFO (code.toStdString() << ": missing \"" << key.toStdString() << "\"");
            CHECK (mappings.containsKey (key));
            CHECK (mappings[key].isNotEmpty());
        }

        for (auto& key : mappings.getAllKeys())
        {
            INFO (code.toStdString() << ": \"" << key.toStdString() << "\" is no longer used in the code");
            CHECK (keys.count (key) == 1);
        }
    }
}

TEST_CASE ("Placeholders survive translation", "[l10n]")
{
    for (auto& code : shippedTranslationCodes())
    {
        const juce::LocalisedStrings strings (
            sourceDir.getChildFile ("resources/translations/" + code + ".txt").loadFileAsString(), false);
        const auto& mappings = strings.getMappings();

        for (auto& key : mappings.getAllKeys())
            for (auto* placeholder : { "{n}", "{name}", "{time}", "{max}", "{length}", "{version}" })
                if (key.contains (placeholder))
                {
                    INFO (code.toStdString() << ": " << key.toStdString());
                    CHECK (mappings[key].contains (placeholder));
                }
    }
}

TEST_CASE ("The Portuguese translation is Brazilian", "[l10n]")
{
    // CLAUDE.md: one Portuguese locale, and it targets Brazil.
    const auto pt = sourceDir.getChildFile ("resources/translations/pt.txt").loadFileAsString().toLowerCase();
    for (auto* european : { "ficheiro", "ecrã", "utilizador", "descarregar", "deteção", "a gravar" })
    {
        INFO (european);
        CHECK_FALSE (pt.contains (european));
    }
}
