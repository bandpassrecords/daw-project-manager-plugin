#include "TestHelpers.h"

#include "core/Outbox.h"

using namespace dpm;

TEST_CASE ("Outbox file names sort in the order messages were written", "[outbox]")
{
    const auto a = outboxFileName (test::fixedTime(), 41, "1A2B3C4D-0000-0000-0000-000000000000");
    const auto b = outboxFileName (test::fixedTime(), 42, "00000000-0000-0000-0000-000000000000");
    const auto c = outboxFileName (test::fixedTime (1), 0, "ffffffff-0000-0000-0000-000000000000");

    CHECK (a == "20261018T120000000Z-000041-1a2b3c4d.json");
    CHECK (a < b);
    CHECK (b < c);
}

TEST_CASE ("A message is written whole, with no temp file left behind", "[outbox]")
{
    test::TempDir tmp;
    Outbox outbox (tmp.dir.getChildFile ("outbox"));

    const auto file = outbox.write (R"({"hello":"world"})", test::fixedTime(), protocol::newId());
    REQUIRE (file.existsAsFile());
    CHECK (file.getParentDirectory() == outbox.directory());
    CHECK (juce::JSON::parse (file.loadFileAsString())["hello"].toString() == "world");
    CHECK (outbox.directory().findChildFiles (juce::File::findFiles, false, "*.tmp").isEmpty());
}

TEST_CASE ("Messages written in the same millisecond keep their order", "[outbox]")
{
    test::TempDir tmp;
    Outbox outbox (tmp.dir.getChildFile ("outbox"));

    for (int i = 0; i < 20; ++i)
        REQUIRE (outbox.write ("{\"n\":" + juce::String (i) + "}", test::fixedTime(), protocol::newId()).existsAsFile());

    const auto messages = test::outboxMessages ({ tmp.dir });
    REQUIRE (messages.size() == 20);
    for (int i = 0; i < 20; ++i)
        CHECK ((int) messages[(size_t) i]["n"] == i);
}

TEST_CASE ("An outbox that can't be created reports failure", "[outbox]")
{
    test::TempDir tmp;
    const auto blocker = tmp.dir.getChildFile ("outbox");
    REQUIRE (blocker.replaceWithText ("a file where the folder should be"));

    Outbox outbox (blocker);
    CHECK (outbox.write ("{}", test::fixedTime(), protocol::newId()) == juce::File());
}
