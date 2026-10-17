#include "Outbox.h"

#include "BridgeProtocol.h"

#include <atomic>

namespace dpm
{

Outbox::Outbox (juce::File directory) : dir (std::move (directory)) {}

juce::String outboxFileName (juce::Time createdAt, juce::uint32 sequence, const juce::String& messageId)
{
    // "2026-10-18T01:09:00.123Z" -> "20261018T010900123Z"
    const auto stamp = protocol::formatTime (createdAt).removeCharacters ("-:.");
    const auto idPart = messageId.removeCharacters ("-").substring (0, 8).toLowerCase();
    return stamp + "-" + juce::String (sequence % 1000000).paddedLeft ('0', 6) + "-" + idPart + ".json";
}

juce::File Outbox::write (const juce::String& json, juce::Time createdAt, const juce::String& messageId)
{
    // Shared by every instance in this process, so two instances writing in
    // the same millisecond still sort in the order they wrote.
    static std::atomic<juce::uint32> sequence { 0 };

    if (! dir.createDirectory())
        return {};

    const auto target = dir.getChildFile (outboxFileName (createdAt, sequence++, messageId));
    const auto temp = target.withFileExtension (".tmp");

    if (! temp.replaceWithText (json, false, false, "\n"))
    {
        temp.deleteFile();
        return {};
    }

    if (! temp.moveFileTo (target))
    {
        temp.deleteFile();
        return {};
    }

    return target;
}

} // namespace dpm
