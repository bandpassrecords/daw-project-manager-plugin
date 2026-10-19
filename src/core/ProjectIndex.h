#pragma once

#include "BridgeProtocol.h"

#include <optional>
#include <vector>

namespace dpm
{

struct IndexedTodo
{
    juce::String id;
    juce::String text;
    bool completed = false;
    juce::Time createdAt;
};

/** One project the app knows about, as written in index.json. */
struct IndexedProject
{
    juce::String id;
    juce::String profileId;
    juce::String profileName;
    juce::String name;
    juce::String filePath;
    juce::String dawType;

    /** The to-dos the app shows for this project. For a version-stack member,
        the app puts the stack's to-dos here: the plugin never needs to know
        about stacks.
    */
    std::vector<IndexedTodo> todos;

    /** The notes the app shows for this project (a stack member's: its stack's). */
    juce::String notes;

    /** The file name without folder or extension ("My Song" for
        "C:\Music\My Song.cpr"), read with either separator whatever the OS.
    */
    juce::String fileStem() const;
    juce::String fileName() const;

    protocol::ProjectRef ref() const { return { id, profileId, name, filePath }; }
    const IndexedTodo* findTodo (const juce::String& todoId) const;
};

/** The newest plugin release the app knows of. The app checks GitHub for it
    (the plugin itself never goes on the network) and leaves it out when it
    doesn't check — update checks off, or no network.
*/
struct CompanionRelease
{
    juce::String version;   // "0.2.0"
    juce::String url;       // the release page
};

/** index.json: what the app tells every plugin instance about its library. */
struct ProjectIndex
{
    int schema = 0;
    juce::Time generatedAt;
    juce::String appVersion;
    std::optional<CompanionRelease> latestRelease;
    std::vector<IndexedProject> projects;

    /** Ids of the plugin messages the app applied lately: how an edit learns
        it arrived, whatever the app made of it.
    */
    juce::StringArray applied;

    const IndexedProject* find (const juce::String& profileId, const juce::String& id) const;
    const IndexedProject* find (const protocol::ProjectRef& ref) const { return find (ref.profileId, ref.id); }

    /** Unknown fields are ignored, so the app can add to the file without
        breaking older plugins. A breaking change gets a new file name.
    */
    static std::optional<ProjectIndex> parse (const juce::String& json, juce::String& error);
};

/** launch.json: the app writes this just before it opens a project in a DAW. */
struct LaunchHint
{
    juce::String projectId;
    juce::String profileId;
    juce::String filePath;
    juce::Time launchedAt;

    static std::optional<LaunchHint> parse (const juce::String& json);
};

/** The last path component of a path written with either separator. */
juce::String fileNameOfAnyPath (const juce::String& path);

} // namespace dpm
