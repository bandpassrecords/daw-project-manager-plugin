cmake_minimum_required(VERSION 3.22)

# One version for the plugin, the installers and the release tag: the newest
# entry in changelog/changelog.json. CI fails a tag that doesn't match it, the
# same gate the app has for its own changelog. -DDPM_VERSION=x.y.z overrides.
if(NOT DPM_VERSION)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/changelog/changelog.json" DPM_CHANGELOG)
    string(JSON DPM_VERSION GET "${DPM_CHANGELOG}" releases 0 version)
endif()
if(NOT DPM_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "DPM_VERSION '${DPM_VERSION}' is not a bare x.y.z version")
endif()

# A universal binary is the only kind a macOS user can install without asking
# which Mac they have. Both must be set before project(), which reads them.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(DAWProjectManager VERSION ${DPM_VERSION} LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DPM_BUILD_TESTS "Build the unit tests" ON)
option(DPM_BUILD_STANDALONE "Also build a Standalone app (development only: it has no host transport)" OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# Pinned exactly. Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=<checkout> to build
# against a local copy instead of downloading.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

# Logic with no GUI and no host: compiled into the plugin and into the tests.
set(DPM_CORE_SOURCES
    src/core/BridgePaths.cpp
    src/core/BridgeProtocol.cpp
    src/core/CaptureEngine.cpp
    src/core/CaptureNaming.cpp
    src/core/CapturePolicy.cpp
    src/core/CaptureWriter.cpp
    src/core/BridgeSession.cpp
    src/core/PluginState.cpp
    src/core/Localisation.cpp
    src/core/NotesBook.cpp
    src/core/Outbox.cpp
    src/core/ProjectIndex.cpp
    src/core/ProjectMatcher.cpp
    src/core/TestTransport.cpp
    src/core/TodoBook.cpp
    src/core/Version.cpp)

set(DPM_JUCE_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    DPM_VERSION_STRING="${DPM_VERSION}")

if(MSVC)
    set(DPM_WARNINGS /W4 /permissive-)
else()
    set(DPM_WARNINGS -Wall -Wextra -Wshadow -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------

set(DPM_FORMATS VST3)
if(APPLE)
    list(APPEND DPM_FORMATS AU)
endif()
if(DPM_BUILD_STANDALONE)
    list(APPEND DPM_FORMATS Standalone)
endif()

juce_add_plugin(DAWProjectManager
    PRODUCT_NAME "DAW Project Manager"
    COMPANY_NAME "BandPass Records"
    COMPANY_WEBSITE "https://github.com/bandpassrecords/daw-project-manager"
    BUNDLE_ID com.bandpassrecords.dpm.companion
    VERSION ${DPM_VERSION}
    PLUGIN_MANUFACTURER_CODE Bnpr
    PLUGIN_CODE Dpmc
    FORMATS ${DPM_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    ICON_BIG resources/brand/icon.png
    VST3_CATEGORIES Fx Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(DAWProjectManagerData
    HEADER_NAME DPMBinaryData.h
    NAMESPACE DPMBinaryData
    SOURCES
        resources/brand/logo.svg
        resources/translations/de.txt
        resources/translations/es.txt
        resources/translations/fr.txt
        resources/translations/it.txt
        resources/translations/ja.txt
        resources/translations/pt.txt
        resources/translations/ru.txt
        resources/translations/zh.txt)

set(DPM_PLATFORM_SOURCES src/platform/HostWindowTitle.cpp)
if(APPLE)
    list(APPEND DPM_PLATFORM_SOURCES src/platform/HostWindowTitleMac.mm)
endif()

target_sources(DAWProjectManager PRIVATE
    ${DPM_CORE_SOURCES}
    ${DPM_PLATFORM_SOURCES}
    src/plugin/BridgeClient.cpp
    src/plugin/PluginProcessor.cpp
    src/plugin/Translations.cpp
    src/ui/BrandLookAndFeel.cpp
    src/ui/PluginEditor.cpp
    src/ui/ProjectPicker.cpp
    src/ui/TestPanel.cpp
    src/ui/TodoList.cpp)

target_include_directories(DAWProjectManager PRIVATE src)
target_compile_definitions(DAWProjectManager PUBLIC ${DPM_JUCE_DEFINITIONS}
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_compile_options(DAWProjectManager PRIVATE ${DPM_WARNINGS})

target_link_libraries(DAWProjectManager
    PRIVATE
        DAWProjectManagerData
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(APPLE)
    target_link_libraries(DAWProjectManager PRIVATE "-framework AppKit")
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(DPM_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)

    juce_add_console_app(DAWProjectManagerTests PRODUCT_NAME "DAWProjectManagerTests")
    target_sources(DAWProjectManagerTests PRIVATE
        ${DPM_CORE_SOURCES}
        tests/BridgePathsTests.cpp
        tests/BridgeProtocolTests.cpp
        tests/CaptureNamingTests.cpp
        tests/CapturePolicyTests.cpp
        tests/CaptureEngineTests.cpp
        tests/CaptureWriterTests.cpp
        tests/BridgeSessionTests.cpp
        tests/PluginStateTests.cpp
        tests/OutboxTests.cpp
        tests/PresenceTests.cpp
        tests/ProjectIndexTests.cpp
        tests/ProjectMatcherTests.cpp
        tests/LocalisationTests.cpp
        tests/NotesTests.cpp
        tests/TestTransportTests.cpp
        tests/TodoBookTests.cpp
        tests/VersionTests.cpp)
    target_include_directories(DAWProjectManagerTests PRIVATE src tests)
    target_compile_definitions(DAWProjectManagerTests PRIVATE ${DPM_JUCE_DEFINITIONS}
        DPM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(DAWProjectManagerTests PRIVATE ${DPM_WARNINGS})
    target_link_libraries(DAWProjectManagerTests PRIVATE
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_data_structures
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    enable_testing()
    include(CTest)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(DAWProjectManagerTests)
endif()
