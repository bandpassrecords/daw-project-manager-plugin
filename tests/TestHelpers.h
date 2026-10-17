#pragma once

#include "core/BridgePaths.h"
#include "core/BridgeProtocol.h"
#include "core/ProjectIndex.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <vector>

namespace dpm::test
{

/** A fresh folder under the system temp directory, deleted afterwards. */
struct TempDir
{
    juce::File dir;

    TempDir()
        : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("dpm-companion-tests")
                   .getChildFile (juce::Uuid().toString()))
    {
        dir.createDirectory();
    }

    ~TempDir() { dir.deleteRecursively(); }

    BridgeFolder bridge() const { return { dir.getChildFile ("plugin_bridge") }; }
};

/** A fixed moment, so tests never depend on the clock. 2026-10-18 12:00:00 UTC. */
inline juce::Time fixedTime (int plusSeconds = 0)
{
    return juce::Time ((juce::int64) 1792324800000 + (juce::int64) plusSeconds * 1000);
}

struct TodoSpec
{
    const char* id;
    const char* text;
    bool completed = false;
};

struct ProjectSpec
{
    const char* id;
    const char* profileId;
    const char* name;
    const char* filePath;
    const char* dawType = "";
    std::vector<TodoSpec> todos = {};
};

/** index.json text as the app writes it. */
inline juce::String indexJson (std::initializer_list<ProjectSpec> projects, juce::Time generatedAt = fixedTime())
{
    juce::Array<juce::var> list;
    for (auto& p : projects)
    {
        juce::Array<juce::var> todos;
        for (auto& t : p.todos)
        {
            auto* todo = new juce::DynamicObject();
            todo->setProperty ("id", t.id);
            todo->setProperty ("text", t.text);
            todo->setProperty ("completed", t.completed);
            todo->setProperty ("createdAt", "2026-10-01T09:00:00.000Z");
            todos.add (juce::var (todo));
        }

        auto* project = new juce::DynamicObject();
        project->setProperty ("id", p.id);
        project->setProperty ("profileId", p.profileId);
        project->setProperty ("profileName", juce::String (p.profileId).toUpperCase());
        project->setProperty ("name", p.name);
        project->setProperty ("filePath", p.filePath);
        project->setProperty ("dawType", p.dawType);
        project->setProperty ("todos", todos);
        list.add (juce::var (project));
    }

    auto* app = new juce::DynamicObject();
    app->setProperty ("version", "2.10.0");

    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", 1);
    root->setProperty ("generatedAt", protocol::formatTime (generatedAt));
    root->setProperty ("app", juce::var (app));
    root->setProperty ("projects", list);
    return juce::JSON::toString (juce::var (root));
}

inline ProjectIndex parseIndex (const juce::String& json)
{
    juce::String error;
    auto index = ProjectIndex::parse (json, error);
    INFO (error.toStdString());
    REQUIRE (index.has_value());
    return *index;
}

/** Every file in the outbox, parsed, in the order the app would read them. */
inline std::vector<juce::var> outboxMessages (const BridgeFolder& bridge)
{
    auto files = bridge.outboxDir().findChildFiles (juce::File::findFiles, false, "*.json");
    std::sort (files.begin(), files.end(),
               [] (const juce::File& a, const juce::File& b) { return a.getFileName() < b.getFileName(); });

    std::vector<juce::var> messages;
    for (auto& f : files)
        messages.push_back (juce::JSON::parse (f.loadFileAsString()));
    return messages;
}

} // namespace dpm::test
