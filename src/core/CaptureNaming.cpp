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

    // Long enough to recognise, short enough that the whole path stays well
    // inside Windows' 260 characters.
    base = base.substring (0, 60).trimEnd();

    return base + "_" + capturedAt.formatted ("%Y%m%d-%H%M%S") + ".flac";
}

bool isPartialCapture (const juce::File& f)
{
    return f.getFileName().startsWithChar ('.') || f.hasFileExtension (".part");
}

juce::String partialCaptureName (const juce::String& instanceId, int takeNumber)
{
    return ".take-" + instanceId.removeCharacters ("-").substring (0, 12) + "-" + juce::String (takeNumber)
           + ".flac.part";
}

} // namespace dpm
