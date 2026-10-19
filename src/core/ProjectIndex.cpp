#include "ProjectIndex.h"

namespace dpm
{

juce::String fileNameOfAnyPath (const juce::String& path)
{
    auto p = path.trim();
    while (p.endsWithChar ('/') || p.endsWithChar ('\\'))
        p = p.dropLastCharacters (1);

    const auto lastSep = juce::jmax (p.lastIndexOfChar ('/'), p.lastIndexOfChar ('\\'));
    return p.substring (lastSep + 1);
}

juce::String IndexedProject::fileName() const
{
    return fileNameOfAnyPath (filePath);
}

juce::String IndexedProject::fileStem() const
{
    const auto file = fileName();
    const auto dot = file.lastIndexOfChar ('.');
    return dot > 0 ? file.substring (0, dot) : file;
}

const IndexedTodo* IndexedProject::findTodo (const juce::String& todoId) const
{
    for (auto& t : todos)
        if (t.id == todoId)
            return &t;
    return nullptr;
}

const IndexedProject* ProjectIndex::find (const juce::String& profileId, const juce::String& id) const
{
    if (id.isEmpty())
        return nullptr;

    for (auto& p : projects)
        if (p.id == id && p.profileId == profileId)
            return &p;
    return nullptr;
}

namespace
{
    juce::Time timeOr (const juce::var& v, juce::Time fallback = {})
    {
        if (auto t = protocol::parseTime (v.toString()))
            return *t;
        return fallback;
    }

    IndexedTodo parseTodo (const juce::var& v)
    {
        IndexedTodo t;
        t.id = v["id"].toString();
        t.text = v["text"].toString();
        t.completed = (bool) v["completed"];
        t.createdAt = timeOr (v["createdAt"]);
        return t;
    }

    IndexedProject parseProject (const juce::var& v)
    {
        IndexedProject p;
        p.id = v["id"].toString();
        p.profileId = v["profileId"].toString();
        p.profileName = v["profileName"].toString();
        p.name = v["name"].toString();
        p.filePath = v["filePath"].toString();
        p.dawType = v["dawType"].toString();
        p.notes = v["notes"].toString();

        if (auto* todos = v["todos"].getArray())
            for (auto& t : *todos)
                if (t.isObject() && t["id"].toString().isNotEmpty())
                    p.todos.push_back (parseTodo (t));

        if (p.name.isEmpty())
            p.name = p.fileStem();
        return p;
    }
}

std::optional<ProjectIndex> ProjectIndex::parse (const juce::String& json, juce::String& error)
{
    juce::var root;
    const auto result = juce::JSON::parse (json, root);
    if (result.failed())
    {
        error = result.getErrorMessage();
        return std::nullopt;
    }
    if (! root.isObject())
    {
        error = "index.json is not a JSON object";
        return std::nullopt;
    }

    ProjectIndex index;
    index.schema = (int) root["schema"];
    if (index.schema < 1)
    {
        error = "index.json has no schema version";
        return std::nullopt;
    }

    index.generatedAt = timeOr (root["generatedAt"]);
    index.appVersion = root["app"]["version"].toString();

    const auto& companion = root["companion"];
    const auto latestVersion = companion["latestVersion"].toString().trim();
    if (latestVersion.isNotEmpty())
        index.latestRelease = CompanionRelease { latestVersion, companion["releaseUrl"].toString().trim() };

    if (auto* applied = root["applied"].getArray())
        for (auto& id : *applied)
            if (id.isString())
                index.applied.add (id.toString());

    if (auto* projects = root["projects"].getArray())
        for (auto& p : *projects)
            if (p.isObject() && p["id"].toString().isNotEmpty())
                index.projects.push_back (parseProject (p));

    return index;
}

std::optional<LaunchHint> LaunchHint::parse (const juce::String& json)
{
    juce::var root;
    if (juce::JSON::parse (json, root).failed() || ! root.isObject())
        return std::nullopt;

    LaunchHint hint;
    hint.projectId = root["projectId"].toString();
    hint.profileId = root["profileId"].toString();
    hint.filePath = root["filePath"].toString();

    auto launchedAt = protocol::parseTime (root["launchedAt"].toString());
    if (! launchedAt || (hint.projectId.isEmpty() && hint.filePath.isEmpty()))
        return std::nullopt;

    hint.launchedAt = *launchedAt;
    return hint;
}

} // namespace dpm
