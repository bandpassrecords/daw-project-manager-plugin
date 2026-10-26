#pragma once

#include <juce_core/juce_core.h>

namespace dpm
{

/** "My Song_20261018-140233.flac": the project's name (made safe for every
    file system) and the local time it was captured. Projects with no link
    are "Unlinked".
*/
juce::String captureFileName (const juce::String& projectName, juce::Time capturedAt);

/** Whether a file in the captures folder is a take still being written. The
    app skips these.
*/
bool isPartialCapture (const juce::File& f);

/** The name a take is written under while it is being recorded. */
juce::String partialCaptureName (const juce::String& instanceId, int takeNumber);

/** Deletes the takes in `folder` nobody has written to for `maxAge`: left
    behind when a DAW crashed or was killed mid-take. A take still being
    recorded (by this DAW or another sharing the folder) is written to all
    the time, so it is never that old. Only takes: anything else is left
    alone. Returns how many were deleted.
*/
int deleteStaleTakes (const juce::File& folder, juce::Time now,
                      juce::RelativeTime maxAge = juce::RelativeTime::days (1));

} // namespace dpm
