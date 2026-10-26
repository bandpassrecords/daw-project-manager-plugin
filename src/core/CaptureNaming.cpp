#include "CaptureNaming.h"

namespace dpm
{

juce::String captureFileName (const juce::String& projectName, juce::Time capturedAt)
{
    auto base = juce::File::createLegalFileName (projectName.trim())
                    .removeCharacters ("\\/:*?\"<>|")
                    .trimCharactersAtEnd (". ")
                    .trimCharactersAtStart (". ");
    if (base.isEmpty())
        base = "Unlinked";

    // Long enough to recognize, short enough that the whole path stays well
    // inside Windows' 260 characters.
    base = base.substring (0, 60).trimEnd();

    return base + "_" + capturedAt.formatted ("%Y%m%d-%H%M%S") + ".flac";
}

bool isPartialCapture (const juce::File& f)
{
    return f.getFileName().startsWithChar ('.') || f.hasFileExtension (".part");
}

int deleteStaleTakes (const juce::File& folder, juce::Time now, juce::RelativeTime maxAge)
{
    int deleted = 0;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, ".take-*.part"))
        if (f.getLastModificationTime() < now - maxAge && f.deleteFile())
            ++deleted;
    return deleted;
}

juce::String partialCaptureName (const juce::String& instanceId, int takeNumber)
{
    return ".take-" + instanceId.removeCharacters ("-").substring (0, 12) + "-" + juce::String (takeNumber)
           + ".flac.part";
}

} // namespace dpm
