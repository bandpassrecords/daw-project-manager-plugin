#include "ProjectMatcher.h"

#include <algorithm>

namespace dpm
{

namespace
{
    constexpr int pathScore = 100;
    constexpr int fileNameScore = 95;
    constexpr int nameBaseScore = 60;     // + the name's length, up to 30
    constexpr int partialNameScore = 40;  // + up to 19: a suggestion, never a link
    constexpr int freshLaunchScore = 65;  // the app opened it moments ago
    constexpr int staleLaunchScore = 40;  // earlier today: a suggestion only
    constexpr int launchBonus = 20;
    constexpr int dawBonus = 2;

    const auto freshLaunch = juce::RelativeTime::minutes (15);
    const auto staleLaunch = juce::RelativeTime::hours (12);

    juce::String normalizePath (const juce::String& s)
    {
        return s.replaceCharacter ('\\', '/').toLowerCase();
    }

    bool isWordChar (juce::juce_wchar c)
    {
        return juce::CharacterFunctions::isLetterOrDigit (c);
    }

    /** Whether `name` is a whole part of the title, between the separators
        DAWs put around a project's name ("Cubase Pro Project - <name>",
        "<name> [<path>] - REAPER", "Edit: <name>", an unsaved "<name>*"),
        rather than a word inside a longer name.
    */
    bool isTitlePart (const juce::String& title, const juce::String& name)
    {
        auto t = title.replaceCharacter ((juce::juce_wchar) 0x2013, '-')     // en dash
                      .replaceCharacter ((juce::juce_wchar) 0x2014, '-');    // em dash
        for (auto* separator : { " - ", " | ", ": ", "[", "]", "(", ")" })
            t = t.replace (separator, "\n");

        const auto wanted = name.trim();
        for (auto& part : juce::StringArray::fromLines (t))
            if (part.trim().trimCharactersAtEnd ("*").trim().equalsIgnoreCase (wanted))
                return true;
        return false;
    }

    bool launchPointsAt (const LaunchHint& hint, const IndexedProject& p)
    {
        if (hint.projectId.isNotEmpty())
            return hint.projectId == p.id && hint.profileId == p.profileId;
        return normalizePath (hint.filePath) == normalizePath (p.filePath);
    }
}

bool containsAsWord (const juce::String& haystack, const juce::String& needle)
{
    if (needle.isEmpty())
        return false;

    const auto h = haystack.toLowerCase();
    const auto n = needle.toLowerCase();

    for (int from = 0;;)
    {
        const auto at = h.indexOf (from, n);
        if (at < 0)
            return false;

        const auto end = at + n.length();
        const bool startOk = at == 0 || ! isWordChar (h[at - 1]) || ! isWordChar (n[0]);
        const bool endOk = end >= h.length() || ! isWordChar (h[end]) || ! isWordChar (n[n.length() - 1]);
        if (startOk && endOk)
            return true;

        from = at + 1;
    }
}

std::vector<MatchCandidate> rankProjects (const ProjectIndex& index, const DetectionContext& ctx)
{
    const auto title = normalizePath (ctx.windowTitle);
    const auto hostDaw = dawTypeForHost (ctx.hostName);

    std::optional<juce::RelativeTime> launchAge;
    if (ctx.launch)
        launchAge = ctx.now - ctx.launch->launchedAt;

    std::vector<MatchCandidate> ranked;

    for (auto& p : index.projects)
    {
        MatchCandidate c { &p, 0, MatchReason::titleHasName };

        if (title.isNotEmpty())
        {
            const auto path = normalizePath (p.filePath);
            const auto fileName = p.fileName().toLowerCase();
            const auto stem = p.fileStem();

            if (path.length() > 3 && title.contains (path))
                c = { &p, pathScore, MatchReason::titleHasPath };
            else if (fileName.contains (".") && containsAsWord (title, fileName))
                c = { &p, fileNameScore, MatchReason::titleHasFileName };
            else if (stem.length() >= 2 && containsAsWord (title, stem))
            {
                // Only the title's whole project name links. A word inside a
                // longer one ("Demo" in "Demo Remix", a project the app hasn't
                // scanned yet) is a suggestion: linking would hand that other
                // project its to-dos and work sessions.
                const auto score = isTitlePart (ctx.windowTitle, stem)
                                       ? nameBaseScore + juce::jmin (stem.length(), 30)
                                       : partialNameScore + juce::jmin (stem.length(), 19);
                c = { &p, score, MatchReason::titleHasName };
            }
        }

        // A launch hint ages: a plugin loaded minutes after the app opened
        // this project is in it; one loaded hours later may be in another
        // project opened from inside the DAW since.
        if (launchAge && launchAge->inSeconds() >= -60 && *launchAge <= staleLaunch
            && launchPointsAt (*ctx.launch, p))
        {
            if (c.score > 0)
                c.score += launchBonus;
            else
                c = { &p, *launchAge <= freshLaunch ? freshLaunchScore : staleLaunchScore,
                      MatchReason::launchedFromApp };
        }

        if (c.score > 0)
        {
            if (hostDaw.isNotEmpty() && p.dawType == hostDaw)
                c.score += dawBonus;
            ranked.push_back (c);
        }
    }

    std::stable_sort (ranked.begin(), ranked.end(),
                      [] (const MatchCandidate& a, const MatchCandidate& b) { return a.score > b.score; });
    return ranked;
}

const IndexedProject* confidentMatch (const std::vector<MatchCandidate>& ranked)
{
    if (ranked.empty() || ranked[0].score < confidentScore)
        return nullptr;
    if (ranked.size() > 1 && ranked[1].score == ranked[0].score)
        return nullptr;
    return ranked[0].project;
}

std::vector<PickerRow> buildPickerRows (const ProjectIndex& index, const std::vector<MatchCandidate>& ranked,
                                        const juce::String& query)
{
    juce::StringArray terms;
    terms.addTokens (query.toLowerCase(), " \t", "\"");
    terms.removeEmptyStrings();

    auto matches = [&terms] (const IndexedProject& p)
    {
        const auto haystack = (p.name + " " + p.profileName + " " + p.fileName()).toLowerCase();
        for (auto& t : terms)
            if (! haystack.contains (t))
                return false;
        return true;
    };

    auto row = [] (const IndexedProject& p, bool suggested)
    {
        return PickerRow { p.ref(), p.profileName, p.dawType, suggested };
    };

    std::vector<PickerRow> rows;
    std::vector<const IndexedProject*> shown;

    for (auto& c : ranked)
    {
        if (shown.size() >= maxSuggestions)
            break;
        if (matches (*c.project))
        {
            rows.push_back (row (*c.project, true));
            shown.push_back (c.project);
        }
    }

    std::vector<const IndexedProject*> rest;
    for (auto& p : index.projects)
        if (std::find (shown.begin(), shown.end(), &p) == shown.end() && matches (p))
            rest.push_back (&p);

    std::stable_sort (rest.begin(), rest.end(), [] (const IndexedProject* a, const IndexedProject* b)
    {
        return a->name.compareNatural (b->name) < 0;
    });

    for (auto* p : rest)
        rows.push_back (row (*p, false));

    return rows;
}

juce::String dawTypeForHost (const juce::String& hostName)
{
    const auto h = hostName.toLowerCase();

    struct Rule { const char* needle; const char* dawType; };
    static constexpr Rule rules[] = {
        { "cubase", "Cubase" },
        { "nuendo", "Nuendo" },
        { "ableton", "Ableton Live" },
        { "fl studio", "FL Studio" },
        { "fruity", "FL Studio" },
        { "reaper", "Reaper" },
        { "studio one", "Studio One" },
        { "bitwig", "Bitwig Studio" },
        { "logic", "Logic Pro" },
        { "garageband", "GarageBand" },
        { "pro tools", "Pro Tools" },
        { "waveform", "Waveform" },
        { "tracktion", "Waveform" },
        { "ardour", "Ardour" },
        { "renoise", "Renoise" },
        { "luna", "LUNA" },
        { "sonar", "Sonar" },
        { "cakewalk", "Sonar" },
        { "maschine", "Maschine" },
    };

    for (auto& r : rules)
        if (h.contains (r.needle))
            return r.dawType;
    return {};
}

} // namespace dpm
