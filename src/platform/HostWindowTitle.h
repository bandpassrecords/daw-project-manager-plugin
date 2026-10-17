#pragma once

#include <juce_core/juce_core.h>

namespace dpm
{

/** The title of the host's main window, which every major DAW fills with the
    open project's name. On macOS the window's represented file, when the
    host sets one, is appended in brackets so its full path can match.

    Empty where it can't be read (Linux, or no window yet). Message thread only.
*/
juce::String readHostWindowTitle();

/** Titles of this plugin's own windows, which must never be taken for the
    host's (some hosts open plugin editors as top-level windows).
*/
bool isOwnWindowTitle (const juce::String& title);

} // namespace dpm
