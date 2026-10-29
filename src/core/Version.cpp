#include "Version.h"

#include <array>
#include <optional>

namespace dpm
{

namespace
{
    std::optional<std::array<int, 3>> parse (juce::String v)
    {
        v = v.trim();
        if (v.startsWithIgnoreCase ("v"))
            v = v.substring (1);

        // "1.2.3-beta+5" -> "1.2.3"
        v = v.upToFirstOccurrenceOf ("-", false, false).upToFirstOccurrenceOf ("+", false, false);

        juce::StringArray parts;
        parts.addTokens (v, ".", {});
        if (parts.size() != 3)
            return std::nullopt;

        std::array<int, 3> numbers {};
        for (int i = 0; i < 3; ++i)
        {
            if (parts[i].isEmpty() || ! parts[i].containsOnly ("0123456789"))
                return std::nullopt;
            numbers[(size_t) i] = parts[i].getIntValue();
        }
        return numbers;
    }
}

bool isNewerVersion (const juce::String& remote, const juce::String& local)
{
    const auto r = parse (remote);
    const auto l = parse (local);
    return r && l && *r > *l;
}

bool isReleasePageUrl (const juce::String& url)
{
    // Exactly the releases page, its latest release, or one release's page:
    // nothing a browser could resolve to another page ("../"), and no query.
    const juce::String prefix (releasesUrlPrefix);
    if (url == prefix || url == prefix + "/" || url == prefix + "/latest")
        return true;

    const auto tagPrefix = prefix + "/tag/";
    if (! url.startsWith (tagPrefix))
        return false;

    const auto tag = url.substring (tagPrefix.length());
    return tag.isNotEmpty() && ! tag.contains ("..")
           && tag.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_+");
}

} // namespace dpm
