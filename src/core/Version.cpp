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
    const juce::String prefix (releasesUrlPrefix);
    return url == prefix || url.startsWith (prefix + "/");
}

} // namespace dpm
