#pragma once

#include <juce_core/juce_core.h>

namespace dpm
{

/** Whether `remote` is a strictly newer x.y.z version than `local`. A
    leading "v" and anything after the third number ("-beta", "+build") are
    ignored; a version that isn't x.y.z is never newer.
*/
bool isNewerVersion (const juce::String& remote, const juce::String& local);

/** Where the plugin's releases are published. An update link from the app
    is only ever opened when it points here.
*/
inline constexpr const char* releasesUrlPrefix = "https://github.com/bandpassrecords/daw-project-manager-plugin/releases";

/** Whether `url` is on the plugin's releases page: the page itself or
    anything under it, never a page that merely starts with the same text.
*/
bool isReleasePageUrl (const juce::String& url);

} // namespace dpm
