#include "PluginEditor.h"

#include <algorithm>

namespace dpm
{

// --- LevelMeter ----------------------------------------------------------------

void LevelMeter::push (float peak)
{
    const auto db = juce::Decibels::gainToDecibels (peak, -60.0f);
    const auto target = juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f);
    // Rises at once, falls gently.
    level = target > level ? target : juce::jmax (target, level - 0.04f);
    repaint();
}

void LevelMeter::setRecording (bool isRecording)
{
    if (recording != isRecording)
    {
        recording = isRecording;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (palette::background);
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

    const auto fill = bounds.withWidth (bounds.getWidth() * juce::jlimit (0.0f, 1.0f, level));
    g.setColour (recording ? palette::recording : brand::grey.brighter (0.4f));
    g.fillRoundedRectangle (fill, bounds.getHeight() * 0.5f);
}

// --- CompanionEditor -------------------------------------------------------------

namespace
{
    void style (juce::Label& label, float size, juce::Colour colour, bool bold = false)
    {
        label.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
        label.setColour (juce::Label::textColourId, colour);
        label.setMinimumHorizontalScale (1.0f);
        label.setBorderSize ({});
    }

    void heading (juce::Label& label, const juce::String& text)
    {
        label.setText (text.toUpperCase(), juce::dontSendNotification);
        style (label, 12.0f, palette::textTertiary, true);
    }

    int textWidth (const juce::String& text, const juce::Font& font)
    {
        return (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, text));
    }

    juce::String lengthText (int seconds)
    {
        if (seconds < 60)
            return TRANS ("{n} s").replace ("{n}", juce::String (seconds));
        return TRANS ("{n} min").replace ("{n}", juce::String (seconds / 60));
    }

    constexpr int modeAutomatic = 1, modeManual = 2, modeOff = 3;
}

CompanionEditor::CompanionEditor (CompanionProcessor& p) : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    logo = createLogo();
    title.setText ("DAW Project Manager", juce::dontSendNotification);
    style (title, 16.0f, palette::text, true);
    subtitle.setText ("COMPANION", juce::dontSendNotification);
    style (subtitle, 11.0f, palette::accent, true);
    style (appStatus, 12.0f, palette::textSecondary);
    for (auto* c : { &title, &subtitle, &appStatus })
        addAndMakeVisible (c);

    // Project
    heading (projectHeading, TRANS ("Project"));
    style (projectName, 17.0f, palette::text, true);
    style (projectPath, 12.0f, palette::textTertiary);
    style (relinkHint, 13.0f, palette::warning);
    projectPath.setMinimumHorizontalScale (0.6f);
    changeProjectButton.setButtonText (TRANS ("Change..."));
    changeProjectButton.onClick = [this] { showPicker (true); };
    relinkButton.setButtonText (TRANS ("Link"));
    makePrimary (relinkButton);
    relinkButton.onClick = [this]
    {
        processor.bridge().withSession ([this] (CompanionSession& s)
        {
            if (auto* target = s.index() != nullptr ? s.index()->find (relinkTarget) : nullptr)
                s.linkTo (*target, true);
        });
        processor.bridge().changed();
    };
    for (auto* c : std::initializer_list<juce::Component*> { &projectHeading, &projectName, &projectPath,
                                                            &relinkHint, &changeProjectButton, &relinkButton })
        addAndMakeVisible (c);

    // Capture
    heading (captureHeading, TRANS ("Pre-listen capture"));
    modeLabel.setText (TRANS ("Mode"), juce::dontSendNotification);
    lengthLabel.setText (TRANS ("Length"), juce::dontSendNotification);
    style (modeLabel, 13.0f, palette::textSecondary);
    style (lengthLabel, 13.0f, palette::textSecondary);
    style (captureStatus, 13.0f, palette::textSecondary);

    modeBox.addItem (TRANS ("Automatic"), modeAutomatic);
    modeBox.addItem (TRANS ("Manual"), modeManual);
    modeBox.addItem (TRANS ("Off"), modeOff);
    modeBox.onChange = [this]
    {
        const auto mode = modeBox.getSelectedId() == modeManual ? CaptureMode::manual
                        : modeBox.getSelectedId() == modeOff    ? CaptureMode::off
                                                                : CaptureMode::automatic;
        processor.bridge().withSession ([mode] (CompanionSession& s) { s.setCaptureMode (mode); });
        processor.applyCaptureSettings();
        refreshCaptureStatus();
    };

    for (auto seconds : CaptureSettings::lengthChoices())
        lengthBox.addItem (lengthText (seconds), seconds);
    lengthBox.onChange = [this]
    {
        const auto seconds = lengthBox.getSelectedId();
        processor.bridge().withSession ([seconds] (CompanionSession& s) { s.setCaptureMaxSeconds (seconds); });
        processor.applyCaptureSettings();
    };

    recordButton.setClickingTogglesState (false);
    recordButton.setColour (juce::TextButton::textColourOffId, palette::recording.brighter (0.4f));
    recordButton.onClick = [this]
    {
        auto& engine = processor.capture();
        if (engine.isManualTake())
            engine.pressStop();
        else
            engine.pressRecord();
    };

    for (auto* c : std::initializer_list<juce::Component*> { &captureHeading, &modeLabel, &lengthLabel, &modeBox,
                                                            &lengthBox, &recordButton, &meter, &captureStatus })
        addAndMakeVisible (c);

    // To-dos
    for (auto* tab : { &todosTab, &notesTab })
    {
        tab->setClickingTogglesState (false);
        tab->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        tab->setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        tab->setColour (juce::TextButton::textColourOffId, palette::textTertiary);
        tab->setColour (juce::TextButton::textColourOnId, palette::text);
        addAndMakeVisible (*tab);
    }
    todosTab.setButtonText (TRANS ("To-dos").toUpperCase());
    notesTab.setButtonText (TRANS ("Notes").toUpperCase());
    todosTab.onClick = [this] { showNotes (false); };
    notesTab.onClick = [this] { showNotes (true); };

    notesEditor.setMultiLine (true, true);
    notesEditor.setReturnKeyStartsNewLine (true);
    notesEditor.setScrollbarsShown (true);
    notesEditor.setIndents (10, 8);
    notesEditor.setInputRestrictions (maxNotesLength);
    notesEditor.setTextToShowWhenEmpty (TRANS ("Write notes for this project..."), palette::textTertiary);
    notesEditor.onTextChange = [this] { refreshNotesStatus(); };
    saveNotesButton.setButtonText (TRANS ("Save"));
    makePrimary (saveNotesButton);
    saveNotesButton.onClick = [this] { saveNotes(); };
    reloadNotesButton.setButtonText (TRANS ("Reload"));
    reloadNotesButton.onClick = [this]
    {
        notesLoaded = processor.bridge().withSession ([] (CompanionSession& s) { return s.notes(); });
        notesEditor.setText (notesLoaded, juce::dontSendNotification);
        refreshNotesStatus();
    };
    style (notesStatus, 12.0f, palette::textTertiary);
    for (auto* c : std::initializer_list<juce::Component*> { &notesEditor, &saveNotesButton, &reloadNotesButton, &notesStatus })
        addChildComponent (c);
    todoList.onToggle = [this] (const juce::String& todoId, bool completed)
    {
        processor.bridge().withSession ([&] (CompanionSession& s) { s.setTodoCompleted (todoId, completed); });
        processor.bridge().changed();
    };
    todoInput.setTextToShowWhenEmpty (TRANS ("Add a to-do..."), palette::textTertiary);
    todoInput.setIndents (10, 7);
    todoInput.setInputRestrictions (maxTodoLength);
    todoInput.onReturnKey = [this] { addTodo(); };
    addTodoButton.setButtonText (TRANS ("Add"));
    makePrimary (addTodoButton);
    addTodoButton.onClick = [this] { addTodo(); };
    stampToggle.setButtonText (TRANS ("Stamp with playhead time"));
    stampToggle.onClick = [this]
    {
        const auto on = stampToggle.getToggleState();
        processor.bridge().withSession ([on] (CompanionSession& s) { s.setStampTodosWithTime (on); });
    };
    for (auto* c : std::initializer_list<juce::Component*> { &todoList, &todoInput, &addTodoButton, &stampToggle })
        addAndMakeVisible (c);

    style (footer, 11.0f, palette::textTertiary);
    addAndMakeVisible (footer);

    // Opens the release page in the browser: the app found the release, so
    // the plugin itself never goes on the network.
    updateLink.setFont (juce::FontOptions (11.0f, juce::Font::bold), false, juce::Justification::centredRight);
    updateLink.setColour (juce::HyperlinkButton::textColourId, palette::warning);
    addChildComponent (updateLink);

    picker.rowsFor = [this] (const juce::String& query)
    {
        return processor.bridge().withSession ([&query] (CompanionSession& s)
        {
            return s.index() != nullptr ? buildPickerRows (*s.index(), s.suggestions(), query)
                                        : std::vector<PickerRow> {};
        });
    };
    picker.onPick = [this] (const protocol::ProjectRef& ref)
    {
        processor.bridge().withSession ([&ref] (CompanionSession& s)
        {
            if (auto* project = s.index() != nullptr ? s.index()->find (ref) : nullptr)
                s.linkTo (*project, true);
        });
        showPicker (false);
        processor.bridge().changed();
    };
    picker.onUnlink = [this]
    {
        processor.bridge().withSession ([] (CompanionSession& s) { s.unlink(); });
        showPicker (false);
        processor.bridge().changed();
    };
    picker.onClose = [this] { showPicker (false); };
    addChildComponent (picker);

    processor.bridge().addChangeListener (this);

    setResizable (true, true);
    // The Standalone has no DAW around it: a pretend one goes on top.
    const auto testHeight = processor.isStandalone() ? TestPanel::preferredHeight : 0;
    if (processor.isStandalone())
    {
        testPanel = std::make_unique<TestPanel> (processor);
        addAndMakeVisible (*testPanel);
    }

    setResizeLimits (420, 540 + testHeight, 1000, 1200 + testHeight);
    setSize (480, 660 + testHeight);

    showNotes (false);
    refreshFromSession();
    startTimerHz (20);
}

CompanionEditor::~CompanionEditor()
{
    processor.bridge().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void CompanionEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromSession();
}

void CompanionEditor::refreshFromSession()
{
    struct Snapshot
    {
        bool hasIndex = false;
        bool linked = false;
        protocol::ProjectRef project;
        juce::String profileName;
        juce::String relinkName;
        protocol::ProjectRef relinkRef;
        std::vector<TodoView> todos;
        CompanionState state;
        int undelivered = 0;
        std::optional<CompanionRelease> update;
        juce::String notes;
    };

    const auto snap = processor.bridge().withSession ([] (CompanionSession& s)
    {
        Snapshot out;
        out.hasIndex = s.index() != nullptr;
        out.state = s.state();
        out.project = s.state().project;
        out.linked = out.project.isLinked();
        if (auto* linked = s.linkedProject())
            out.profileName = linked->profileName;
        if (auto* relink = s.relinkSuggestion())
        {
            out.relinkName = relink->name;
            out.relinkRef = relink->ref();
        }
        out.todos = s.todos();
        out.undelivered = s.undeliveredCount();
        out.update = s.availableUpdate();
        out.notes = s.notes();
        return out;
    });

    // The notes editor follows the session unless the user is typing in it.
    if (! notesDirty() && notesEditor.getText() != snap.notes)
    {
        notesEditor.setText (snap.notes, juce::dontSendNotification);
        notesLoaded = snap.notes;
    }
    notesEditor.setEnabled (snap.linked);
    notesEditor.setTextToShowWhenEmpty (snap.linked ? TRANS ("Write notes for this project...")
                                                    : TRANS ("Link a project to see its notes."),
                                        palette::textTertiary);
    refreshNotesStatus();

    appConnected = snap.hasIndex;
    appStatus.setText (appConnected ? TRANS ("App connected") : TRANS ("App not found"), juce::dontSendNotification);

    if (snap.linked)
    {
        projectName.setText (snap.project.name, juce::dontSendNotification);
        juce::String path = snap.project.filePath;
        if (snap.profileName.isNotEmpty())
            path = snap.profileName + detailSeparator + path;
        projectPath.setText (path, juce::dontSendNotification);
        projectPath.setTooltip (snap.project.filePath);
    }
    else
    {
        projectName.setText (TRANS ("Not linked to a project"), juce::dontSendNotification);
        projectPath.setText (TRANS ("Pick the project this DAW session belongs to, so captures and to-dos reach it."),
                             juce::dontSendNotification);
        projectPath.setTooltip ({});
    }

    // Unlinked, choosing a project is the one thing to do: it gets the accent.
    projectLinked = snap.linked;
    changeProjectButton.setButtonText (snap.linked ? TRANS ("Change...") : TRANS ("Choose project"));
    if (snap.linked)
    {
        changeProjectButton.removeColour (juce::TextButton::buttonColourId);
        changeProjectButton.removeColour (juce::TextButton::textColourOffId);
    }
    else
    {
        makePrimary (changeProjectButton);
    }

    relinkTarget = snap.relinkRef;
    relinkHint.setText (snap.relinkName.isEmpty() ? juce::String()
                                                  : TRANS ("This looks like {name}.").replace ("{name}", snap.relinkName),
                        juce::dontSendNotification);
    relinkHint.setVisible (snap.relinkName.isNotEmpty());
    relinkButton.setVisible (snap.relinkName.isNotEmpty());

    modeBox.setSelectedId (snap.state.capture.mode == CaptureMode::manual ? modeManual
                           : snap.state.capture.mode == CaptureMode::off  ? modeOff
                                                                          : modeAutomatic,
                           juce::dontSendNotification);
    lengthBox.setSelectedId (snap.state.capture.maxSeconds, juce::dontSendNotification);
    stampToggle.setToggleState (snap.state.stampTodosWithTime, juce::dontSendNotification);

    const auto open = std::count_if (snap.todos.begin(), snap.todos.end(), [] (const TodoView& t) { return ! t.completed; });
    todosTab.setButtonText (TRANS ("To-dos").toUpperCase() + (open > 0 ? "  " + juce::String (open) : juce::String()));

    todoList.setTodos (snap.todos);
    todoList.setEmptyText (snap.linked ? TRANS ("No to-dos yet.") : TRANS ("Link a project to see its to-dos."));
    todoInput.setEnabled (snap.linked);
    addTodoButton.setEnabled (snap.linked);
    stampToggle.setEnabled (snap.linked);

    juce::String footerText = "v" DPM_VERSION_STRING;
    if (snap.undelivered == 1)
        footerText << detailSeparator << TRANS ("1 change waiting for the app");
    else if (snap.undelivered > 1)
        footerText << detailSeparator << TRANS ("{n} changes waiting for the app").replace ("{n}", juce::String (snap.undelivered));
    footer.setText (footerText, juce::dontSendNotification);

    if (snap.update)
    {
        updateLink.setButtonText (TRANS ("Update available: {version}").replace ("{version}", "v" + snap.update->version));
        updateLink.setURL (juce::URL (snap.update->url));
        updateLink.setTooltip (snap.update->url);
    }
    updateLink.setVisible (snap.update.has_value());

    if (picker.isVisible())
        picker.refresh (snap.hasIndex, snap.linked);

    refreshCaptureStatus();
    resized();
    repaint();
}

void CompanionEditor::refreshCaptureStatus()
{
    auto& engine = processor.capture();
    const auto settings = processor.bridge().withSession ([] (CompanionSession& s) { return s.state(); });
    const auto& mode = settings.capture.mode;
    const auto recording = engine.isRecording();

    // Record only means something in manual mode; elsewhere it isn't shown.
    if (recordButton.isVisible() != (mode == CaptureMode::manual))
    {
        recordButton.setVisible (mode == CaptureMode::manual);
        resized();
    }
    recordButton.setButtonText (engine.isManualTake() ? TRANS ("Stop") : TRANS ("Record"));
    recordButton.setToggleState (engine.isManualTake(), juce::dontSendNotification);
    lengthBox.setEnabled (mode != CaptureMode::off);
    meter.setRecording (recording);

    // The pulsing dot by the heading.
    if (recording || recordingShown)
        repaint (captureCard);
    recordingShown = recording;

    juce::String status;
    auto colour = palette::textSecondary;

    if (recording)
    {
        status = TRANS ("Recording {time} of {max}")
                     .replace ("{time}", formatClock (engine.recordedSeconds()))
                     .replace ("{max}", formatClock (settings.capture.maxSeconds));
        colour = palette::recording.brighter (0.3f);
    }
    else
    {
        const auto report = processor.bridge().lastTake();

        if (report.any && report.verdict != TakeVerdict::keep)
        {
            switch (report.verdict)
            {
                case TakeVerdict::discardTooShort:   status = TRANS ("Last pass not kept: too short"); break;
                case TakeVerdict::discardNotBetter:  status = TRANS ("Last pass not kept: shorter than the current capture"); break;
                case TakeVerdict::discardEnded:      status = TRANS ("Last pass not kept: looped or rendered offline"); break;
                case TakeVerdict::discardSilent:     status = TRANS ("Last pass not kept: silent"); break;
                case TakeVerdict::discardIncomplete: status = TRANS ("Last pass not kept: audio was lost"); break;
                case TakeVerdict::keep:              break;
            }
        }
        else if (settings.lastCapture.exists())
        {
            status = TRANS ("Last capture: {time} ({length})")
                         .replace ("{time}", settings.lastCapture.capturedAt.toString (true, true, false, true))
                         .replace ("{length}", formatClock (settings.lastCapture.seconds));
        }
        else
        {
            status = mode == CaptureMode::automatic ? TRANS ("Captures each playback pass, up to the chosen length.")
                   : mode == CaptureMode::manual    ? TRANS ("Press Record to capture what plays.")
                                                    : TRANS ("Capture is off.");
        }
    }

    captureStatus.setText (status, juce::dontSendNotification);
    captureStatus.setColour (juce::Label::textColourId, colour);
}

void CompanionEditor::timerCallback()
{
    meter.push (processor.capture().takeMeterPeak());
    refreshCaptureStatus();
}

void CompanionEditor::showPicker (bool show)
{
    if (show)
    {
        const auto state = processor.bridge().withSession ([] (CompanionSession& s)
        {
            return std::pair { s.index() != nullptr, s.state().project.isLinked() };
        });
        picker.setVisible (true);
        picker.refresh (state.first, state.second);
        picker.toFront (true);
    }
    else
    {
        picker.setVisible (false);
    }
}

void CompanionEditor::showNotes (bool notes)
{
    showingNotes = notes;
    todosTab.setToggleState (! notes, juce::dontSendNotification);
    notesTab.setToggleState (notes, juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &todoList, &todoInput, &addTodoButton, &stampToggle })
        c->setVisible (! notes);
    for (auto* c : std::initializer_list<juce::Component*> { &notesEditor, &saveNotesButton, &notesStatus })
        c->setVisible (notes);
    refreshNotesStatus();
    repaint (tabsRow);
}

void CompanionEditor::refreshNotesStatus()
{
    const auto [pending, appNotes] = processor.bridge().withSession ([] (CompanionSession& s)
    {
        return std::pair { s.notesPending(), s.notes() };
    });

    // Typing while the app's notes moved on: keep the typing, offer the app's.
    const auto changedInApp = notesDirty() && appNotes != notesLoaded;

    juce::String status;
    if (changedInApp)
        status = TRANS ("The notes changed in the app.");
    else if (pending && ! notesDirty())
        status = TRANS ("Waiting for the app");

    notesStatus.setText (status, juce::dontSendNotification);
    notesStatus.setColour (juce::Label::textColourId, changedInApp ? palette::warning : palette::textTertiary);
    reloadNotesButton.setVisible (showingNotes && changedInApp);
    saveNotesButton.setEnabled (notesDirty());
}

void CompanionEditor::saveNotes()
{
    const auto text = notesEditor.getText();
    const auto saved = processor.bridge().withSession ([&text] (CompanionSession& s) { return s.setNotes (text); });
    notesLoaded = text;
    if (saved)
        processor.bridge().changed();
    refreshNotesStatus();
}

void CompanionEditor::addTodo()
{
    const auto text = todoInput.getText();
    const auto playhead = processor.playheadSeconds();
    const auto added = processor.bridge().withSession ([&] (CompanionSession& s) { return s.addTodo (text, playhead); });

    if (added)
    {
        todoInput.clear();
        processor.bridge().changed();
    }
}

void CompanionEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette::background);
    drawBrandStripe (g, stripe.toFloat());

    if (logo != nullptr)
        logo->drawWithin (g, logoArea.toFloat(), juce::RectanglePlacement::centred, 1.0f);

    // Whether the app is there: a dot in a tinted pill.
    const auto statusColour = appConnected ? palette::connected : palette::warning;
    const auto pill = statusPill.toFloat();
    g.setColour (statusColour.withAlpha (0.14f));
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
    g.setColour (statusColour);
    g.fillEllipse (pill.getX() + 10.0f, pill.getCentreY() - 3.5f, 7.0f, 7.0f);

    drawCard (g, projectCard);
    drawCard (g, captureCard);
    drawCard (g, todoCard);

    // While a take records, a red dot pulses at the end of the heading.
    if (processor.capture().isRecording())
    {
        const auto phase = (float) (juce::Time::getMillisecondCounter() % 1200) / 1200.0f;
        const auto alpha = 0.55f + 0.45f * std::cos (phase * juce::MathConstants<float>::twoPi);
        const auto row = captureHeading.getBounds().toFloat();
        g.setColour (palette::recording.withAlpha (alpha));
        g.fillEllipse (row.getRight() - 10.0f, row.getCentreY() - 5.0f, 10.0f, 10.0f);
    }

    // Tabs: a rule under the row, the brand's orange under the open one.
    g.setColour (palette::divider);
    g.fillRect (tabsRow.withTop (tabsRow.getBottom() - 1));
    const auto& openTab = showingNotes ? notesTab : todosTab;
    g.setColour (palette::accent);
    g.fillRoundedRectangle (openTab.getBounds().withTop (tabsRow.getBottom() - 2).withHeight (2).toFloat(), 1.0f);
}

void CompanionEditor::resized()
{
    auto bounds = getLocalBounds();
    if (testPanel != nullptr)
        testPanel->setBounds (bounds.removeFromTop (TestPanel::preferredHeight));
    stripe = bounds.removeFromTop (4);
    auto area = bounds.reduced (16);

    // Header: the mark, the name, and whether the app is there.
    auto header = area.removeFromTop (36);
    const auto pillWidth = textWidth (appStatus.getText(), appStatus.getFont()) + 34;
    statusPill = header.removeFromRight (pillWidth).withSizeKeepingCentre (pillWidth, 24);
    appStatus.setBounds (statusPill.withTrimmedLeft (24).withTrimmedRight (8));
    header.removeFromRight (8);
    logoArea = header.removeFromLeft (34).withSizeKeepingCentre (34, 34);
    header.removeFromLeft (10);
    header = header.withSizeKeepingCentre (header.getWidth(), 34);
    title.setBounds (header.removeFromTop (20));
    subtitle.setBounds (header);
    area.removeFromTop (12);

    auto footerRow = area.removeFromBottom (16);
    if (updateLink.isVisible())
    {
        const auto linkWidth = textWidth (updateLink.getButtonText(), juce::Font (juce::FontOptions (11.0f, juce::Font::bold))) + 8;
        updateLink.setBounds (footerRow.removeFromRight (linkWidth));
    }
    footer.setBounds (footerRow);
    area.removeFromBottom (6);

    // Project card. Unlinked, the explanation gets two lines.
    const auto hasHint = relinkHint.isVisible();
    projectCard = area.removeFromTop ((projectLinked ? 92 : 110) + (hasHint ? 34 : 0));
    {
        auto c = projectCard.reduced (14, 12);
        const auto buttonWidth = juce::jmax (96, textWidth (changeProjectButton.getButtonText(), juce::Font (juce::FontOptions (15.0f))) + 28);
        changeProjectButton.setBounds (c.getRight() - buttonWidth, c.getY(), buttonWidth, 28);
        projectHeading.setBounds (c.removeFromTop (16).withTrimmedRight (buttonWidth + 8));
        c.removeFromTop (6);
        projectName.setBounds (c.removeFromTop (24).withTrimmedRight (buttonWidth + 8));
        c.removeFromTop (2);
        projectPath.setBounds (c.removeFromTop (projectLinked ? 18 : 34));
        if (hasHint)
        {
            c.removeFromTop (6);
            auto row = c.removeFromTop (28);
            relinkButton.setBounds (row.removeFromRight (80));
            relinkHint.setBounds (row);
        }
    }
    area.removeFromTop (10);

    // Capture card
    captureCard = area.removeFromTop (128);
    {
        auto c = captureCard.reduced (14, 12);
        captureHeading.setBounds (c.removeFromTop (16));
        c.removeFromTop (8);

        auto row = c.removeFromTop (30);
        if (recordButton.isVisible())
        {
            recordButton.setBounds (row.removeFromRight (90));
            row.removeFromRight (10);
        }
        const auto modeLabelWidth = textWidth (modeLabel.getText(), modeLabel.getFont()) + 8;
        const auto lengthLabelWidth = textWidth (lengthLabel.getText(), lengthLabel.getFont()) + 8;
        auto modeArea = row.removeFromLeft (row.getWidth() / 2);
        modeLabel.setBounds (modeArea.removeFromLeft (modeLabelWidth));
        modeBox.setBounds (modeArea.withTrimmedRight (12));
        lengthLabel.setBounds (row.removeFromLeft (lengthLabelWidth));
        lengthBox.setBounds (row);

        c.removeFromTop (14);
        meter.setBounds (c.removeFromTop (6));
        c.removeFromTop (8);
        captureStatus.setBounds (c.removeFromTop (20));
    }
    area.removeFromTop (10);

    // To-do card
    todoCard = area;
    {
        auto c = todoCard.reduced (14, 10);
        tabsRow = c.removeFromTop (30);
        auto tabs = tabsRow;
        for (auto* tab : { &todosTab, &notesTab })
            tab->setBounds (tabs.removeFromLeft (textWidth (tab->getButtonText(), juce::Font (juce::FontOptions (13.0f))) + 28)
                                .withTrimmedBottom (2));
        c.removeFromTop (8);

        auto notesArea = c;
        auto notesRow = notesArea.removeFromBottom (30);
        saveNotesButton.setBounds (notesRow.removeFromRight (80));
        notesRow.removeFromRight (8);
        reloadNotesButton.setBounds (notesRow.removeFromRight (80));
        notesRow.removeFromRight (8);
        notesStatus.setBounds (notesRow);
        notesArea.removeFromBottom (8);
        notesEditor.setBounds (notesArea);

        stampToggle.setBounds (c.removeFromBottom (24));
        c.removeFromBottom (6);
        auto inputRow = c.removeFromBottom (34);
        addTodoButton.setBounds (inputRow.removeFromRight (70));
        inputRow.removeFromRight (8);
        todoInput.setBounds (inputRow);
        c.removeFromBottom (8);
        todoList.setBounds (c);
    }

    picker.setBounds (getLocalBounds());
}

} // namespace dpm
