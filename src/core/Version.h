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

/** Whether `url` is the plugin's releases page, its latest release, or one
    release's page (/releases/tag/<tag>). Nothing else: the plugin opens it
    in the browser, so a link that resolves elsewhere ("../") is refused.
*/
bool isReleasePageUrl (const juce::String& url);

} // namespace dpm
