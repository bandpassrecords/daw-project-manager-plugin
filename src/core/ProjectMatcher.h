#pragma once

#include "ProjectIndex.h"

#include <optional>
#include <vector>

namespace dpm
{

/** What the plugin can observe about the project it was loaded into. */
struct DetectionContext
{
    /** The host's own description, e.g. "Steinberg Cubase 13". */
    juce::String hostName;

    /** The DAW's main window title, which every major DAW fills with the
        open project's name ("Cubase Pro Project - My Song", "My Song.flp - FL
        Studio 21", "My Song [C:\…\My Song.rpp] - REAPER v7"). Empty where the
        platform can't read it.
    */
    juce::String windowTitle;

    std::optional<LaunchHint> launch;
    juce::Time now;
};

enum class MatchReason
{
    titleHasPath,
    titleHasFileName,
    titleHasName,
    launchedFromApp
};

struct MatchCandidate
{
    const IndexedProject* project = nullptr;
    int score = 0;
    MatchReason reason = MatchReason::titleHasName;
};

/** Every project that the context points at, best first. Projects with no
    evidence at all are left out.
*/
std::vector<MatchCandidate> rankProjects (const ProjectIndex& index, const DetectionContext& context);

/** The candidate to link to without asking, or nullptr when the evidence is
    weak or two projects score the same.
*/
const IndexedProject* confidentMatch (const std::vector<MatchCandidate>& ranked);

/** Score at or above which a match links without asking. */
inline constexpr int confidentScore = 60;

/** The app's DAW type name ("Cubase", "Ableton Live", …, as
    MetadataExtractor writes it) for a host description, or empty.
*/
juce::String dawTypeForHost (const juce::String& hostName);

/** Whether `needle` occurs in `haystack` with no letter or digit directly
    on either side. Case-insensitive.
*/
bool containsAsWord (const juce::String& haystack, const juce::String& needle);

/** One row of the project picker. A copy, not a pointer into the index: the
    index is replaced whenever the app rewrites it.
*/
struct PickerRow
{
    protocol::ProjectRef ref;
    juce::String profileName;
    juce::String dawType;
    bool suggested = false;
};

/** The picker's list: the best few suggestions first, then every other
    project by name. A query keeps only projects whose name, profile or file
    name contain every word of it.
*/
std::vector<PickerRow> buildPickerRows (const ProjectIndex& index, const std::vector<MatchCandidate>& ranked,
                                        const juce::String& query);

inline constexpr size_t maxSuggestions = 3;

} // namespace dpm
