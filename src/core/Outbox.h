#pragma once

#include <juce_core/juce_core.h>

namespace dpm
{

/** The folder the plugin leaves messages in for the app.

    Each message is its own file, written to "<name>.tmp" and renamed when
    complete, so the app never reads half a message. Names sort in the order
    the messages were written, which is the order the app applies them in.
*/
class Outbox
{
public:
    explicit Outbox (juce::File directory);

    /** Writes one message. Returns the file written, or a default File when
        the folder can't be written (the caller keeps the message and retries).
    */
    juce::File write (const juce::String& json, juce::Time createdAt, const juce::String& messageId);

    const juce::File& directory() const { return dir; }

private:
    juce::File dir;
};

/** "20261018T010900123Z-000042-1a2b3c4d.json": UTC time, then a sequence
    number for messages written in the same millisecond, then part of the
    message id against two instances colliding.
*/
juce::String outboxFileName (juce::Time createdAt, juce::uint32 sequence, const juce::String& messageId);

} // namespace dpm
